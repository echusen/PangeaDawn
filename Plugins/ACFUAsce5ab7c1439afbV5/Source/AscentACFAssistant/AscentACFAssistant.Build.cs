// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class AscentACFAssistant : ModuleRules
{
	public AscentACFAssistant(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		bUseUnity = false;

		foreach (string Resource in new[] { "marked.min.js", "purify.min.js", "markdown.js", "LICENSE.marked", "LICENSE.dompurify" })
		{
			RuntimeDependencies.Add(Path.Combine(PluginDirectory, "Resources", "Phone", Resource), StagedFileType.NonUFS);
		}

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"Slate",
			"SlateCore",
			"AscentAIAgent",
			"AscentAITools",
			"ToolsetRegistry"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"DeveloperSettings",
			"ModelContextProtocol",
			"ModelContextProtocolEngine",
			"Json",
			"JsonUtilities",
			"Projects",
			"UnrealEd",
			"EditorFramework",
			"ToolMenus",
			"LevelEditor",
			"Settings",
			"ImageWrapper",
			"AppFramework",
			"DesktopPlatform",
			"Sockets",
			"HTTP",
			"HTTPServer",
			"ApplicationCore"
		});

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			// Windows CNG verifies the SHA-256 of the downloaded Iroh helper and generates the Iroh pairing's random bytes.
			// DPAPI (crypt32) seals that pairing at rest.
			PublicSystemLibraries.Add("bcrypt.lib");
			PublicSystemLibraries.Add("crypt32.lib");
		}
	}
}
