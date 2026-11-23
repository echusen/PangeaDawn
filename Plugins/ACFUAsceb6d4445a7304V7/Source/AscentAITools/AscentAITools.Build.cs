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
			"EditorSubsystem",
			"HTTP",
			"DeveloperSettings",
			// The generation card widgets are public headers, so Slate is a public dependency.
			"InputCore",
			"Slate",
			"SlateCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Projects",
			"ToolMenus",
			"Settings",
			"EditorFramework",
			"UnrealEd",
			"AssetTools",
			"AssetRegistry",
			// Sound effect import: USoundFactory.
			"AudioEditor",
			"DesktopPlatform",
			"EditorScriptingUtilities",
			"Json",
			"JsonUtilities",
			"ImageWrapper",
			"WorkspaceMenuStructure",
			"ContentBrowser",
			// Motion import: BVH skeletons, IK Rigs, IK Retargeter and batch retarget.
			"IKRig",
			"IKRigEditor",
			"SkeletalMeshUtilitiesCommon"
		});
	}
}
