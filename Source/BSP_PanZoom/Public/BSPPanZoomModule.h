// Copyright Big Shot Pictures. Based on Loops 2D Pan/Zoom, Copyright 2026 Loops Creative Studio (MIT, see LICENSE).

#pragma once
#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Styling/SlateTypes.h"

class FEditorViewportClient;
struct FToolMenuContext;

class FBSPPanZoomModule : public IModuleInterface
{
	public:
		virtual void StartupModule() override;
		virtual void ShutdownModule() override;

	private:
		void RegisterToolbarExtension();
		void UnregisterToolbarExtension();
		void OnToggleClicked(const FToolMenuContext& InContext);
		ECheckBoxState GetToggleCheckState(const FToolMenuContext& InContext) const;
		FText GetToggleTooltipText() const;
		void FillOptionsMenu(class UToolMenu* Menu);

		TSharedPtr<class FBSPPanZoomInputProcessor> InputProcessor;
};
