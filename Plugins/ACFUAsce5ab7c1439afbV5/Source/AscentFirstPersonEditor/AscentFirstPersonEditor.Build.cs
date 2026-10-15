// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

using UnrealBuildTool;

public class AscentFirstPersonEditor : ModuleRules
{
    public AscentFirstPersonEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "AscentFirstPerson",
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "Slate",
                "SlateCore",
                "InputCore",
                "ToolMenus",
                "ToolWidgets",
                "UnrealEd",
                "EditorFramework",
                "PropertyEditor",
                "AssetRegistry",
                "ContentBrowser",
                "WorkspaceMenuStructure",
                "Projects",
                "DeveloperSettings",
                "Blutility",
                "UMGEditor",
            }
        );
    }
}
