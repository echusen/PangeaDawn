// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved. 

using System.IO;
using UnrealBuildTool;

public class VehicleSystem : ModuleRules
{
	public VehicleSystem(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(
			new string[] 
			{
                Path.Combine(ModuleDirectory, "Public")
			}
			);

		PublicDependencyModuleNames.Add("GameplayAbilities");
		PublicDependencyModuleNames.Add("AdvancedRPGSystem");
				
		
		PrivateIncludePaths.AddRange(
			new string[] 
			{
				
			}
			);
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"AscentCombatFramework",
				"AIModule",
				"AscentCoreInterfaces",
				"AIFramework",
                "ChaosVehicles","AscentTeams","GameplayTags",
				"EnhancedInput",
				"ActionsSystem",
				"AnimGraphRuntime",
				"AnimationCore",
				"MotionWarping"
            }
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"NetCore",
				"AdvancedRPGSystem",
				"CollisionsManager",
                "AscentCoreInterfaces",
                "AIFramework",
                "ChaosVehicles",
				"MountSystem",
				"EnhancedInput",
				"GameplayTasks",
				"InventorySystem"
            }
			);
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				
			}
			);
	}
}
