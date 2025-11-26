// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class FabSharedInterchangeHandler : ModuleRules
	{
		public FabSharedInterchangeHandler(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
					"InterchangeCore",
					"InterchangeEngine",
					"InterchangeFactoryNodes",
					"InterchangeNodes",
					"MeshDescription",
					"StaticMeshDescription"
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"ApplicationCore",
					"AssetTools",
					"InterchangeImport",
					"InterchangePipelines",
					"DeveloperSettings",
					"InputCore",
					"Slate",
					"SlateCore",
					"Json",
					"JsonUtilities",
					"USDStageImporter",
					"UnrealUSDWrapper"
				}
			);

			if (Target.Type == TargetType.Editor)
			{
				PrivateDependencyModuleNames.AddRange(
					new string[]
					{
						"MainFrame",
						"UnrealEd",
						"Kismet",
						"KismetCompiler",
						"AssetRegistry"
					}
				);
			}
		}
	}
}
