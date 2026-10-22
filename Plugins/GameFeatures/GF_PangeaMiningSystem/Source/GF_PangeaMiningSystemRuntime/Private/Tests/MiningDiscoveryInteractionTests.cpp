#include "Actors/MiningDiscoveryNodeActor.h"
#include "Actors/MiningSiteActor.h"
#include "Actors/VillageBase.h"
#include "Actors/FacilityGroup.h"
#include "Components/FacilityManagerComponent.h"
#include "Components/ACFInteractionComponent.h"
#include "Components/UpgradeSystemComponent.h"
#include "DataAssets/BaseUpgradeDefinition.h"
#include "DataAssets/MiningSiteDefinition.h"
#include "Engine/World.h"
#include "Engine/EngineBaseTypes.h"
#include "GameFramework/Pawn.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPangeaMiningDiscoveryInteractionTest,
    "Pangea.ACF.Mining.VillageUnlockShowsEstablishInteraction",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPangeaMiningDiscoveryInteractionTest::RunTest(const FString& Parameters)
{
    UBaseUpgradeDefinition* VillageDefinition = LoadObject<UBaseUpgradeDefinition>(nullptr,
        TEXT("/Game/_Game/BaseUpgradeSystem/BUD_Village.BUD_Village"));
    UMiningSiteDefinition* SiteDefinition = LoadObject<UMiningSiteDefinition>(nullptr,
        TEXT("/Game/_Game/MiningSystem/DA_MiningSite_Iron_Test.DA_MiningSite_Iron_Test"));
    if (!TestNotNull(TEXT("Configured village definition"), VillageDefinition) ||
        !TestNotNull(TEXT("Configured mining site definition"), SiteDefinition)) return false;
    TArray<FText> ConfigurationErrors;
    VillageDefinition->GetConfigurationErrors(ConfigurationErrors);
    if (!TestTrue(TEXT("Migrated village definition has valid checked actions"), ConfigurationErrors.IsEmpty())) return false;

    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    if (!TestNotNull(TEXT("Isolated test world"), World)) return false;
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    // Actor::ProcessEvent suppresses interface dispatch in uninitialized worlds.
    World->InitializeActorsForPlay(FURL());
    AVillageBase* Village = World->SpawnActor<AVillageBase>();
    AMiningDiscoveryNodeActor* Node = World->SpawnActor<AMiningDiscoveryNodeActor>();
    APawn* Pawn = World->SpawnActor<APawn>();
    if (!TestNotNull(TEXT("Village"), Village) || !TestNotNull(TEXT("Discovery node"), Node) ||
        !TestNotNull(TEXT("Interacting pawn"), Pawn)) return false;

    Node->bUnlocked = false;
    Node->FacilityTag = FGameplayTag::RequestGameplayTag(TEXT("Test.Village.Mine"));
    Node->SiteDefinition = SiteDefinition;
    Node->MiningSiteActorClass = AMiningSiteActor::StaticClass();
    UPangeaUpgradeSystemComponent* Upgrades = NewObject<UPangeaUpgradeSystemComponent>(Village);
    Village->AddInstanceComponent(Upgrades);
    Upgrades->UpgradeDefinition = VillageDefinition;

    // Real checked actions require a concrete target, not the old silent missing-manager no-op.
    UPangeaFacilityManagerComponent* Facilities = NewObject<UPangeaFacilityManagerComponent>(Village);
    Village->AddInstanceComponent(Facilities);
    AFacilityGroup* Blacksmith = World->SpawnActor<AFacilityGroup>();
    if (!TestNotNull(TEXT("Concrete blacksmith facility"), Blacksmith)) return false;
    Blacksmith->FacilityTag = FGameplayTag::RequestGameplayTag(TEXT("Test.Village.Blacksmith"));
    Blacksmith->AttachToActor(Village, FAttachmentTransformRules::KeepRelativeTransform);
    Facilities->DiscoverFacilityGroups();
    TArray<FGameplayTag> AdvertisedUnlocks;
    Upgrades->GetFacilitiesUnlockedAtLevel(2, AdvertisedUnlocks);
    TestTrue(TEXT("Mining action advertises its logical unlock without a phantom facility action"), AdvertisedUnlocks.Contains(Node->FacilityTag));

    UACFInteractionComponent* Interaction = NewObject<UACFInteractionComponent>(Pawn);
    Pawn->AddInstanceComponent(Interaction);
    Interaction->RegisterComponent();
    Pawn->DispatchBeginPlay();
    if (!TestTrue(TEXT("ACF interaction component has begun play"), Interaction->HasBegunPlay())) return false;
    const FObjectPropertyBase* PawnOwnerProperty = FindFProperty<FObjectPropertyBase>(Interaction->GetClass(), TEXT("PawnOwner"));
    if (!TestTrue(TEXT("ACF caches the test pawn as interaction owner"), PawnOwnerProperty &&
        PawnOwnerProperty->GetObjectPropertyValue_InContainer(Interaction) == Pawn)) return false;
    Interaction->RegisterInteractable(Node);
    TestFalse(TEXT("Locked discovery is not offered by ACF"), Interaction->HasValidInteractable());
    TestTrue(TEXT("Locked discovery has no establish prompt"), Node->GetInteractableName_Implementation().IsEmpty());

    Upgrades->OnLevelIncreased(1, Pawn);
    TestEqual(TEXT("First checked level commits"), Upgrades->CurrentLevel, 1);
    TestFalse(TEXT("Blacksmith milestone does not unlock the mine"), Node->bUnlocked);
    Upgrades->OnLevelIncreased(2, Pawn);
    TestEqual(TEXT("Second checked level commits"), Upgrades->CurrentLevel, 2);
    TestTrue(TEXT("Mine milestone executes the matching unlock action"), Node->bUnlocked);
    TestTrue(TEXT("Discovery exposes the ACF interactable interface"), Node->GetClass()->ImplementsInterface(UACFInteractableInterface::StaticClass()));
    TestTrue(TEXT("ACF interface permits the unlocked discovery"), IACFInteractableInterface::Execute_CanBeInteracted(Node, Pawn));
    const FArrayProperty* CandidatesProperty = FindFProperty<FArrayProperty>(Interaction->GetClass(), TEXT("interactables"));
    if (CandidatesProperty)
    {
        const FScriptArrayHelper Candidates(CandidatesProperty, CandidatesProperty->ContainerPtrToValuePtr<void>(Interaction));
        TestTrue(TEXT("ACF retains the registered discovery candidate"), Candidates.Num() > 0);
    }
    Interaction->RefreshInteractions();
    TestTrue(TEXT("Unlocked discovery becomes the ACF interaction target"), Interaction->GetCurrentBestInteractableActor() == Node);
    TestEqual(TEXT("Establish prompt is restored"), Node->GetInteractableName_Implementation().ToString(), FString(TEXT("Establish Mining Site")));

    Interaction->Interact();
    TestNotNull(TEXT("ACF interaction establishes the mining site"), Node->EstablishedSite.Get());
    TestFalse(TEXT("An established mine cannot be established again"), Node->CanSetUpSite());
    return true;
}
#endif
