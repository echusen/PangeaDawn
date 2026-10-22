#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "ACF/PangeaAdvancedHitAction.h"
#include "ACFAbilitySet.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPangeaOwnedIntegrationTest, "Pangea.ACF.Player.ProjectOwnedIntegrationContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPangeaOwnedIntegrationTest::RunTest(const FString& Parameters)
{
    UACFAbilitySet* Set = LoadObject<UACFAbilitySet>(nullptr,
        TEXT("/Game/_Game/Characters/Player/DA_Pangea_AS_Player.DA_Pangea_AS_Player"));
    if (!TestNotNull(TEXT("Pangea player ability set"), Set)) return false;
    FActionAbilityConfig Hit;
    if (!TestTrue(TEXT("Pangea player has a hit action"), Set->TryGetActionAbilityByTag(FGameplayTag::RequestGameplayTag(TEXT("Actions.Hit")), Hit))) return false;
    TestTrue(TEXT("Hit reaction uses project-owned code, not the vendor advanced hit action"),
        Hit.Action && Hit.Action->IsA<UPangeaAdvancedHitAction>());
    return !HasAnyErrors();
}
#endif
