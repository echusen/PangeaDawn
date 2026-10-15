// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

using UnrealBuildTool;

public class AscentGraphIntegrations : ModuleRules
{
	public AscentGraphIntegrations(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"GameplayTags",
				"AGSGraphRuntime"
			});

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"AscentCoreInterfaces",
				"AscentCombatFramework",
				"AscentDialogueSystem",
				"AscentSaveSystem",
				"AscentTeams"
			});

		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{

			});
	}
}
