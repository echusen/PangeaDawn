// Copyright Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;
public class FabLauncher : ModuleRules
{
	public FabLauncher(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"AlembicImporter",
				"AlembicLibrary",
				"AssetRegistry",
				"AssetTools",
				"ContentBrowser",
				"Core",
				"CoreUObject",
				"EditorScriptingUtilities",
				"EditorStyle",
				"Engine",
				"Foliage",
				"FoliageEdit",
				"HTTP",
				"InterchangeCore",
				"InterchangeEngine",
				"InterchangePipelines",
				"Json",
				"JsonUtilities",
				"LevelEditor",
				"MaterialEditor",
				"Networking",
				"Projects",
				"PythonScriptPlugin",
				"Settings",
				"Slate",
				"SlateCore",
				"Sockets",
				"ToolMenus",
				"UnrealEd",
				"USDStageImporter",
				"UnrealUSDWrapper",
			}
		);

		if ((Target.Version.MajorVersion >= 5) && (Target.Version.MinorVersion >= 3))
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"FabLauncherQuixelDependencies",
					"FabSharedInterchangeHandler"
				}
			);
		}
	}
}
