// Copyright Big Shot Pictures. Based on Loops 2D Pan/Zoom, Copyright 2026 Loops Creative Studio (MIT, see LICENSE).
#pragma once
#include "Engine/DeveloperSettings.h"
#include "BSPPanZoomSettings.generated.h"

UCLASS(Config=EditorPerProjectUserSettings, meta=(DisplayName="BSP 2D Pan/Zoom"))
class BSP_PANZOOM_API UBSPPanZoomSettings : public UDeveloperSettings
{
	GENERATED_BODY()
public:
	virtual FName GetContainerName() const override { return TEXT("Editor"); }
	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }
	// Outside Animation Mode, leave the toggle/drag and reset keys to Unreal.
	// This is so our \ hotkey won't conflict with the place Colour Calibrator tool.
	// The toolbar and existing numpad shortcuts remain available in all modes.
	UPROPERTY(Config, EditAnywhere, Category="Shortcuts")
	bool bAnimationModeOnly = true;
	// When the viewport looks through a camera (pilot, lock, Sequencer camera cuts), pan
	// slides the frame with an off-centre projection, like Maya's film offset, instead of
	// tilting the view. Free viewports always tilt.
	UPROPERTY(Config, EditAnywhere, Category="View", meta=(DisplayName="Lens Shift Pan Through Cameras"))
	bool bLensShiftThroughCamera = true;
};
