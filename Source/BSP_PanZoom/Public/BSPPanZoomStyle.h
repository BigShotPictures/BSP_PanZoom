// Copyright Big Shot Pictures. Based on Loops 2D Pan/Zoom, Copyright 2026 Loops Creative Studio (MIT, see LICENSE).

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateStyle.h"

class FBSPPanZoomStyle
{
	public:
		static void Initialize();
		static void Shutdown();
		static FName GetStyleSetName();
		static const ISlateStyle& Get();

	private:
		static TSharedRef<class FSlateStyleSet> Create();
		static TSharedPtr<class FSlateStyleSet> StyleInstance;
};
