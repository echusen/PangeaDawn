// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2025. All Rights Reserved.

using UnrealBuildTool;

public class AscentAttributesEditor : ModuleRules
{
    public AscentAttributesEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "Slate",
                "AscentGASEditor",
                "SlateCore",
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "UnrealEd",
                "Kismet",
                "BlueprintGraph",
                "GraphEditor",
                "ApplicationCore",
                "PropertyEditor",
                "EditorSubsystem",
                "InputCore",
                "GameplayAbilities",
                "GameplayTags",
                "GameplayTasks"
            }
        );
    }
}


