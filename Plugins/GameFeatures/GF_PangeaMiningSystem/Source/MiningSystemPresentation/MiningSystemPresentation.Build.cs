using UnrealBuildTool;

public class MiningSystemPresentation : ModuleRules
{
    public MiningSystemPresentation(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;


        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "GameplayTags",
                "GF_PangeaMiningSystemRuntime",
                "SmartObjectsModule",
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "AIModule",
                "AscentCombatFramework",
                "GameplayBehaviorSmartObjectsModule",
                "GameplayInteractionsModule",
                "GameplayStateTreeModule",
                "GameplayTasks",
                "NavigationSystem",
                "Slate",
                "SlateCore",
                "StateTreeModule",
            }
        );

    }
}
