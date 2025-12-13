using UnrealBuildTool;

public class PangeaMiningSystem : ModuleRules
{
    public PangeaMiningSystem(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core", 
                "InventorySystem",
                "AIFramework",
                "AscentCoreInterfaces",
                "SmartObjectsModule",
                "GameplayBehaviorSmartObjectsModule",
                "GameplayTags",
                "GameplayTasks",
                "InputCore",
                "AIModule"
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "CoreUObject",
                "Engine",
                "Slate",
                "SlateCore", 
                "AscentCombatFramework"
            }
        );
    }
}