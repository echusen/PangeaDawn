using UnrealBuildTool;

public class AttributeSystem : ModuleRules
{
    public AttributeSystem(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "CoreUObject",
                "Engine",
                "GameplayAbilities",
                "Slate",
                "AscentGASRuntime",
                "SlateCore",
                "Core",
                "InputCore",
                "EnhancedInput",
                "AIModule",
                "StateTreeModule",
                "GameplayStateTreeModule",
                "UMG",
            }
        );
    }
}