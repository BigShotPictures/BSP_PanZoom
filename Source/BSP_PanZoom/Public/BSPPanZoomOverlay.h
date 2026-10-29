// Copyright Big Shot Pictures. Based on Loops 2D Pan/Zoom, Copyright 2026 Loops Creative Studio (MIT, see LICENSE).

#pragma once
#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"

class FEditorViewportClient;

class SBSPPanZoomOverlay : public SLeafWidget
{
	public:
		SLATE_BEGIN_ARGS(SBSPPanZoomOverlay) {}
		SLATE_END_ARGS()
		void Construct(const FArguments& InArgs, FEditorViewportClient* InViewportClient);
		virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
		virtual FVector2D ComputeDesiredSize(float) const override;

	private:
		FEditorViewportClient* ViewportClient = nullptr;
};
