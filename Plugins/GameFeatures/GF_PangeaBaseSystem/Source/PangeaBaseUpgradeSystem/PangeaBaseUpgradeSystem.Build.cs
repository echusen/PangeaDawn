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
				"InventorySystem", // Public item-grant and explicit cost types.
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
                "AscentQuestSystem",
                "AscentMapsSystem",
                "AscentSaveSystem",
                "AdvancedRPGSystem"
            }
        );
    }
}
