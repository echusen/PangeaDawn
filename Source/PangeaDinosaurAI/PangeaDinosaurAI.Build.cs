// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class PangeaDinosaurAI : ModuleRules
{
	public PangeaDinosaurAI(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(new string[] { "MountSystem", "CharacterController", "CollisionsManager", "InventorySystem" });
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new string[] { "UnrealEd", "GameplayAbilities" }); // PIE/save regression tests.
		}

		// Public headers derive from ACF character/initializer and expose save/interaction interfaces.
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "GameplayTags", "PangeaCore", "ModularGameplay", "AscentCombatFramework", "AscentSaveSystem", "AscentCoreInterfaces", "AdvancedRPGSystem", "AscentGASRuntime" });
	}
}
