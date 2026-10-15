// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.

using UnrealBuildTool;

public class AscentAITools : ModuleRules
{
	public AscentAITools(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"DeveloperSettings"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Projects",
			"InputCore",
			"Slate",
			"SlateCore",
			"ToolMenus",
			"Settings",
			"EditorFramework",
			"UnrealEd",
			"AssetTools",
			"AssetRegistry",
			"EditorSubsystem",
			"EditorScriptingUtilities",
			"HTTP",
			"Json",
			"JsonUtilities",
			"ImageWrapper",
			"WorkspaceMenuStructure",
			"ContentBrowser"
		});
	}
}
