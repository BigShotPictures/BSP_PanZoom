// Copyright Big Shot Pictures. Based on Loops 2D Pan/Zoom, Copyright 2026 Loops Creative Studio (MIT, see LICENSE).
#pragma once
#include "Framework/Commands/Commands.h"
#include "Styling/AppStyle.h"

class FBSPPanZoomCommands : public TCommands<FBSPPanZoomCommands>
{
public:
	FBSPPanZoomCommands() : TCommands(TEXT("BSPPanZoom"), NSLOCTEXT("BSPPanZoom", "Commands", "BSP 2D Pan/Zoom"), NAME_None, FAppStyle::GetAppStyleSetName()) {}
	virtual void RegisterCommands() override;
	TSharedPtr<FUICommandInfo> ToggleAndDrag;
	TSharedPtr<FUICommandInfo> Reset;
	TSharedPtr<FUICommandInfo> ZoomIn;
	TSharedPtr<FUICommandInfo> ZoomOut;
	TSharedPtr<FUICommandInfo> PanLeft;
	TSharedPtr<FUICommandInfo> PanRight;
	TSharedPtr<FUICommandInfo> PanUp;
	TSharedPtr<FUICommandInfo> PanDown;
	TSharedPtr<FUICommandInfo> ToggleZoom100;
	TSharedPtr<FUICommandInfo> ToggleControlLock;
};
