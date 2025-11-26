// Copyright Recursoft LLC. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class SMSystemTests : ModuleRules
{
	public SMSystemTests(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.AddRange(
			new string[]
			{
				Path.Combine(ModuleDirectory, "Public")
			});

		PrivateIncludePaths.AddRange(
			new string[]
			{
				Path.Combine(ModuleDirectory, "Private"),
				Path.Combine(ModuleDirectory, "../SMSystemEditor/Private"),
				Path.Combine(ModuleDirectory, "../SMExtendedEditor/Private")
			});
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"GameplayTags"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"UnrealEd",
				"BlueprintGraph",
				"KismetCompiler",
				"SlateCore",
				"InputCore",
				"SMSystem",
				"SMSystemEditor",
				"SMExtendedRuntime",
				"SMExtendedEditor",
				"SMAssetTools",
				"SMSearch",
				"Kismet"
			}
		);
	}
}