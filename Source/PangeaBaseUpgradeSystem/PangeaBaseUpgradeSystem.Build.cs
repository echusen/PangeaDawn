using UnrealBuildTool;

public class PangeaBaseUpgradeSystem : ModuleRules
{
    public PangeaBaseUpgradeSystem(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core", "GameplayTags", "AscentCoreInterfaces",
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "CoreUObject",
                "Engine",
                "Slate",
                "SlateCore",
                "GameplayAbilities",
                "GameplayTasks",
                "AIModule",
                "AscentCombatFramework",
                "InventorySystem",
                "AscentQuestSystem",
                "AscentMapsSystem"
            }
        );
    }
}