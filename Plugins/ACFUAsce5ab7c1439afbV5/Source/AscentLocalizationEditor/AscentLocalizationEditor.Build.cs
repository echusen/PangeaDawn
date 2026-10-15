// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

using UnrealBuildTool;

public class AscentLocalizationEditor : ModuleRules
{
	public AscentLocalizationEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		bUseUnity = false;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"Slate",
			"SlateCore",
			"InputCore",
			"UnrealEd",
			"EditorSubsystem",
			"ToolMenus",
			"Localization",
			"LocalizationCommandletExecution",
			"AscentACFAssistant",
			"Json"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Projects",
			"EditorFramework",
			"LevelEditor",
			"WorkspaceMenuStructure",
			"DesktopPlatform",
			"AppFramework"
		});
	}
}
