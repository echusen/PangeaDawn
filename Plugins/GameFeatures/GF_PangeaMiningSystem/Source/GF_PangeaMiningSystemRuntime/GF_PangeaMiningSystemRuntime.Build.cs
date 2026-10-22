// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class GF_PangeaMiningSystemRuntime : ModuleRules
{
	public GF_PangeaMiningSystemRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.Add("StateTreeEditorModule"); // Transient compiled interaction fixtures only.
			PrivateDependencyModuleNames.Add("PropertyBindingUtils"); // StateTree compiler fixture descriptor destruction.
			PrivateDependencyModuleNames.Add("AscentTeams"); // Isolated ACF possession fixtures supply GameState team configuration.
		}
		
		PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
			}
			);

				
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"GameplayTags",
				"SmartObjectsModule",
				"PangeaCore",
				"PangeaBaseUpgradeSystem",
				"AscentCombatFramework",
				"AIFramework",
				"AscentSaveSystem",
				"AscentCoreInterfaces",
				"InventorySystem",
				"UMG",
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"AIModule",
				"GameplayBehaviorSmartObjectsModule",
				"GameplayInteractionsModule",
				"GameplayStateTreeModule",
				"GameplayTasks",
				"NavigationSystem",
				"Slate",
				"SlateCore",
                "AscentUINavigationSystem",
                "CommonUI",
				"StateTreeModule",
			}
			);
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);
	}
}
