// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Pangea_Dawn : ModuleRules
{
	public Pangea_Dawn(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate",
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"Pangea_Dawn",
			"Pangea_Dawn/Variant_Platforming",
			"Pangea_Dawn/Variant_Platforming/Animation",
			"Pangea_Dawn/Variant_Combat",
			"Pangea_Dawn/Variant_Combat/AI",
			"Pangea_Dawn/Variant_Combat/Animation",
			"Pangea_Dawn/Variant_Combat/Gameplay",
			"Pangea_Dawn/Variant_Combat/Interfaces",
			"Pangea_Dawn/Variant_Combat/UI",
			"Pangea_Dawn/Variant_SideScrolling",
			"Pangea_Dawn/Variant_SideScrolling/AI",
			"Pangea_Dawn/Variant_SideScrolling/Gameplay",
			"Pangea_Dawn/Variant_SideScrolling/Interfaces",
			"Pangea_Dawn/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
