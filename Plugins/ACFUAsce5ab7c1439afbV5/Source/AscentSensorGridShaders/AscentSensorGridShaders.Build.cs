// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved. 

using System.IO;
using UnrealBuildTool;

public class AscentSensorGridShaders : ModuleRules
{
	public AscentSensorGridShaders(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.AddRange(
			new string[]
			{
				Path.Combine(ModuleDirectory, "Public")
			}
			);

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"RenderCore",
				"VectorVM",
				"RHI",
				"Projects",
				"NiagaraCore"
			}
			);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"NiagaraCore",
				"Renderer"
			}
			);

		if (Target.bBuildEditor == true)
		{
			PublicDependencyModuleNames.Add("TargetPlatform");
		}

		PrivateIncludePathModuleNames.AddRange(
			new string[]
			{
				"DerivedDataCache",
				"Niagara"
			}
			);
	}
}
