// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

using UnrealBuildTool;

public class ClimbingSystemEditor : ModuleRules
{
	public ClimbingSystemEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
			}
			);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
                "InputCore",
                "UnrealEd",
                "AssetTools",
                "AssetDefinition",
				"Slate",
				"SlateCore",
                "AdvancedPreviewScene",
                "PropertyEditor",
                "Projects",
                "ToolMenus",
                "CommonMenuExtensions",
                "EditorFramework",
                "SkeletalMeshUtilitiesCommon",
                "TargetPlatform",
                "SkeletonEditor",
                "RenderCore",
                "RHI",

                "ClimbingSystem"
			}
			);
	}
}
