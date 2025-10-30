// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using UnrealBuildTool.Rules;

public class AscentEditor : ModuleRules {
    public AscentEditor(ReadOnlyTargetRules Target)
        : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicIncludePaths.AddRange(
            new string[] {
                // ... add public include paths required here ...
            });

        PrivateIncludePaths.AddRange(
            new string[] {
                // ... add other private include paths required here ...
            });

        PublicDependencyModuleNames.AddRange(
            new string[] {
                "Core",
                "Blutility",
                "UMGEditor",
                "UnrealEd",
                "Kismet",
                "AssetTools",
                "EditorSubsystem"
                // ... add other public dependencies that you statically link with here ...
            });

        PrivateDependencyModuleNames.AddRange(
            new string[] {
                "Projects",
                "InputCore",
                "EditorFramework",
                "UnrealEd",
                "ToolMenus",
                "CoreUObject",
                "Engine",
                "Slate",
                "SlateCore",
                "AnimGraph",
                "AnimGraphRuntime",
                "BlueprintGraph", "Persona", "AnimationEditMode", "CharacterController",
                "ActionsSystem","AscentGASRuntime", "EditorScriptingUtilities"
                // ... add private dependencies that you statically link with here ...
            });

        DynamicallyLoadedModuleNames.AddRange(
            new string[] {
                // ... add any modules that your module loads dynamically here ...
            });
    }
}
