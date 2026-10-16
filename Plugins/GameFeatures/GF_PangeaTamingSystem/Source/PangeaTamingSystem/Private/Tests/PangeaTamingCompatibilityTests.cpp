#include "Components/PangeaTamingComponent.h"
#include "DataAssets/TameSpeciesConfig.h"
#include "Misc/AutomationTest.h"
#include "Components/ACFInventoryComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPangeaTamingSafetyTest, "Pangea.ACF.Taming.InvalidInstigatorAndRoles",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPangeaTamingSafetyTest::RunTest(const FString& Parameters)
{
    UPangeaTamingComponent* Taming = NewObject<UPangeaTamingComponent>();
    Taming->TameSpeciesConfig = NewObject<UTameSpeciesConfig>(Taming);
    Taming->TameSpeciesConfig->bCanBeMount = true;
    Taming->TameSpeciesConfig->bCanBeCompanion = false;
    Taming->StartTameAttempt(nullptr);
    TestFalse(TEXT("A missing instigator is never cached"), Taming->CachedInstigator.IsValid());
    Taming->OnTameResolved(true, ETamedRole::Mount);
    TestEqual(TEXT("A result without a valid tamer cannot tame"), Taming->GetTameState(), ETameState::Wild);
    Taming->SetTamedRole(ETamedRole::Companion);
    TestEqual(TEXT("Unsupported companion role falls back to allowed mount"), Taming->GetTamedRole(), ETamedRole::Mount);
    Taming->SetTamedRole(ETamedRole::None);
    TestEqual(TEXT("Role can still be cleared"), Taming->GetTamedRole(), ETamedRole::None);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPangeaTamingInventoryTest, "Pangea.ACF.Taming.ConsumesGUIDItemsOnce",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPangeaTamingInventoryTest::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    if (!TestNotNull(TEXT("Temporary test world"), World)) return false;
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    APawn* Tamer = World->SpawnActor<APawn>();
    AActor* Creature = World->SpawnActor<AActor>();
    if (!TestNotNull(TEXT("Tamer"), Tamer) || !TestNotNull(TEXT("Creature"), Creature)) return false;
    UACFInventoryComponent* Inventory = NewObject<UACFInventoryComponent>(Tamer);
    Tamer->AddInstanceComponent(Inventory);
    Inventory->RegisterComponent();
    Tamer->DispatchBeginPlay();
    Inventory->AddItemToInventoryByClass(UACFItem::StaticClass(), 3, false);
    if (!TestEqual(TEXT("Test inventory contains three items"), Inventory->GetTotalCountOfItemsByClass(UACFItem::StaticClass()), 3)) return false;

    UPangeaTamingComponent* Taming = NewObject<UPangeaTamingComponent>(Creature);
    Creature->AddInstanceComponent(Taming);
    Taming->TameSpeciesConfig = NewObject<UTameSpeciesConfig>(Taming);
    Taming->TameSpeciesConfig->RequiredTamingItem = UACFItem::StaticClass();
    Taming->TameSpeciesConfig->RequiredItemCount = 2;
    Taming->TameSpeciesConfig->bCanBeCompanion = false;
    Taming->CachedInstigator = Tamer;
    Taming->OnTameResolved(true, ETamedRole::Mount);
    TestEqual(TEXT("Successful tame consumes GUID-identified inventory entries"), Inventory->GetTotalCountOfItemsByClass(UACFItem::StaticClass()), 1);
    TestEqual(TEXT("Successful tame sets state"), Taming->GetTameState(), ETameState::Tamed);
    Taming->OnTameResolved(true, ETamedRole::Mount);
    Taming->HandleLoadedActor();
    TestEqual(TEXT("Duplicate result and save restoration do not consume again"), Inventory->GetTotalCountOfItemsByClass(UACFItem::StaticClass()), 1);
    return true;
}
#endif
