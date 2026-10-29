// Copyright Big Shot Pictures. Based on Loops 2D Pan/Zoom, Copyright 2026 Loops Creative Studio (MIT, see LICENSE).

#include "BSPPanZoomOverlay.h"
#include "BSPPanZoomSubsystem.h"
#include "EditorViewportClient.h"
#include "Editor.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

namespace BSPPanZoomOverlayLayout
{
	//Default Layout
	constexpr float FrameWidth = 140.0f;
	constexpr float FrameHeight = 80.0f;
	constexpr float Margin = 12.0f;
	constexpr float TextHeight = 20.0f;
}

void SBSPPanZoomOverlay::Construct(const FArguments& InArgs, FEditorViewportClient* InViewportClient)
{
	ViewportClient = InViewportClient;
}

FVector2D SBSPPanZoomOverlay::ComputeDesiredSize(float) const
{
	return FVector2D(BSPPanZoomOverlayLayout::FrameWidth + BSPPanZoomOverlayLayout::Margin * 2.0f,
		BSPPanZoomOverlayLayout::FrameHeight + BSPPanZoomOverlayLayout::Margin * 2.0f + BSPPanZoomOverlayLayout::TextHeight * 2.0f);
}

int32 SBSPPanZoomOverlay::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	if (!ViewportClient || !GEditor)
	{
		return LayerId;
	}

	UBSPPanZoomSubsystem* Subsystem = GEditor->GetEditorSubsystem<UBSPPanZoomSubsystem>();

	float ZoomPercent = 100.0f;
	FVector2D CropSize = FVector2D(1.0f, 1.0f);
	FVector2D CropCenterOffset = FVector2D::ZeroVector;
	bool bIsAnimControlLockActive = false;
	if (!Subsystem || !Subsystem->GetOverlayInfo(ViewportClient, ZoomPercent, CropSize, CropCenterOffset, bIsAnimControlLockActive))
	{
		return LayerId;
	}

	const FVector2D LocalSize = AllottedGeometry.GetLocalSize();
	const float FrameLeft = BSPPanZoomOverlayLayout::Margin;
	const float FrameTop = BSPPanZoomOverlayLayout::Margin + BSPPanZoomOverlayLayout::TextHeight;

	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId,
		AllottedGeometry.ToPaintGeometry(FVector2D(BSPPanZoomOverlayLayout::FrameWidth, 
			BSPPanZoomOverlayLayout::FrameHeight), FSlateLayoutTransform(FVector2D(FrameLeft, FrameTop))),
		FCoreStyle::Get().GetBrush("WhiteBrush"),
		ESlateDrawEffect::None,
		FLinearColor(0.0f, 0.0f, 0.0f, 0.35f)
	);

	{
		const FVector2D InnerSize(BSPPanZoomOverlayLayout::FrameWidth * CropSize.X, BSPPanZoomOverlayLayout::FrameHeight * CropSize.Y);
		const FVector2D InnerCenter(
			FrameLeft + BSPPanZoomOverlayLayout::FrameWidth * (0.5f + CropCenterOffset.X),
			FrameTop + BSPPanZoomOverlayLayout::FrameHeight * (0.5f + CropCenterOffset.Y)
		);
		const FVector2D InnerTopLeft = InnerCenter - InnerSize * 0.5f;

		TArray<FVector2D> Points;
		Points.Add(InnerTopLeft);
		Points.Add(InnerTopLeft + FVector2D(InnerSize.X, 0.0f));
		Points.Add(InnerTopLeft + InnerSize);
		Points.Add(InnerTopLeft + FVector2D(0.0f, InnerSize.Y));
		Points.Add(InnerTopLeft);

		const FLinearColor FrameColor = bIsAnimControlLockActive
			? FLinearColor(1.0f, 0.85f, 0.0f, 1.0f)
			: FLinearColor(33.0f / 255.0f, 166.0f / 255.0f, 227.0f / 255.0f, 1.0f);

		FSlateDrawElement::MakeLines(
			OutDrawElements,
			LayerId + 1,
			AllottedGeometry.ToPaintGeometry(),
			Points,
			ESlateDrawEffect::None,
			FrameColor,
			true,
			2.0f
		);
	}
	const FString ZoomText = FString::Printf(TEXT("2D Pan/Zoom: %.0f%%"), ZoomPercent);
	FSlateDrawElement::MakeText(
		OutDrawElements,
		LayerId + 2,
		AllottedGeometry.ToPaintGeometry(FVector2D(BSPPanZoomOverlayLayout::FrameWidth, BSPPanZoomOverlayLayout::TextHeight), FSlateLayoutTransform(FVector2D(FrameLeft, FrameTop - BSPPanZoomOverlayLayout::TextHeight))),
		ZoomText,
		FCoreStyle::GetDefaultFontStyle("Bold", 8),
		ESlateDrawEffect::None,
		FLinearColor::White
	);

	if (bIsAnimControlLockActive)
	{
		static const FString LockText = TEXT("Auto Focus on Control");

		FSlateDrawElement::MakeText(
			OutDrawElements,
			LayerId + 2,
			AllottedGeometry.ToPaintGeometry(FVector2D(BSPPanZoomOverlayLayout::FrameWidth, BSPPanZoomOverlayLayout::TextHeight), FSlateLayoutTransform(FVector2D(FrameLeft, FrameTop + BSPPanZoomOverlayLayout::FrameHeight))),
			LockText,
			FCoreStyle::GetDefaultFontStyle("Bold", 8),
			ESlateDrawEffect::None,
			FLinearColor(1.0f, 0.85f, 0.0f, 1.0f)
		);
	}

	return LayerId + 3;
}
