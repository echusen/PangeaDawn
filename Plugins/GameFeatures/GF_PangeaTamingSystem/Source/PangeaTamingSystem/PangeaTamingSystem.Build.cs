// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class PangeaTamingSystem : ModuleRules
{
	public PangeaTamingSystem(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(new string[] { "AIFramework", "AscentUINavigationSystem" });
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// TamingWidget publicly inherits from ACFHUDUserWidget, which derives from CommonUI.
		PublicDependencyModuleNames.Add("CommonUI");

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "GameplayTags", "GameplayAbilities", "GameplayTasks", "AIModule", "AscentTeams", "InventorySystem", "AscentCombatFramework", "UMG", "ActionsSystem", "PangeaCore" });
	}
}
