#include "Components/PangeaBreedableComponent.h"
#include "Components/PangeaBreedingFarmComponent.h"
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPangeaBreedingSafetyTest, "Pangea.ACF.Breeding.FertilityAndInvalidParents",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPangeaBreedingSafetyTest::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    if (!TestNotNull(TEXT("Authority fixture world"), World)) return false;
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    AActor* Owner = World->SpawnActor<AActor>();
    if (!TestNotNull(TEXT("Authority fixture owner"), Owner)) return false;
    UPangeaBreedableComponent* Parent = NewObject<UPangeaBreedableComponent>(Owner);
    Owner->AddInstanceComponent(Parent);
    TestTrue(TEXT("Default parent is fertile"), IPDBreedableInterface::Execute_IsFertile(Parent));
    Parent->bIsOnFertilityCooldown = true;
    TestFalse(TEXT("Cooldown prevents fertility"), IPDBreedableInterface::Execute_IsFertile(Parent));
    Parent->bIsOnFertilityCooldown = false;
    IPDBreedableInterface::Execute_SetFertile(Parent, false);
    TestFalse(TEXT("Explicit infertility is respected"), IPDBreedableInterface::Execute_IsFertile(Parent));
    UPangeaBreedingFarmComponent* Farm = NewObject<UPangeaBreedingFarmComponent>();
    TestNull(TEXT("Missing parents cannot spawn an egg"), Farm->TryBreed(nullptr, nullptr));
    TestNull(TEXT("A creature cannot breed with itself"), Parent->BreedWith(Parent, Farm));
    TArray<UPangeaBreedableComponent*> Males, Females;
    Farm->ContainedBreedables.Add(nullptr);
    Farm->GetBreedablesByGender(Males, Females);
    TestEqual(TEXT("Invalid entries never appear in male list"), Males.Num(), 0);
    TestEqual(TEXT("Invalid entries never appear in female list"), Females.Num(), 0);
    return true;
}
#endif
