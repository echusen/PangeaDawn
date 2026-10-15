// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class VehicleSystemEditor : ModuleRules
{
	public VehicleSystemEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.AddRange(
			new string[]
			{
				Path.Combine(ModuleDirectory, "Public")
			}
			);

		// VehicleSystem must be PUBLIC so this editor module can access VEHICLESYSTEM_API exports
		// (the runtime FAnimNode_ACFCopyPoseRotations struct backing the AnimGraph node below).
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"VehicleSystem",
				"AnimGraphRuntime",
				"AnimationCore"
			}
			);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"AnimGraph",
				"BlueprintGraph",
				"UnrealEd",
				"GraphEditor",
				"Kismet"
			}
			);

		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
			}
			);
	}
}
