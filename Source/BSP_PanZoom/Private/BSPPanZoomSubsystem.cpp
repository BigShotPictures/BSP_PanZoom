// Copyright Big Shot Pictures. Based on Loops 2D Pan/Zoom, Copyright 2026 Loops Creative Studio (MIT, see LICENSE).

#include "BSPPanZoomSubsystem.h"
#include "BSPPanZoomOverlay.h"
#include "BSPPanZoomSettings.h"
#include "EditorViewportClient.h"
#include "SEditorViewport.h"
#include "LevelEditorViewport.h"
#include "IAssetViewport.h"
#include "SLevelViewport.h"
#include "Editor.h"
#include "SceneView.h"
#include "GameFramework/Actor.h"
#include "Components/SceneComponent.h"
#include "EditorModeManager.h"
#include "ControlRig.h"
#include "Rigs/RigHierarchy.h"
#include "IControlRigObjectBinding.h"
#include "EditMode/ControlRigEditMode.h"

/*
 * Perspective pan/zoom is applied through FEditorViewportClient::ViewModifiers
 * when Unreal builds each FSceneView. Previously we wrote the viewport's FOV,
 * rotation and location directly, which competed with Sequencer updating those
 * same properties while Allow Cinematic Control was enabled.
 *
 * ModifyView adjusts the evaluated view parameters, leaving the perspective
 * viewport camera properties and camera actor/lens settings under Unreal's
 * control. CalcSceneView is also used for picking and gizmos, so those views
 * receive the same adjustment as rendering; a render-only effect would not.
 *
 * Each calculation starts from that viewport's current camera. Camera movement
 * and cuts therefore no longer need a global last-camera object, Sequencer event
 * subscriptions or a separate follow-camera refresh pass. Removing that manual
 * synchronization and perspective transform restoration accounts for most of
 * the deleted code. The modifier still needs explicit removal on disable and
 * viewport cleanup, and the cached controlling-view FOV needs a neutral refresh.
 *
 * Perspective pan is stored in frame units. When the viewport looks through a
 * camera it is applied as a lens shift (FMinimalViewInfo::OffCenterProjectionOffset
 * on the viewport's controlling view), which slides the frame like Maya's film
 * offset with no perspective change. Free viewports have no such struct, so pan
 * tilts the rendered view instead.
 *
 * Orthographic views retain their transform-based implementation. DOF is still
 * temporarily disabled and restored. Other view modifiers (including camera
 * blends) can conflict through callback ordering.
 */
namespace BSPPanZoomLimits
{
	constexpr float MinZoom = 0.05f;
	constexpr float MaxZoom = 20000.0f;
}

namespace BSPPanZoomView
{
	// FEditorViewportClient keeps the camera view it renders through in protected members.
	// A derived class may form a pointer to a protected member of its base ([class.protected]),
	// and applying that pointer to any FEditorViewportClient is ordinary member access.
	// No engine change, no cast. Never instantiated.
	struct FViewInfoAccess : public FEditorViewportClient
	{
		FViewInfoAccess() = delete;

		// The camera view CalcSceneView builds the projection from, or null in a free viewport.
		static FMinimalViewInfo* GetControllingViewInfo(FEditorViewportClient& Client)
		{
			return Client.*(&FViewInfoAccess::bUseControllingActorViewInfo)
				? &(Client.*(&FViewInfoAccess::ControllingActorViewInfo))
				: nullptr;
		}
	};

	static FMinimalViewInfo* GetLensShiftView(FEditorViewportClient& Client)
	{
		return GetDefault<UBSPPanZoomSettings>()->bLensShiftThroughCamera
			? FViewInfoAccess::GetControllingViewInfo(Client)
			: nullptr;
	}

	// Width / height of the rendered frame: the camera's when it constrains the aspect ratio
	// (letterboxed), else the viewport's.
	static float GetFrameAspectRatio(FEditorViewportClient& Client)
	{
		const FMinimalViewInfo* CameraView = FViewInfoAccess::GetControllingViewInfo(Client);
		if (CameraView && CameraView->bConstrainAspectRatio && CameraView->AspectRatio > 0.0f)
		{
			return CameraView->AspectRatio;
		}
		const FIntPoint Size = Client.Viewport ? Client.Viewport->GetSizeXY() : FIntPoint::ZeroValue;
		return Size.X > 0 && Size.Y > 0 ? static_cast<float>(Size.X) / static_cast<float>(Size.Y) : 16.0f / 9.0f;
	}

	// The rendered frame's size in viewport pixels, smaller than the viewport when letterboxed.
	static FVector2D GetFrameSize(FEditorViewportClient& Client, const FIntPoint& ViewportSize)
	{
		const FVector2D Size(FMath::Max(ViewportSize.X, 1), FMath::Max(ViewportSize.Y, 1));
		const float FrameAspect = GetFrameAspectRatio(Client);
		return FrameAspect > Size.X / Size.Y
			? FVector2D(Size.X, Size.X / FrameAspect)
			: FVector2D(Size.Y * FrameAspect, Size.Y);
	}

	static float GetTanHalfFOV(float FOVDegrees)
	{
		return FMath::Tan(FMath::DegreesToRadians(FMath::Clamp(FOVDegrees, 0.001f, 179.0f) * 0.5f));
	}
}

FBSPPanZoomState& UBSPPanZoomSubsystem::GetState(FEditorViewportClient* ViewportClient)
{
	if (GEditor && !ViewportListChangedHandle.IsValid())
	{
		ViewportListChangedHandle = GEditor->OnViewportClientListChanged().AddUObject(this, &UBSPPanZoomSubsystem::RemoveClosedViewports);
	}
	return ViewportStates.FindOrAdd(ViewportClient);
}

const FBSPPanZoomState* UBSPPanZoomSubsystem::FindState(const FEditorViewportClient* ViewportClient) const
{
	return ViewportStates.Find(const_cast<FEditorViewportClient*>(ViewportClient));
}

bool UBSPPanZoomSubsystem::IsEnabled(const FEditorViewportClient* ViewportClient) const
{
	const FBSPPanZoomState* State = FindState(ViewportClient);
	return State && State->bEnabled;
}

void UBSPPanZoomSubsystem::CaptureBaseIfNeeded(FEditorViewportClient* ViewportClient, FBSPPanZoomState& State)
{
	if (State.bHasBase || !ViewportClient)
	{
		return;
	}

	State.BaseLocation = ViewportClient->GetViewLocation();
	State.BaseRotation = ViewportClient->GetViewRotation();
	State.BaseFOV = ViewportClient->ViewFOV;
	State.BaseOrthoZoom = ViewportClient->GetOrthoZoom();
	State.bHasBase = true;
}

void UBSPPanZoomSubsystem::ModifyView(FEditorViewportViewModifierParams& Params)
{
	FEditorViewportClient* ViewportClient = Params.ViewportClient;
	FBSPPanZoomState* State = ViewportStates.Find(ViewportClient);
	if (!State || !ViewportClient || !ViewportClient->IsPerspective())
	{
		return;
	}

	// CalcSceneView caches the modified FOV in ControllingActorViewInfo. Start
	// from ViewFOV on EVERY call so picking/repeated draws cannot compound zoom.
	// This property is maintained by the editor/Sequencer and is never written here.
	Params.ViewInfo.FOV = ViewportClient->ViewFOV;
	State->BaseLocation = Params.ViewInfo.Location;
	State->BaseRotation = Params.ViewInfo.Rotation;
	State->BaseFOV = Params.ViewInfo.FOV;
	if (!State->bEnabled && !State->bAnimControlLockEnabled)
	{
		return;
	}

	if (State->bAnimControlLockEnabled)
	{
		FTransform ControlTransform;
		if (GetSelectedControlWorldTransform(ControlTransform))
		{
			UpdateAnimControlLockPan(ViewportClient, *State, ControlTransform.GetLocation());
		}
	}

	const float SafeZoom = FMath::Max(State->Zoom, 0.01f);
	const float TanHalfFOV = BSPPanZoomView::GetTanHalfFOV(State->BaseFOV);
	Params.ViewInfo.FOV = FMath::RadiansToDegrees(2.0f * FMath::Atan(TanHalfFOV / SafeZoom));

	if (FMinimalViewInfo* CameraView = BSPPanZoomView::GetLensShiftView(*ViewportClient))
	{
		// CalcSceneView builds the projection from this struct right after the broadcast.
		// The offset is in the zoomed view's NDC, where one unzoomed half-frame spans Zoom
		// half-frames. Unreal refills the struct from the camera every tick, so it is
		// assigned on every call, never accumulated.
		CameraView->OffCenterProjectionOffset = State->PanOffset * SafeZoom;
	}
	else
	{
		// Turn the view so the panned frame point lands at the centre: exact per axis,
		// approximate when both are combined.
		const float TanHalfFOVY = TanHalfFOV / BSPPanZoomView::GetFrameAspectRatio(*ViewportClient);
		FRotator NewRotation = State->BaseRotation;
		NewRotation.Yaw = FRotator::NormalizeAxis(NewRotation.Yaw + FMath::RadiansToDegrees(FMath::Atan(State->PanOffset.X * TanHalfFOV)));
		NewRotation.Pitch = FMath::Clamp(NewRotation.Pitch + FMath::RadiansToDegrees(FMath::Atan(State->PanOffset.Y * TanHalfFOVY)), -89.9f, 89.9f);
		Params.ViewInfo.Rotation = NewRotation;
	}
}

void UBSPPanZoomSubsystem::ApplyToCamera(FEditorViewportClient* ViewportClient, FBSPPanZoomState& State)
{
	if (!ViewportClient || !State.bHasBase)
	{
		return;
	}

	if (!ViewportClient->IsPerspective())
	{
		const float SafeZoom = FMath::Max(State.Zoom, 0.01f);
		const FRotationMatrix RotMatrix(State.BaseRotation);
		const FVector Right = RotMatrix.GetScaledAxis(EAxis::Y);
		const FVector Up = RotMatrix.GetScaledAxis(EAxis::Z);
		const FVector NewLocation = State.BaseLocation + Right * State.PanOffset.X + Up * State.PanOffset.Y;
		ViewportClient->SetViewLocation(NewLocation);
		ViewportClient->SetOrthoZoom(State.BaseOrthoZoom / SafeZoom);
	}
	ViewportClient->EngineShowFlags.SetDepthOfField(false);
	ViewportClient->Invalidate();
}

void UBSPPanZoomSubsystem::RestoreCamera(FEditorViewportClient* ViewportClient, FBSPPanZoomState& State)
{
	if (!ViewportClient || !State.bHasBase || !State.ViewModifierHandle.IsValid())
	{
		return;
	}

	ViewportClient->EngineShowFlags.SetDepthOfField(State.bWasDepthOfFieldEnabled);

	if (!ViewportClient->IsPerspective())
	{
		ViewportClient->SetViewLocation(State.BaseLocation);
		ViewportClient->SetOrthoZoom(State.BaseOrthoZoom);
	}
	else if (ViewportClient->Viewport)
	{
		// Flush the controlling actor's cached FOV with a neutral modifier before
		// detaching it, including when playback/realtime is paused.
		FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(
			ViewportClient->Viewport, ViewportClient->GetScene(), ViewportClient->EngineShowFlags));
		ViewportClient->CalcSceneView(&ViewFamily);
	}
	ViewportClient->ViewModifiers.Remove(State.ViewModifierHandle);
	State.ViewModifierHandle.Reset();
	ViewportClient->Invalidate();
}

void UBSPPanZoomSubsystem::Deinitialize()
{
	if (GEditor)
	{
		GEditor->OnViewportClientListChanged().Remove(ViewportListChangedHandle);
		ViewportListChangedHandle.Reset();
		for (TPair<FEditorViewportClient*, FBSPPanZoomState>& Pair : ViewportStates)
		{
			if (GEditor->GetAllViewportClients().Contains(Pair.Key))
			{
				Pair.Value.bEnabled = false;
				Pair.Value.bAnimControlLockEnabled = false;
				RestoreCamera(Pair.Key, Pair.Value);
				RemoveOverlayIfNeeded(Pair.Key, Pair.Value);
			}
		}
	}
	ViewportStates.Empty();
	Super::Deinitialize();
}

void UBSPPanZoomSubsystem::SetEnabled(FEditorViewportClient* ViewportClient, bool bEnabled)
{
	if (!ViewportClient)
	{
		return;
	}

	FBSPPanZoomState& State = GetState(ViewportClient);
	if (State.bEnabled == bEnabled)
	{
		return;
	}

	State.bEnabled = bEnabled;
	if (bEnabled)
	{
		CaptureBaseIfNeeded(ViewportClient, State);
		// Save DOF on each off-to-on transition even if a base already exists.
		// ApplyToCamera disables it; RestoreCamera restores this saved setting.
		State.bWasDepthOfFieldEnabled = ViewportClient->EngineShowFlags.DepthOfField;
		State.ViewModifierHandle = ViewportClient->ViewModifiers.AddUObject(this, &UBSPPanZoomSubsystem::ModifyView);

		RefreshOverlayPresence(ViewportClient, State);
		ApplyToCamera(ViewportClient, State);
	}
	else
	{
		if (State.bAnimControlLockEnabled)
		{
			DisableAnimControlLock(ViewportClient, State);
		}

		RestoreCamera(ViewportClient, State);
		RemoveOverlayIfNeeded(ViewportClient, State);
		State.bHasBase = false;
	}
}

void UBSPPanZoomSubsystem::AddOverlayIfNeeded(FEditorViewportClient* ViewportClient, FBSPPanZoomState& State)
{
	if (State.OverlayWidget.IsValid() || !ViewportClient || !ViewportClient->IsLevelEditorClient())
	{
		return;
	}
	TSharedPtr<SEditorViewport> EditorViewportWidget = ViewportClient->GetEditorViewportWidget();
	TSharedPtr<SLevelViewport> LevelViewportWidget = StaticCastSharedPtr<SLevelViewport>(EditorViewportWidget);
	if (!LevelViewportWidget.IsValid())
	{
		return;
	}
	TSharedRef<SBSPPanZoomOverlay> Overlay = SNew(SBSPPanZoomOverlay, ViewportClient);
	LevelViewportWidget->AddOverlayWidget(Overlay);
	State.OverlayWidget = Overlay;
}

void UBSPPanZoomSubsystem::RemoveOverlayIfNeeded(FEditorViewportClient* ViewportClient, FBSPPanZoomState& State)
{
	if (!State.OverlayWidget.IsValid()){return;}
	if (ViewportClient && ViewportClient->IsLevelEditorClient())
	{
		TSharedPtr<SEditorViewport> EditorViewportWidget = ViewportClient->GetEditorViewportWidget();
		TSharedPtr<SLevelViewport> LevelViewportWidget = StaticCastSharedPtr<SLevelViewport>(EditorViewportWidget);
		if (LevelViewportWidget.IsValid())
		{
			LevelViewportWidget->RemoveOverlayWidget(State.OverlayWidget.ToSharedRef());
		}
	}

	State.OverlayWidget.Reset();
}

void UBSPPanZoomSubsystem::RefreshOverlayPresence(FEditorViewportClient* ViewportClient, FBSPPanZoomState& State)
{
	if (State.bEnabled || State.bAnimControlLockEnabled)
	{
		AddOverlayIfNeeded(ViewportClient, State);
	}
	else
	{
		RemoveOverlayIfNeeded(ViewportClient, State);
	}
}

bool UBSPPanZoomSubsystem::GetOverlayInfo(const FEditorViewportClient* ViewportClient, float& OutZoomPercent, FVector2D& OutCropSize, FVector2D& OutCropCenterOffset, bool& OutIsAnimControlLockActive) const
{
	const FBSPPanZoomState* State = FindState(ViewportClient);
	if (!State || !State->bHasBase || !ViewportClient || (!State->bEnabled && !State->bAnimControlLockEnabled))
	{
		return false;
	}

	OutZoomPercent = State->Zoom * 100.0f;

	const float CropFraction = FMath::Clamp(1.0f / FMath::Max(State->Zoom, 0.01f), 0.01f, 1.0f);
	OutCropSize = FVector2D(CropFraction, CropFraction);

	// Perspective pan is already in frame units (1 = half a frame). Ortho pan is world units.
	const float ReferenceScale = ViewportClient->IsPerspective()
		? 2.0f
		: FMath::Max(State->BaseOrthoZoom, 1.0f);
	OutCropCenterOffset = FVector2D(
		FMath::Clamp(State->PanOffset.X / ReferenceScale, -0.5f, 0.5f),
		FMath::Clamp(-State->PanOffset.Y / ReferenceScale, -0.5f, 0.5f)
	);

	OutIsAnimControlLockActive = State->bAnimControlLockEnabled;

	return true;
}

void UBSPPanZoomSubsystem::ToggleEnabled(FEditorViewportClient* ViewportClient)
{
	SetEnabled(ViewportClient, !IsEnabled(ViewportClient));
}

void UBSPPanZoomSubsystem::Pan(FEditorViewportClient* ViewportClient, const FVector2D& ScreenDelta, const FIntPoint& ViewportSize)
{
	if (!ViewportClient || ViewportSize.X <= 0 || ViewportSize.Y <= 0)
	{
		return;
	}

	FBSPPanZoomState& State = GetState(ViewportClient);
	CaptureBaseIfNeeded(ViewportClient, State);
	const float SafeZoom = FMath::Max(State.Zoom, 0.01f);

	if (ViewportClient->IsPerspective())
	{
		// The picture follows the cursor: the visible frame spans 2/Zoom unzoomed
		// half-frames, so a drag across all of it moves the pan by that much. Use the
		// letterboxed frame, not the whole viewport, when the camera constrains aspect.
		const FVector2D FrameSize = BSPPanZoomView::GetFrameSize(*ViewportClient, ViewportSize);
		State.PanOffset.X -= 2.0f * ScreenDelta.X / FrameSize.X / SafeZoom;
		State.PanOffset.Y += 2.0f * ScreenDelta.Y / FrameSize.Y / SafeZoom;
	}
	else
	{
		const float OrthoWidth = FMath::Max(State.BaseOrthoZoom / SafeZoom, 1.0f);
		const float UnitsPerPixel = OrthoWidth / static_cast<float>(ViewportSize.X);
		State.PanOffset.X -= ScreenDelta.X * UnitsPerPixel;
		State.PanOffset.Y -= ScreenDelta.Y * UnitsPerPixel;
	}

	if (State.bEnabled)
	{
		ApplyToCamera(ViewportClient, State);
	}
}

void UBSPPanZoomSubsystem::Zoom(FEditorViewportClient* ViewportClient, float DeltaZoom)
{
	if (!ViewportClient)
	{
		return;
	}

	FBSPPanZoomState& State = GetState(ViewportClient);
	CaptureBaseIfNeeded(ViewportClient, State);

	const float ZoomFactor = FMath::Exp(DeltaZoom);
	State.Zoom = FMath::Clamp(State.Zoom * ZoomFactor, BSPPanZoomLimits::MinZoom, BSPPanZoomLimits::MaxZoom);

	if (State.bEnabled)
	{
		ApplyToCamera(ViewportClient, State);
	}
}

void UBSPPanZoomSubsystem::ToggleZoomTo100Percent(FEditorViewportClient* ViewportClient)
{
	if (!ViewportClient)
	{
		return;
	}

	FBSPPanZoomState& State = GetState(ViewportClient);
	CaptureBaseIfNeeded(ViewportClient, State);

	constexpr float HundredPercentTolerance = 0.001f;
	if (FMath::IsNearlyEqual(State.Zoom, 1.0f, HundredPercentTolerance))
	{
		State.Zoom = State.PreToggleZoom;
		State.PanOffset = State.PreTogglePanOffset;

		if (State.bPreToggleAnimControlLockEnabled && !State.bAnimControlLockEnabled)
		{
			EnableAnimControlLock(ViewportClient, State);
		}
		State.bPreToggleAnimControlLockEnabled = false;
	}
	else
	{
		State.PreToggleZoom = State.Zoom;
		State.PreTogglePanOffset = State.PanOffset;
		State.Zoom = 1.0f;
		State.PanOffset = FVector2D::ZeroVector;

		State.bPreToggleAnimControlLockEnabled = State.bAnimControlLockEnabled;
		if (State.bAnimControlLockEnabled)
		{
			DisableAnimControlLock(ViewportClient, State);
		}
	}

	if (State.bEnabled)
	{
		ApplyToCamera(ViewportClient, State);
	}
}


// TODO : Extract ControlRig function to BSPPanZoomControlRig
bool UBSPPanZoomSubsystem::GetSelectedControlWorldTransform(FTransform& OutTransform) const
{
	FControlRigEditMode* ControlRigEditMode = static_cast<FControlRigEditMode*>(
		GLevelEditorModeTools().GetActiveMode(FControlRigEditMode::ModeName));
	if (!ControlRigEditMode)
	{
		return false;
	}

	TMap<UControlRig*, TArray<FRigElementKey>> SelectedControls;
	ControlRigEditMode->GetAllSelectedControls(SelectedControls);

	for (const TPair<UControlRig*, TArray<FRigElementKey>>& Pair : SelectedControls)
	{
		UControlRig* ControlRig = Pair.Key;
		if (!ControlRig || Pair.Value.Num() == 0)
		{
			continue;
		}

		URigHierarchy* Hierarchy = ControlRig->GetHierarchy();
		if (!Hierarchy)
		{
			continue;
		}

		FTransform ControlTransform = Hierarchy->GetGlobalTransform(Pair.Value[0]);

		if (TSharedPtr<IControlRigObjectBinding> Binding = ControlRig->GetObjectBinding())
		{
			if (USceneComponent* BoundComponent = Cast<USceneComponent>(Binding->GetBoundObject()))
			{
				ControlTransform = ControlTransform * BoundComponent->GetComponentTransform();
			}
		}

		OutTransform = ControlTransform;
		return true;
	}

	return false;
}

void UBSPPanZoomSubsystem::UpdateAnimControlLockPan(FEditorViewportClient* ViewportClient, FBSPPanZoomState& State, const FVector& ControlWorldLocation)
{
	if (!ViewportClient || !State.bHasBase)
	{
		return;
	}

	// X forward, Y right, Z up, relative to the unpanned view.
	const FVector Local = State.BaseRotation.UnrotateVector(ControlWorldLocation - State.BaseLocation);

	if (!ViewportClient->IsPerspective())
	{
		State.PanOffset = FVector2D(Local.Y, Local.Z);
		return;
	}
	if (Local.X <= KINDA_SMALL_NUMBER)
	{
		// Behind the camera: keep the last aim.
		return;
	}

	// Put the control's unzoomed frame position at the centre. A lens shift needs its
	// projected position; the tilt fallback needs the pitch measured after the yaw.
	const float TanHalfFOV = BSPPanZoomView::GetTanHalfFOV(State.BaseFOV);
	const float TanHalfFOVY = TanHalfFOV / BSPPanZoomView::GetFrameAspectRatio(*ViewportClient);
	const float PitchDistance = BSPPanZoomView::GetLensShiftView(*ViewportClient)
		? Local.X
		: FVector2D(Local.X, Local.Y).Size();
	State.PanOffset.X = Local.Y / Local.X / TanHalfFOV;
	State.PanOffset.Y = Local.Z / PitchDistance / TanHalfFOVY;
}

void UBSPPanZoomSubsystem::ToggleAnimControlLock(FEditorViewportClient* ViewportClient)
{
	if (!ViewportClient)
	{
		return;
	}

	FBSPPanZoomState& State = GetState(ViewportClient);

	if (State.bAnimControlLockEnabled)
	{
		DisableAnimControlLock(ViewportClient, State);
	}
	else
	{
		EnableAnimControlLock(ViewportClient, State);
	}
}

bool UBSPPanZoomSubsystem::EnableAnimControlLock(FEditorViewportClient* ViewportClient, FBSPPanZoomState& State)
{
	if (!ViewportClient)
	{
		return false;
	}

	FTransform ControlWorldTransform;
	if (!GetSelectedControlWorldTransform(ControlWorldTransform))
	{
		// Nothing selected: the shortcut/restore is a no-op.
		return false;
	}

	SetEnabled(ViewportClient, true);
	State.bAnimControlLockEnabled = true;
	UpdateAnimControlLockPan(ViewportClient, State, ControlWorldTransform.GetLocation());
	RefreshOverlayPresence(ViewportClient, State);
	ApplyToCamera(ViewportClient, State);
	return true;
}

void UBSPPanZoomSubsystem::DisableAnimControlLock(FEditorViewportClient* ViewportClient, FBSPPanZoomState& State)
{
	// Unlock: leave the camera exactly where it currently sits.
	State.bAnimControlLockEnabled = false;
	RefreshOverlayPresence(ViewportClient, State);
}

void UBSPPanZoomSubsystem::RemoveClosedViewports()
{
	// Viewport clients are not UObjects. Drop closed viewports before dereferencing
	// their keys (for example after changing the editor viewport layout).
	for (auto It = ViewportStates.CreateIterator(); It; ++It)
	{
		if (!GEditor || !GEditor->GetAllViewportClients().Contains(It.Key()))
		{
			It.RemoveCurrent();
		}
	}
}

void UBSPPanZoomSubsystem::TickAllAnimControlLocks()
{
	bool bAnyLockActive = false;
	for (const TPair<FEditorViewportClient*, FBSPPanZoomState>& Pair : ViewportStates)
	{
		if (Pair.Value.bAnimControlLockEnabled)
		{
			bAnyLockActive = true;
			break;
		}
	}
	if (!bAnyLockActive)
	{
		return;
	}

	FTransform ControlWorldTransform;
	if (!GetSelectedControlWorldTransform(ControlWorldTransform))
	{
		// Selection lost: freeze the camera(s) at their last known aim rather than moving them.
		return;
	}

	for (TPair<FEditorViewportClient*, FBSPPanZoomState>& Pair : ViewportStates)
	{
		FEditorViewportClient* ViewportClient = Pair.Key;
		FBSPPanZoomState& State = Pair.Value;
		if (!ViewportClient || !State.bAnimControlLockEnabled)
		{
			continue;
		}

		UpdateAnimControlLockPan(ViewportClient, State, ControlWorldTransform.GetLocation());
		ApplyToCamera(ViewportClient, State);
	}
}

void UBSPPanZoomSubsystem::Reset(FEditorViewportClient* ViewportClient)
{
	if (!ViewportClient){return;}

	FBSPPanZoomState& State = GetState(ViewportClient);
	State.PanOffset = FVector2D::ZeroVector;
	State.Zoom = 1.0f;

	if (State.bEnabled){ApplyToCamera(ViewportClient, State);}
}

float UBSPPanZoomSubsystem::GetZoom(const FEditorViewportClient* ViewportClient) const
{
	const FBSPPanZoomState* State = FindState(ViewportClient);
	return State ? State->Zoom : 1.0f;
}

void UBSPPanZoomSubsystem::SetZoom(FEditorViewportClient* ViewportClient, float NewZoom)
{
	if (!ViewportClient){return;}

	SetEnabled(ViewportClient, true);
	FBSPPanZoomState& State = GetState(ViewportClient);
	State.Zoom = FMath::Clamp(NewZoom, BSPPanZoomLimits::MinZoom, BSPPanZoomLimits::MaxZoom);
	ApplyToCamera(ViewportClient, State);
}

FVector2D UBSPPanZoomSubsystem::GetPan(const FEditorViewportClient* ViewportClient) const
{
	const FBSPPanZoomState* State = FindState(ViewportClient);
	return State ? State->PanOffset : FVector2D::ZeroVector;
}

void UBSPPanZoomSubsystem::SetPan(FEditorViewportClient* ViewportClient, const FVector2D& NewPan)
{
	if (!ViewportClient){return;}

	SetEnabled(ViewportClient, true);
	FBSPPanZoomState& State = GetState(ViewportClient);
	State.PanOffset = NewPan;
	ApplyToCamera(ViewportClient, State);
}

FEditorViewportClient* UBSPPanZoomSubsystem::FindTargetViewportClient()
{
	if (!GEditor){return nullptr;}

	for (FEditorViewportClient* Candidate : GEditor->GetAllViewportClients())
	{
		const TSharedPtr<SEditorViewport> Widget = Candidate ? Candidate->GetEditorViewportWidget() : nullptr;
		if (Widget.IsValid() && Widget->IsHovered())
		{
			return Candidate;
		}
	}
	if (FViewport* ActiveViewport = GEditor->GetActiveViewport())
	{
		for (FEditorViewportClient* Candidate : GEditor->GetAllViewportClients())
		{
			if (Candidate && Candidate->Viewport == ActiveViewport)
			{
				return Candidate;
			}
		}
	}
	return GCurrentLevelEditingViewportClient;
}

bool UBSPPanZoomSubsystem::IsLiveViewportClient(const FEditorViewportClient* ViewportClient)
{
	return ViewportClient && GEditor && GEditor->GetAllViewportClients().Contains(const_cast<FEditorViewportClient*>(ViewportClient));
}
