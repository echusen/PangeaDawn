// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class GF_PangeaHuntingSystemRuntime : ModuleRules
{
	public GF_PangeaHuntingSystemRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		if (Target.bBuildEditor) PrivateDependencyModuleNames.Add("UnrealEd"); // Feature lifecycle regression tests.

		PublicDependencyModuleNames.AddRange(
			new[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"GameplayTags",
				"GameFeatures",
				"ModularGameplay",
				"PangeaCore",
				"PangeaDinosaurAI",
				"AscentCombatFramework",
				"CharacterController",
				"AscentCoreInterfaces",
				"AdvancedRPGSystem",
				"UMG",
				"Slate",
				"SlateCore", "AscentUINavigationSystem", "AscentUITools", "CommonUI"
			});
	}
}
