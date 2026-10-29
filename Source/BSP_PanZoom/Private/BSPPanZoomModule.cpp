// Copyright Big Shot Pictures. Based on Loops 2D Pan/Zoom, Copyright 2026 Loops Creative Studio (MIT, see LICENSE).

#include "BSPPanZoomModule.h"
#include "BSPPanZoomCommands.h"
#include "BSPPanZoomInputProcessor.h"
#include "BSPPanZoomSubsystem.h"
#include "BSPPanZoomSettings.h"
#include "EditorViewportClient.h"
#include "LevelEditorViewport.h"
#include "Editor.h"
#include "ToolMenus.h"
#include "BSPPanZoomStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "Modules/ModuleManager.h"
#include "ToolMenuContext.h"
#include "ToolMenuDelegates.h"
#include "ViewportToolbar/UnrealEdViewportToolbarContext.h"
#include "SEditorViewport.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "FBSPPanZoomModule"

namespace BSPPanZoom
{
	static const FName ToolbarMenuName("LevelEditor.ViewportToolbar");
	static const FName ToolbarSectionName("BSPPanZoom");

	static FEditorViewportClient* GetActiveEditorViewportClient()
	{
		if (!GEditor)
		{
			return nullptr;
		}
		FViewport* ActiveVP = GEditor->GetActiveViewport();
		if (ActiveVP)
		{
			for (FEditorViewportClient* Candidate : GEditor->GetAllViewportClients())
			{
				if (Candidate && Candidate->Viewport == ActiveVP)
				{
					return Candidate;
				}
			}
		}
		for (FEditorViewportClient* Candidate : GEditor->GetAllViewportClients())
		{
			if (Candidate)
			{
				return Candidate;
			}
		}
		return nullptr;
	}

	static FEditorViewportClient* GetViewportClientFromContext(const FToolMenuContext& Context)
	{
		if (const UUnrealEdViewportToolbarContext* ContextObject = Context.FindContext<UUnrealEdViewportToolbarContext>())
		{
			if (TSharedPtr<SEditorViewport> ViewportWidget = ContextObject->Viewport.Pin())
			{
				if (TSharedPtr<FEditorViewportClient> Client = ViewportWidget->GetViewportClient())
				{
					return Client.Get();
				}
			}
		}
		return GetActiveEditorViewportClient();
	}

	// Fonctionne seulement pour 5.8
	// static FEditorViewportClient* GetViewportClientFromContext(const FToolMenuContext& Context)
	// {
	// 	if (TSharedPtr<FEditorViewportClient> ContextClient = UUnrealEdViewportToolbarContext::GetEditorViewportClient(Context))
	// 	{
	// 		return ContextClient.Get();
	// 	}
	// 	return GetActiveEditorViewportClient();
	// }

	static void FocusViewport(FEditorViewportClient* ViewportClient)
	{
		if (!ViewportClient)
		{
			return;
		}
		if (TSharedPtr<SEditorViewport> EditorViewportWidget = ViewportClient->GetEditorViewportWidget())
		{
			FSlateApplication::Get().SetKeyboardFocus(EditorViewportWidget, EFocusCause::SetDirectly);
		}
	}
}

void FBSPPanZoomModule::StartupModule()
{
	FBSPPanZoomStyle::Initialize();
	FBSPPanZoomCommands::Register();

	InputProcessor = MakeShared<FBSPPanZoomInputProcessor>();
	FSlateApplication::Get().RegisterInputPreProcessor(InputProcessor);

	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(
		this, &FBSPPanZoomModule::RegisterToolbarExtension));

}

void FBSPPanZoomModule::ShutdownModule()
{
	UnregisterToolbarExtension();

	if (FSlateApplication::IsInitialized() && InputProcessor.IsValid())
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(InputProcessor);
	}
	InputProcessor.Reset();
	FBSPPanZoomCommands::Unregister();

	FBSPPanZoomStyle::Shutdown();
}

void FBSPPanZoomModule::RegisterToolbarExtension()
{
	if (!UToolMenus::IsToolMenuUIEnabled())
	{
		return;
	}

	UToolMenu* Menu = UToolMenus::Get()->ExtendMenu(BSPPanZoom::ToolbarMenuName);
	if (!Menu)
	{
		return;
	}

	FToolMenuSection& Section = Menu->FindOrAddSection(BSPPanZoom::ToolbarSectionName);

	FToolMenuEntry ToggleEntry = FToolMenuEntry::InitToolBarButton(
		"BSPPanZoomToggle",
		FToolUIActionChoice(FToolUIAction(
			FToolMenuExecuteAction::CreateRaw(this, &FBSPPanZoomModule::OnToggleClicked),
			FToolMenuCanExecuteAction(),
			FToolMenuGetActionCheckState::CreateRaw(this, &FBSPPanZoomModule::GetToggleCheckState)
		)),
		// Empty label: icon-only in the toolbar, matching the look of most other viewport toolbar buttons.
		FText::GetEmpty(),
		TAttribute<FText>::CreateRaw(this, &FBSPPanZoomModule::GetToggleTooltipText),
		FSlateIcon(FBSPPanZoomStyle::GetStyleSetName(), "BSPPanZoom.Icon"),
		EUserInterfaceActionType::ToggleButton
	);
	Section.AddEntry(ToggleEntry);

	FToolMenuEntry OptionsEntry = FToolMenuEntry::InitComboButton(
		"BSPPanZoomOptions",
		FUIAction(),
		FNewToolMenuDelegate::CreateRaw(this, &FBSPPanZoomModule::FillOptionsMenu),
		LOCTEXT("OptionsLabel", "Pan/Zoom"),
		LOCTEXT("OptionsTooltip", "Exact 2D Pan/Zoom values for this viewport, and options"));
	Section.AddEntry(OptionsEntry);
}

void FBSPPanZoomModule::FillOptionsMenu(UToolMenu* Menu)
{
	FEditorViewportClient* const Client = BSPPanZoom::GetViewportClientFromContext(Menu->Context);
	if (!Client)
	{
		return;
	}

	// The menu can outlive the viewport (layout change while open), so every callback re-checks.
	auto GetSubsystem = [Client]() -> UBSPPanZoomSubsystem*
	{
		return GEditor && UBSPPanZoomSubsystem::IsLiveViewportClient(Client)
			? GEditor->GetEditorSubsystem<UBSPPanZoomSubsystem>()
			: nullptr;
	};

	FToolMenuSection& ViewSection = Menu->AddSection("BSPPanZoomView", LOCTEXT("ViewSection", "2D Pan/Zoom"));

	ViewSection.AddEntry(FToolMenuEntry::InitWidget("Zoom",
		SNew(SSpinBox<float>)
			.MinValue(5.0f).MaxValue(2000000.0f)
			.MinSliderValue(25.0f).MaxSliderValue(800.0f)
			.Delta(1.0f)
			.ToolTipText(LOCTEXT("ZoomTip", "100% is the camera's own framing, 200% twice as close"))
			.Value_Lambda([GetSubsystem, Client]()
			{
				const UBSPPanZoomSubsystem* Subsystem = GetSubsystem();
				return Subsystem ? Subsystem->GetZoom(Client) * 100.0f : 100.0f;
			})
			.OnValueChanged_Lambda([GetSubsystem, Client](float Percent)
			{
				if (UBSPPanZoomSubsystem* Subsystem = GetSubsystem())
				{
					Subsystem->SetZoom(Client, Percent / 100.0f);
				}
			}),
		LOCTEXT("ZoomLabel", "Zoom %")));

	// Perspective pan is in frames (1 = half a frame, edge to centre); ortho pan is world units.
	const bool bPerspective = Client->IsPerspective();
	auto AddPanEntry = [&ViewSection, GetSubsystem, Client, bPerspective](FName Name, FText Label, FText ToolTip, int32 Axis)
	{
		ViewSection.AddEntry(FToolMenuEntry::InitWidget(Name,
			SNew(SSpinBox<float>)
				.MinSliderValue(bPerspective ? -1.0f : TOptional<float>())
				.MaxSliderValue(bPerspective ? 1.0f : TOptional<float>())
				.Delta(bPerspective ? 0.01f : 1.0f)
				.ToolTipText(ToolTip)
				.Value_Lambda([GetSubsystem, Client, Axis]()
				{
					const UBSPPanZoomSubsystem* Subsystem = GetSubsystem();
					return Subsystem ? static_cast<float>(Subsystem->GetPan(Client)[Axis]) : 0.0f;
				})
				.OnValueChanged_Lambda([GetSubsystem, Client, Axis](float Value)
				{
					if (UBSPPanZoomSubsystem* Subsystem = GetSubsystem())
					{
						FVector2D Pan = Subsystem->GetPan(Client);
						Pan[Axis] = Value;
						Subsystem->SetPan(Client, Pan);
					}
				}),
			Label));
	};
	AddPanEntry("PanX", LOCTEXT("PanXLabel", "Pan X"),
		bPerspective ? LOCTEXT("PanXTipPersp", "Sideways: 1 brings the frame's right edge to the centre") : LOCTEXT("PanXTipOrtho", "Sideways, in world units"), 0);
	AddPanEntry("PanY", LOCTEXT("PanYLabel", "Pan Y"),
		bPerspective ? LOCTEXT("PanYTipPersp", "Up and down: 1 brings the frame's top edge to the centre") : LOCTEXT("PanYTipOrtho", "Up and down, in world units"), 1);

	ViewSection.AddMenuEntry("Reset",
		LOCTEXT("ResetLabel", "Reset"),
		LOCTEXT("ResetTip", "Back to 100% zoom, no pan"),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([GetSubsystem, Client]()
		{
			if (UBSPPanZoomSubsystem* Subsystem = GetSubsystem())
			{
				Subsystem->Reset(Client);
			}
		})));

	FToolMenuSection& OptionsSection = Menu->AddSection("BSPPanZoomOptions", LOCTEXT("OptionsSection", "Options"));

	OptionsSection.AddMenuEntry("LensShift",
		LOCTEXT("LensShiftLabel", "Lens Shift Through Cameras"),
		LOCTEXT("LensShiftTip", "When looking through a camera, pan slides the frame like Maya's film offset instead of tilting the view"),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([Client]()
			{
				UBSPPanZoomSettings* Settings = GetMutableDefault<UBSPPanZoomSettings>();
				Settings->bLensShiftThroughCamera = !Settings->bLensShiftThroughCamera;
				Settings->SaveConfig();
				if (UBSPPanZoomSubsystem::IsLiveViewportClient(Client))
				{
					Client->Invalidate();
				}
			}),
			FCanExecuteAction(),
			FIsActionChecked::CreateLambda([]() { return GetDefault<UBSPPanZoomSettings>()->bLensShiftThroughCamera; })),
		EUserInterfaceActionType::ToggleButton);
}

void FBSPPanZoomModule::UnregisterToolbarExtension()
{
	if (UToolMenus* ToolMenus = UToolMenus::TryGet())
	{
		ToolMenus->RemoveSection(BSPPanZoom::ToolbarMenuName, BSPPanZoom::ToolbarSectionName);
	}
}

void FBSPPanZoomModule::OnToggleClicked(const FToolMenuContext& InContext)
{
	if (GEditor)
	{
		if (FEditorViewportClient* Client = BSPPanZoom::GetViewportClientFromContext(InContext))
		{
			if (UBSPPanZoomSubsystem* Subsystem = GEditor->GetEditorSubsystem<UBSPPanZoomSubsystem>())
			{
				Subsystem->ToggleEnabled(Client);
			}
			BSPPanZoom::FocusViewport(Client);
		}
	}
}

ECheckBoxState FBSPPanZoomModule::GetToggleCheckState(const FToolMenuContext& InContext) const
{
	if (GEditor)
	{
		if (FEditorViewportClient* Client = BSPPanZoom::GetViewportClientFromContext(InContext))
		{
			if (const UBSPPanZoomSubsystem* Subsystem = GEditor->GetEditorSubsystem<UBSPPanZoomSubsystem>())
			{
				return Subsystem->IsEnabled(Client) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			}
		}
	}
	return ECheckBoxState::Unchecked;
}

FText FBSPPanZoomModule::GetToggleTooltipText() const
{
	const FBSPPanZoomCommands& Commands = FBSPPanZoomCommands::Get();

	FFormatOrderedArguments Args;
	Args.Add(Commands.ToggleAndDrag->GetInputText());
	Args.Add(Commands.Reset->GetInputText());
	Args.Add(Commands.PanLeft->GetInputText());
	Args.Add(Commands.PanRight->GetInputText());
	Args.Add(Commands.PanUp->GetInputText());
	Args.Add(Commands.PanDown->GetInputText());
	Args.Add(Commands.ZoomIn->GetInputText());
	Args.Add(Commands.ZoomOut->GetInputText());
	Args.Add(Commands.ToggleZoom100->GetInputText());
	Args.Add(Commands.ToggleControlLock->GetInputText());

	return FText::Format(LOCTEXT("BSPPanZoomToggleTooltip",
		"BSP 2D Pan/Zoom\n"
		"Tap {0} to toggle\n"
		"{1} to reset the view\n"
		"Hold {0} + MMB drag to pan (slides the frame through a camera, tilts a free view)\n"
		"Hold {0} + RMB drag or mouse wheel to zoom (adjusts FOV / ortho zoom)\n"
		"Toggle/drag and reset shortcuts default to Animation Mode only\n"
		"{2}/{3}/{4}/{5} to pan, {6}/{7} to zoom\n"
		"{8} to toggle zoom+pan between their current values and 100%\n"
		"{9} to lock the camera to the selected Control Rig control\n"),
		Args);
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FBSPPanZoomModule, BSP_PanZoom)

