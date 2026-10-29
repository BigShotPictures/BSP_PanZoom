// Copyright Big Shot Pictures. Based on Loops 2D Pan/Zoom, Copyright 2026 Loops Creative Studio (MIT, see LICENSE).

using UnrealBuildTool;

public class BSP_PanZoom : ModuleRules
{
	public BSP_PanZoom(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"DeveloperSettings"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"RenderCore",
			"Slate",
			"SlateCore",
			"UnrealEd",
			"EditorSubsystem",
			"LevelEditor",
			"ToolMenus",
			"Projects",
			"ControlRig",
			"ControlRigEditor"
		});
	}
}
