// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

using UnrealBuildTool;

public class AscentFirstPersonUncooked : ModuleRules
{
    public AscentFirstPersonUncooked(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "AnimationCore",
                "AscentFirstPerson",
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "CoreUObject",
                "Engine",
                "Slate",
                "SlateCore",
                "AnimationModifiers",
                "AnimationBlueprintLibrary",
                "BlueprintGraph",
                "AnimGraph",
                "AnimGraphRuntime",
                "GraphEditor",
            }
        );
    }
}
