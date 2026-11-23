// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

using UnrealBuildTool;

public class ClimbingSystem : ModuleRules
{
	public ClimbingSystem(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"PhysicsCore",
				"MotionWarping",
				// The free climb component exposes gameplay effect costs by value in its header.
				"AscentGASRuntime",
				"GameplayAbilities"
			});

		PrivateDependencyModuleNames.Add("CharacterController");
	}
}
