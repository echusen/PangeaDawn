using UnrealBuildTool;

public class PangeaBaseUpgradeSystem : ModuleRules
{
    public PangeaBaseUpgradeSystem(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "GameplayTags",
                "AscentCoreInterfaces",
                "ModularGameplay",
                "UMG",
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "Slate",
                "SlateCore",
                "AscentUINavigationSystem",
                "CommonUI",
                "GameplayAbilities",
                "GameplayTasks",
                "AIModule",
                "AscentCombatFramework",
                "InventorySystem",
                "AscentQuestSystem",
                "AscentMapsSystem",
                "AscentSaveSystem",
                "AdvancedRPGSystem"
            }
        );
    }
}
