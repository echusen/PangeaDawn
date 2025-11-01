// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class PangeaTamingSystem : ModuleRules
{
	public PangeaTamingSystem(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(new string[] { "AIFramework" });
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "GameplayTags", "GameplayAbilities", "GameplayTasks", "AIModule", "AscentTeams", "InventorySystem" });
	}
}
