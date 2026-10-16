// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class GF_PangeaMiningSystemRuntime : ModuleRules
{
	public GF_PangeaMiningSystemRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
			}
			);

		PrivateIncludePaths.AddRange(
			new string[]
			{
				System.IO.Path.Combine(EngineDirectory, "Plugins/Runtime/GameplayInteractions/Source/GameplayInteractionsModule/Private"),
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
