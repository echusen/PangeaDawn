// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved. 

using System.IO;
using UnrealBuildTool;

public class AscentSensorGrid : ModuleRules
{
	public AscentSensorGrid(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.AddRange(
			new string[]
			{
				Path.Combine(ModuleDirectory, "Public")
			}
			);

		// The Niagara Data Interface reaches into renderer internals, same as the engine's own GPU data interfaces
		PrivateIncludePaths.AddRange(
			new string[]
			{
				Path.Combine(ModuleDirectory, "Private"),
				Path.Combine(EngineDirectory, "Source", "Runtime", "Renderer", "Private")
			}
			);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"AscentSensorGridShaders",
				"Projects",
				"MeshDescription",
				"MovieScene",
				"NiagaraCore",
				"Niagara",
				"NiagaraShader",
				"RenderCore",
				"Renderer",
				"VectorVM",
				"RHI"
			}
			);
	}
}
