#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Actors/MiningSiteActor.h"
#include "AIController.h"
#include "Components/MiningSiteComponent.h"
#include "DataAssets/MiningSiteDefinition.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningRestoreOwnershipTest,
    "Pangea.ACF.Mining.RestoreCleanupRespectsSiteOwnership",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningRestoreOwnershipTest::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Editor, false);
    if (!TestNotNull(TEXT("Isolated ownership world"), World)) return false;
    GEngine->CreateNewWorldContext(EWorldType::Editor).SetCurrentWorld(World);
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    AMiningSiteActor* First = World->SpawnActor<AMiningSiteActor>();
    AMiningSiteActor* Second = World->SpawnActor<AMiningSiteActor>();
    if (!First || !Second) return false;
    First->MiningSiteComponent->SiteDefinition = NewObject<UMiningSiteDefinition>(First);
    First->MiningSiteComponent->SiteDefinition->SiteChestActorClass = AStaticMeshActor::StaticClass();
    FMiningSiteLevelDefinition Level;
    Level.WorkerClass = APawn::StaticClass();
    First->MiningSiteComponent->SiteDefinition->Levels = { Level };

    auto Chest = [World](AMiningSiteActor* Site)
    {
        AActor* Actor = World->SpawnActor<AStaticMeshActor>();
        Actor->SetOwner(Site);
        Actor->Tags.Add(FName(*FString::Printf(TEXT("Mining.SiteChest.%s"), *Site->GetName())));
        return Actor;
    };
    auto Worker = [World](AMiningSiteActor* Site)
    {
        APawn* Pawn = World->SpawnActor<APawn>();
        AAIController* Controller = World->SpawnActor<AAIController>();
        Controller->SetOwner(Site);
        Controller->Possess(Pawn); // Pawn.Owner is the controller, not the site.
        Pawn->Tags.Add(TEXT("Mining.Worker"));
        Pawn->Tags.Add(FName(*FString::Printf(TEXT("Mining.Site.%s"), *Site->GetName())));
        return Pawn;
    };
    AActor* OwnChest = Chest(First);
    AActor* OtherChest = Chest(Second);
    APawn* OwnWorker = Worker(First);
    APawn* OtherWorker = Worker(Second);
    AActor* UnownedChest = World->SpawnActor<AStaticMeshActor>();
    APawn* UnownedPawn = World->SpawnActor<APawn>();
    // Every actor is nearby and uses the same classes: proximity/class is not ownership.
    First->OnLoaded_Implementation();
    TestTrue(TEXT("Restoring a site removes its tagged chest"), OwnChest->IsActorBeingDestroyed());
    TestTrue(TEXT("Restoring a site removes its possessed, tagged worker"), OwnWorker->IsActorBeingDestroyed());
    TestFalse(TEXT("Another nearby site's chest survives"), OtherChest->IsActorBeingDestroyed());
    TestFalse(TEXT("Another nearby site's worker survives"), OtherWorker->IsActorBeingDestroyed());
    TestFalse(TEXT("Unowned same-class scenery is not a site chest"), UnownedChest->IsActorBeingDestroyed());
    TestFalse(TEXT("Unowned same-class pawn is not site workforce"), UnownedPawn->IsActorBeingDestroyed());
    First->OnLoaded_Implementation();
    TestFalse(TEXT("Repeated restore still preserves the other site's worker"), OtherWorker->IsActorBeingDestroyed());
    TestFalse(TEXT("Repeated restore still preserves the other site's chest"), OtherChest->IsActorBeingDestroyed());
    AActor* OwnedNonChestActor = World->SpawnActor<AActor>();
    OwnedNonChestActor->SetOwner(First);
    First->MiningSiteComponent->SiteDefinition->SiteChestActorClass.Reset();
    First->OnLoaded_Implementation();
    TestFalse(TEXT("Missing chest class cannot delete unrelated site-owned actors"), OwnedNonChestActor->IsActorBeingDestroyed());
    return !HasAnyErrors();
}
#endif
