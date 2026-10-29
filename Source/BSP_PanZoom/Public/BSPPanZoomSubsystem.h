// Copyright Big Shot Pictures. Based on Loops 2D Pan/Zoom, Copyright 2026 Loops Creative Studio (MIT, see LICENSE).

#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "BSPPanZoomSubsystem.generated.h"

class FEditorViewportClient;
struct FEditorViewportViewModifierParams;
class SWidget;


USTRUCT()
struct FBSPPanZoomState
{
	GENERATED_BODY()

	// Perspective: fraction of the unzoomed frame. X = +1 brings the frame's right edge to the
	// centre, Y = +1 its top edge. Orthographic: world units along the view's right/up axes.
	UPROPERTY()
	FVector2D PanOffset = FVector2D::ZeroVector;
	UPROPERTY()
	float Zoom = 1.0f;
	UPROPERTY()
	float PreToggleZoom = 1.0f;
	UPROPERTY()
	FVector2D PreTogglePanOffset = FVector2D::ZeroVector;
	UPROPERTY()
	bool bEnabled = false;
	UPROPERTY()
	bool bHasBase = false;
	UPROPERTY()
	FVector BaseLocation = FVector::ZeroVector;
	UPROPERTY()
	FRotator BaseRotation = FRotator::ZeroRotator;
	UPROPERTY()
	float BaseFOV = 90.0f;
	UPROPERTY()
	float BaseOrthoZoom = 0.0f;
	UPROPERTY()
	bool bWasDepthOfFieldEnabled = true;
	UPROPERTY()
	bool bAnimControlLockEnabled = false;
	UPROPERTY()
	bool bPreToggleAnimControlLockEnabled = false;

	TSharedPtr<SWidget> OverlayWidget;
	FDelegateHandle ViewModifierHandle;
};

// Drives 2D Pan/Zoom per-viewport, keyed by the FEditorViewportClient pointer.
UCLASS()
class BSP_PANZOOM_API UBSPPanZoomSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

	public:
		virtual void Deinitialize() override;
		bool IsEnabled(const FEditorViewportClient* ViewportClient) const;
		void SetEnabled(FEditorViewportClient* ViewportClient, bool bEnabled);
		void ToggleEnabled(FEditorViewportClient* ViewportClient);
		void Pan(FEditorViewportClient* ViewportClient, const FVector2D& ScreenDelta, const FIntPoint& ViewportSize);
		void Zoom(FEditorViewportClient* ViewportClient, float DeltaZoom);
		void Reset(FEditorViewportClient* ViewportClient);
		void ToggleZoomTo100Percent(FEditorViewportClient* ViewportClient);
		bool GetOverlayInfo(const FEditorViewportClient* ViewportClient, float& OutZoomPercent, FVector2D& OutCropSize, FVector2D& OutCropCenterOffset, bool& OutIsAnimControlLockActive) const;
		void ToggleAnimControlLock(FEditorViewportClient* ViewportClient);
		void TickAllAnimControlLocks();

		// Absolute values, for the toolbar menu and UBSPPanZoomLibrary. Setting either enables pan/zoom.
		float GetZoom(const FEditorViewportClient* ViewportClient) const;
		void SetZoom(FEditorViewportClient* ViewportClient, float NewZoom);
		FVector2D GetPan(const FEditorViewportClient* ViewportClient) const;
		void SetPan(FEditorViewportClient* ViewportClient, const FVector2D& NewPan);

		// The viewport under the mouse, else the active one. Null if there is none.
		static FEditorViewportClient* FindTargetViewportClient();
		static bool IsLiveViewportClient(const FEditorViewportClient* ViewportClient);

	private:
		FBSPPanZoomState& GetState(FEditorViewportClient* ViewportClient);
		const FBSPPanZoomState* FindState(const FEditorViewportClient* ViewportClient) const;

		void CaptureBaseIfNeeded(FEditorViewportClient* ViewportClient, FBSPPanZoomState& State);
		void ApplyToCamera(FEditorViewportClient* ViewportClient, FBSPPanZoomState& State);
		void RestoreCamera(FEditorViewportClient* ViewportClient, FBSPPanZoomState& State);

		void AddOverlayIfNeeded(FEditorViewportClient* ViewportClient, FBSPPanZoomState& State);
		void RemoveOverlayIfNeeded(FEditorViewportClient* ViewportClient, FBSPPanZoomState& State);
		
		void RefreshOverlayPresence(FEditorViewportClient* ViewportClient, FBSPPanZoomState& State);
		// Focus on Control Rig
		bool GetSelectedControlWorldTransform(FTransform& OutTransform) const;
		void UpdateAnimControlLockPan(FEditorViewportClient* ViewportClient, FBSPPanZoomState& State, const FVector& ControlWorldLocation);
		bool EnableAnimControlLock(FEditorViewportClient* ViewportClient, FBSPPanZoomState& State);
		void DisableAnimControlLock(FEditorViewportClient* ViewportClient, FBSPPanZoomState& State);

		TMap<FEditorViewportClient*, FBSPPanZoomState> ViewportStates;
		FDelegateHandle ViewportListChangedHandle;
		void RemoveClosedViewports();
		void ModifyView(FEditorViewportViewModifierParams& Params);
};
