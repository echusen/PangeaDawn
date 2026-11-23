// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved. 

using System.IO;
using UnrealBuildTool;

public class CharacterController : ModuleRules
{
	public CharacterController(ReadOnlyTargetRules Target) : base(Target)
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
				"Core",
				"AscentCoreInterfaces",
				"DeveloperSettings",
				"AdvancedRPGSystem",
				"ActionsSystem",
				"Niagara",
                 "AnimationCore",
                 // Public animation headers expose FAnimNode_SkeletalControlBase, so every module
                 // including them needs this: it cannot stay a private dependency.
                 "AnimGraphRuntime",
                 "AscentGASRuntime",
                 "GameplayAbilities",
                 "EnhancedInput",
                 "GameplayTags",
                 // ACFAnimInstance.h exposes first person types (the hand offset data) by value.
                 "AscentFirstPerson"
            }
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"GameplayTags",
				"AIModule",
				"AscentTargetingSystem",
				"MotionWarping",
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
