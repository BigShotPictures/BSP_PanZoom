// Copyright Big Shot Pictures. Based on Loops 2D Pan/Zoom, Copyright 2026 Loops Creative Studio (MIT, see LICENSE).

#include "BSPPanZoomStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Interfaces/IPluginManager.h"
#include "Brushes/SlateImageBrush.h"

TSharedPtr<FSlateStyleSet> FBSPPanZoomStyle::StyleInstance = nullptr;

void FBSPPanZoomStyle::Initialize()
{
	if (!StyleInstance.IsValid())
	{
		StyleInstance = Create();
		FSlateStyleRegistry::RegisterSlateStyle(*StyleInstance);
	}
}

void FBSPPanZoomStyle::Shutdown()
{
	if (StyleInstance.IsValid())
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*StyleInstance);
		StyleInstance.Reset();
	}
}

FName FBSPPanZoomStyle::GetStyleSetName()
{
	static FName StyleSetName(TEXT("BSPPanZoomStyle"));
	return StyleSetName;
}

const ISlateStyle& FBSPPanZoomStyle::Get()
{
	return *StyleInstance;
}

TSharedRef<FSlateStyleSet> FBSPPanZoomStyle::Create()
{
	TSharedRef<FSlateStyleSet> Style = MakeShareable(new FSlateStyleSet(GetStyleSetName()));

	if (const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("BSP_PanZoom")))
	{
		Style->SetContentRoot(Plugin->GetBaseDir() / TEXT("Resources"));
	}

	const FVector2D IconSize(40.0f, 40.0f);
	Style->Set("BSPPanZoom.Icon", new FSlateImageBrush(Style->RootToContentDir(TEXT("Icons/BSPPanZoom"), TEXT(".png")), IconSize));

	return Style;
}
