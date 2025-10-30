// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved. 

using UnrealBuildTool;

public class UnitsSystem : ModuleRules
{
    public UnitsSystem(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                 "AscentCombatFramework",
                "AIModule",
                "AscentCoreInterfaces",
                "AIFramework",
                "AscentSaveSystem",
                  "GameplayTags",
                  "AIFramework",
                  "AscentDialogueSystem"
            });

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {

            });

        DynamicallyLoadedModuleNames.AddRange(
            new string[]
            {

            });
    }
}
