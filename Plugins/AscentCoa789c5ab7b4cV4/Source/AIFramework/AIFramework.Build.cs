// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved. 

using System.IO;
using UnrealBuildTool;

public class AIFramework : ModuleRules
{
	public AIFramework(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(
			new string[] 
			{
                Path.Combine(ModuleDirectory, "Public")
			}
			);
				
		
		PrivateIncludePaths.AddRange(
			new string[] 
			{
				
			}
			);
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core","AscentCoreInterfaces", "AscentCombatFramework",
				"GameplayTags","AscentTargetingSystem","AIModule","AscentGASRuntime",
				"ActionsSystem", "InventorySystem", "AscentTeams", "SmartObjectsModule"
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"AdvancedRPGSystem",
				"GameplayTasks",
				"NavigationSystem",
				"AIModule",
				"CharacterController",
				"AscentTeams",
				"DaySequence",
				"GameplayBehaviorSmartObjectsModule",
				"ContextualAnimation"
			}
			);
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				
			}
			);
	}
}
