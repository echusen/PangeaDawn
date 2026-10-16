#include "Characters/PDDinosaurBase.h"
#include "ACFGASStatisticsComponent.h"
#include "Components/ACFDamageHandlerComponent.h"
#include "Components/ACFEquipmentComponent.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPangeaDinosaurSaveTest, "Pangea.ACF.Save.DinosaurPreservesACFComponents",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPangeaDinosaurSaveTest::RunTest(const FString& Parameters)
{
    const APDDinosaurBase* Dinosaur = GetDefault<APDDinosaurBase>();
    const TArray<UActorComponent*> Saved = Dinosaur->GetComponentsToSave_Implementation();
    TestTrue(TEXT("ACF GAS statistics are saved"), Saved.Contains(Dinosaur->FindComponentByClass<UACFGASStatisticsComponent>()));
    TestTrue(TEXT("ACF damage state is saved"), Saved.Contains(Dinosaur->FindComponentByClass<UACFDamageHandlerComponent>()));
    TestTrue(TEXT("ACF equipment and inventory are saved"), Saved.Contains(Dinosaur->FindComponentByClass<UACFEquipmentComponent>()));
    TestFalse(TEXT("Save list has no null components"), Saved.Contains(nullptr));
    return true;
}
#endif
