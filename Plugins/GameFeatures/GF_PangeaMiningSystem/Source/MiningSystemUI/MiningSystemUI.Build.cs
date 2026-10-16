using UnrealBuildTool;

public class MiningSystemUI : ModuleRules
{
    public MiningSystemUI(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
        if (Target.bBuildEditor)
        {
            PrivateDependencyModuleNames.Add("UnrealEd"); // Editor-only latent PIE smoke tests.
            PrivateDependencyModuleNames.Add("PangeaBaseUpgradeSystem"); // Village-to-mine interaction regression.
        }

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "InventorySystem",
                "AscentCombatFramework",
                "GF_PangeaMiningSystemRuntime",
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
                "AscentUITools",
                "GameplayTags",
            }
        );

    }
}
