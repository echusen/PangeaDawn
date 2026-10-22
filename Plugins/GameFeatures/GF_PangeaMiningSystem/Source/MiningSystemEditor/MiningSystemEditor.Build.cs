using UnrealBuildTool;

public class MiningSystemEditor : ModuleRules
{
    public MiningSystemEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "GF_PangeaMiningSystemRuntime",
                "MiningSystemPresentation",
                "MiningSystemUI",
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "Slate",
                "SlateCore",
                "UnrealEd",
                "LevelEditor",
                "AIFramework",
                "AscentCombatFramework",
                "InventorySystem",
                "ActionsSystem",
                "AscentSaveSystem",
                "GameplayAbilities",
                "CharacterController",
                "AscentTeams",
                "AIModule",
                "NavigationSystem",
                "GameplayTasks",
                "SmartObjectsModule",
                "GameplayTags",
            }
        );
    }
}
