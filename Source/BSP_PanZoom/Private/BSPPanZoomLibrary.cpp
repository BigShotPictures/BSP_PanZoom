// Copyright Big Shot Pictures. Based on Loops 2D Pan/Zoom, Copyright 2026 Loops Creative Studio (MIT, see LICENSE).

#include "BSPPanZoomLibrary.h"
#include "BSPPanZoomSubsystem.h"
#include "Editor.h"

namespace BSPPanZoomLibrary
{
	static UBSPPanZoomSubsystem* GetSubsystem()
	{
		return GEditor ? GEditor->GetEditorSubsystem<UBSPPanZoomSubsystem>() : nullptr;
	}
}

void UBSPPanZoomLibrary::SetEnabled(bool bEnabled)
{
	if (UBSPPanZoomSubsystem* Subsystem = BSPPanZoomLibrary::GetSubsystem())
	{
		Subsystem->SetEnabled(UBSPPanZoomSubsystem::FindTargetViewportClient(), bEnabled);
	}
}

bool UBSPPanZoomLibrary::IsEnabled()
{
	const UBSPPanZoomSubsystem* Subsystem = BSPPanZoomLibrary::GetSubsystem();
	return Subsystem && Subsystem->IsEnabled(UBSPPanZoomSubsystem::FindTargetViewportClient());
}

void UBSPPanZoomLibrary::SetZoom(float Zoom)
{
	if (UBSPPanZoomSubsystem* Subsystem = BSPPanZoomLibrary::GetSubsystem())
	{
		Subsystem->SetZoom(UBSPPanZoomSubsystem::FindTargetViewportClient(), Zoom);
	}
}

float UBSPPanZoomLibrary::GetZoom()
{
	const UBSPPanZoomSubsystem* Subsystem = BSPPanZoomLibrary::GetSubsystem();
	return Subsystem ? Subsystem->GetZoom(UBSPPanZoomSubsystem::FindTargetViewportClient()) : 1.0f;
}

void UBSPPanZoomLibrary::SetPan(FVector2D Pan)
{
	if (UBSPPanZoomSubsystem* Subsystem = BSPPanZoomLibrary::GetSubsystem())
	{
		Subsystem->SetPan(UBSPPanZoomSubsystem::FindTargetViewportClient(), Pan);
	}
}

FVector2D UBSPPanZoomLibrary::GetPan()
{
	const UBSPPanZoomSubsystem* Subsystem = BSPPanZoomLibrary::GetSubsystem();
	return Subsystem ? Subsystem->GetPan(UBSPPanZoomSubsystem::FindTargetViewportClient()) : FVector2D::ZeroVector;
}

void UBSPPanZoomLibrary::Reset()
{
	if (UBSPPanZoomSubsystem* Subsystem = BSPPanZoomLibrary::GetSubsystem())
	{
		Subsystem->Reset(UBSPPanZoomSubsystem::FindTargetViewportClient());
	}
}
