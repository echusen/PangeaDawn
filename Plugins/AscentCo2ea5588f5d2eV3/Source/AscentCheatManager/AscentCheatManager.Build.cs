// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2025. All Rights Reserved.

using UnrealBuildTool;

public class AscentCheatManager : ModuleRules
{
    public AscentCheatManager(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
            {
                "Core",
                "CoreUObject",
                "Engine",

            });

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                 "AscentMapsSystem",
                 "InventorySystem",
                "AscentQuestSystem",
                "AdvancedRPGSystem",
                "AscentMapsSystem",
                "GameplayTags", "AscentGASRuntime",

            });

        DynamicallyLoadedModuleNames.AddRange(
            new string[]
            {

            });
    }
}
