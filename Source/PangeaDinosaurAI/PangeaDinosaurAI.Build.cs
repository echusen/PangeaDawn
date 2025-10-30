// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class PangeaDinosaurAI : ModuleRules
{
	public PangeaDinosaurAI(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(new string[] { "MountSystem", "PangeaBreedingSystem", "AscentSaveSystem", "AscentCoreInterfaces", "AscentCombatFramework", "CharacterController", "PangeaTamingSystem" });
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "GameplayTags" });
	}
}
