// Copyright Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;

public class FabLauncherQuixelDependencies : ModuleRules
{
    public FabLauncherQuixelDependencies(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "Foliage",
                "FoliageEdit",
                "InterchangeCore",
                "InterchangeEngine",
                "InterchangePipelines",
                "Json",
                "JsonUtilities",
                "UnrealEd",
                "InterchangeNodes",
                "InterchangeFactoryNodes",
                "DeveloperSettings",
                "FabSharedInterchangeHandler"
            }
        );
    }
}