#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Actions/UA_EnableFacility.h"
#include "Actions/UA_GrantInventory.h"
#include "Actors/FacilityGroup.h"
#include "Actors/VillageBase.h"
#include "ALSSaveTypes.h"
#include "Components/ACFInventoryComponent.h"
#include "Components/FacilityManagerComponent.h"
#include "Components/UpgradeSystemComponent.h"
#include "DataAssets/BaseUpgradeDefinition.h"
#include "Definitions/Fragments/BaseProgressionFragment.h"
#include "Definitions/Fragments/BaseSaveRulesFragment.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Objects/Requirements/Req_HasItems.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "UObject/Script.h"

namespace
{
struct FUpgradeFixture
{
    UWorld* World = UWorld::CreateWorld(EWorldType::EditorPreview, false);
    AVillageBase* Village = World ? World->SpawnActor<AVillageBase>() : nullptr;
    APawn* Pawn = World ? World->SpawnActor<APawn>() : nullptr;
    UPangeaUpgradeSystemComponent* Upgrades = nullptr;
    UPangeaFacilityManagerComponent* Facilities = nullptr;
    UACFInventoryComponent* Inventory = nullptr;
    UBaseUpgradeDefinition* Definition = nullptr;
    UBaseProgressionFragment* Progression = nullptr;
    AFacilityGroup* Group = nullptr;
    FGameplayTag Tag = FGameplayTag::RequestGameplayTag(TEXT("Test.Village.Blacksmith"));

    FUpgradeFixture()
    {
        if (!Village || !Pawn) return;
        Upgrades = NewObject<UPangeaUpgradeSystemComponent>(Village);
        Village->AddInstanceComponent(Upgrades);
        Facilities = NewObject<UPangeaFacilityManagerComponent>(Village);
        Village->AddInstanceComponent(Facilities);
        Definition = NewObject<UBaseUpgradeDefinition>(Upgrades);
        Progression = NewObject<UBaseProgressionFragment>(Definition);
        Definition->Fragments.Add(Progression);
        Upgrades->UpgradeDefinition = Definition;
        FUpgradeLevelDefinition Level;
        Level.Level = 1;
        FUpgradeMilestoneDefinition Milestone;
        Milestone.MilestoneTag = FGameplayTag::RequestGameplayTag(TEXT("Village.Milestone.Blacksmith"));
        Level.Milestones.Add(Milestone);
        Progression->Levels.Add(Level);
        Group = World->SpawnActor<AFacilityGroup>();
        if (Group)
        {
            Group->FacilityTag = Tag;
            Group->AttachToActor(Village, FAttachmentTransformRules::KeepRelativeTransform);
        }
        Facilities->DiscoverFacilityGroups();
        Inventory = NewObject<UACFInventoryComponent>(Pawn);
        Pawn->AddInstanceComponent(Inventory);
        Inventory->RegisterComponent();
        Pawn->DispatchBeginPlay();
    }
    ~FUpgradeFixture() { if (World) World->DestroyWorld(false); }
    FUpgradeMilestoneDefinition& Milestone() { return Progression->Levels[0].Milestones[0]; }
    UUA_EnableFacility* AddFacility(EFacilityEnableMode Mode = EFacilityEnableMode::EnableOnly)
    {
        UUA_EnableFacility* Action = NewObject<UUA_EnableFacility>(Definition);
        Action->FacilityTag = Tag;
        Action->Mode = Mode;
        Milestone().Actions.Add(Action);
        return Action;
    }
    UUA_GrantInventory* AddGrant(int32 Count)
    {
        UUA_GrantInventory* Action = NewObject<UUA_GrantInventory>(Definition);
        Action->ItemsToGrant.Add(FBaseItem(UACFItem::StaticClass(), Count));
        Milestone().Actions.Add(Action);
        return Action;
    }
    void Give(int32 Count) { Inventory->AddItemToInventoryByClass(UACFItem::StaticClass(), Count, false); }
    int32 Count() const { return Inventory->GetTotalCountOfItemsByClass(UACFItem::StaticClass()); }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPangeaUpgradeCostPolicyTest, "Pangea.ACF.Base.ExplicitCostsPreserveOwnershipGates",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPangeaUpgradeCostPolicyTest::RunTest(const FString& Parameters)
{
    FEditorScriptExecutionGuard ScriptGuard;
    FUpgradeFixture F;
    if (!TestNotNull(TEXT("Upgrade fixture"), F.Upgrades) || !TestNotNull(TEXT("ACF inventory"), F.Inventory)) return false;
    F.Give(5);
    if (!TestEqual(TEXT("Fixture inventory"), F.Count(), 5)) return false;
    UReq_HasItems* Gate = NewObject<UReq_HasItems>(F.Definition);
    FRequiredACFItem Required;
    Required.ItemClass = UACFItem::StaticClass(); Required.Count = 5;
    Gate->RequiredItems.Add(Required);
    F.Milestone().Requirements.Add(Gate);
    F.AddFacility();
    TestTrue(TEXT("Ownership-gated upgrade succeeds"), F.Upgrades->TryUpgradeChecked(F.Pawn).IsSuccess());
    TestEqual(TEXT("Existing requirements never become payment"), F.Count(), 5);
    TestTrue(TEXT("Facility actually enabled"), F.Facilities->IsFacilityEnabled(F.Tag));
    TestEqual(TEXT("Level commits after actions"), F.Upgrades->CurrentLevel, 1);

    FUpgradeFixture Paid;
    Paid.Give(5);
    FPangeaUpgradeItemCost Cost;
    Cost.ItemClass = UACFItem::StaticClass(); Cost.Count = 1;
    Paid.Progression->Levels[0].ItemCosts.Add(Cost);
    Cost.Count = 2;
    Paid.Progression->Levels[0].ItemCosts.Add(Cost); // duplicate classes must aggregate, not pass each in isolation
    Paid.AddFacility();
    TestTrue(TEXT("Separately authored payment upgrade succeeds"), Paid.Upgrades->TryUpgradeChecked(Paid.Pawn).IsSuccess());
    TestEqual(TEXT("Only explicit aggregate cost is consumed"), Paid.Count(), 2);
    Paid.Upgrades->LoadCompletedMilestones(Paid.Pawn);
    Paid.Upgrades->LoadCompletedMilestones(Paid.Pawn);
    TestEqual(TEXT("Repeated restore never recharges"), Paid.Count(), 2);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPangeaUpgradeFailureTest, "Pangea.ACF.Base.FailedPreflightNeverCommitsOrCharges",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPangeaUpgradeFailureTest::RunTest(const FString& Parameters)
{
    FEditorScriptExecutionGuard ScriptGuard;
    FUpgradeFixture F;
    if (!TestNotNull(TEXT("Upgrade fixture"), F.Upgrades)) return false;
    F.Give(3);
    FPangeaUpgradeItemCost Cost;
    Cost.ItemClass = UACFItem::StaticClass(); Cost.Count = 4;
    F.Progression->Levels[0].ItemCosts.Add(Cost);
    UUA_EnableFacility* Action = F.AddFacility();
    TestTrue(TEXT("Insufficient explicit payment is reported"), F.Upgrades->TryUpgradeChecked(F.Pawn).Result == EPangeaUpgradeResult::InsufficientItems);
    TestEqual(TEXT("Insufficient payment leaves inventory alone"), F.Count(), 3);
    TestFalse(TEXT("Preflight does not enable facility"), F.Facilities->IsFacilityEnabled(F.Tag));
    TestEqual(TEXT("Failed upgrade never commits level"), F.Upgrades->CurrentLevel, 0);
    TestTrue(TEXT("Failed preflight records no action receipts"), F.Upgrades->SuccessfulActionReceipts.IsEmpty());
    F.Progression->Levels[0].ItemCosts.Reset();
    Action->FacilityTag = FGameplayTag::RequestGameplayTag(TEXT("Test.Village.Mine"));
    TestTrue(TEXT("Missing concrete target is not success"), F.Upgrades->TryUpgradeChecked(F.Pawn).Result == EPangeaUpgradeResult::ActionNotReady);
    F.Upgrades->OnLevelIncreased(4, F.Pawn);
    TestEqual(TEXT("Legacy entry point cannot skip levels"), F.Upgrades->CurrentLevel, 0);
    F.Village->SetRole(ROLE_SimulatedProxy);
    TestTrue(TEXT("Client command rejected"), F.Upgrades->TryUpgradeChecked(F.Pawn).Result == EPangeaUpgradeResult::NotAuthority);
    F.Upgrades->MarkMilestoneCompleted(F.Milestone().MilestoneTag);
    TestTrue(TEXT("Client debug marking rejected"), F.Upgrades->CompletedMilestones.IsEmpty());
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPangeaUpgradeReplayTest, "Pangea.ACF.Base.LoadReconstructsStateWithoutRewards",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPangeaUpgradeReplayTest::RunTest(const FString& Parameters)
{
    FEditorScriptExecutionGuard ScriptGuard;
    FUpgradeFixture F;
    if (!TestNotNull(TEXT("Upgrade fixture"), F.Upgrades)) return false;
    F.AddFacility();
    F.AddGrant(2);
    TestTrue(TEXT("Initial upgrade succeeds"), F.Upgrades->TryUpgradeChecked(F.Pawn).IsSuccess());
    TestEqual(TEXT("Reward granted once"), F.Count(), 2);
    const int32 Receipts = F.Upgrades->SuccessfulActionReceipts.Num();
    TestEqual(TEXT("Two completed action receipts"), Receipts, 2);
    TestFalse(TEXT("No next level cannot re-grant"), F.Upgrades->TryUpgradeChecked(F.Pawn).IsSuccess());
    F.Facilities->DisableFacility(F.Tag);
    for (int32 Pass = 0; Pass < 2; ++Pass)
    {
        F.Upgrades->LoadCompletedMilestones(F.Village); // no interacting pawn during normal ALS load
        TestTrue(TEXT("Load restores absolute facility state"), F.Facilities->IsFacilityEnabled(F.Tag));
        TestEqual(TEXT("Load never dispatches inventory reward"), F.Count(), 2);
    }
    // Reverse authoring storage order; restoration still applies level 1 before level 2.
    FUpgradeLevelDefinition Level2;
    Level2.Level = 2;
    FUpgradeMilestoneDefinition Milestone2;
    Milestone2.MilestoneTag = FGameplayTag::RequestGameplayTag(TEXT("Village.Milestone.Mine"));
    UUA_EnableFacility* Disable = NewObject<UUA_EnableFacility>(F.Definition);
    Disable->FacilityTag = F.Tag; Disable->Mode = EFacilityEnableMode::DisableOnly;
    Milestone2.Actions.Add(Disable); Level2.Milestones.Add(Milestone2);
    F.Progression->Levels.Insert(Level2, 0);
    F.Upgrades->CurrentLevel = 2;
    F.Upgrades->CompletedMilestones.AddTag(Milestone2.MilestoneTag);
    F.Upgrades->LoadCompletedMilestones(F.Village);
    TestFalse(TEXT("Absolute states restore in ascending level order"), F.Facilities->IsFacilityEnabled(F.Tag));
    TArray<uint8> Bytes;
    { FMemoryWriter Writer(Bytes); FALSSaveGameArchive Save(Writer, true); F.Upgrades->Serialize(Save); }
    F.Upgrades->SuccessfulActionReceipts.Reset();
    { FMemoryReader Reader(Bytes); FALSSaveGameArchive Load(Reader, true); F.Upgrades->Serialize(Load); }
    F.Upgrades->OnComponentLoaded();
    TestEqual(TEXT("Action receipts survive SaveGame archive"), F.Upgrades->SuccessfulActionReceipts.Num(), Receipts);
    TestEqual(TEXT("Component load never grants again"), F.Count(), 2);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPangeaUpgradeRecoveryTest, "Pangea.ACF.Base.InterruptedCommandRequiresRecovery",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPangeaUpgradeRecoveryTest::RunTest(const FString& Parameters)
{
    FUpgradeFixture F;
    if (!TestNotNull(TEXT("Upgrade fixture"), F.Upgrades)) return false;
    F.AddFacility();
    TArray<uint8> CleanBackup;
    { FMemoryWriter Writer(CleanBackup); FALSSaveGameArchive Save(Writer, true); F.Upgrades->Serialize(Save); }
    F.Upgrades->PendingOperation = TEXT("Saved inside a one-time action callback");
    TArray<uint8> Bytes;
    { FMemoryWriter Writer(Bytes); FALSSaveGameArchive Save(Writer, true); F.Upgrades->Serialize(Save); }
    F.Upgrades->PendingOperation.Reset();
    { FMemoryReader Reader(Bytes); FALSSaveGameArchive Load(Reader, true); F.Upgrades->Serialize(Load); }
    F.Upgrades->OnComponentLoaded();
    TestTrue(TEXT("Interrupted save is fenced"), F.Upgrades->bUpgradeNeedsRecovery);
    TestTrue(TEXT("Blind retries rejected"), F.Upgrades->TryUpgradeChecked(F.Pawn).Result == EPangeaUpgradeResult::RecoveryRequired);
    TestEqual(TEXT("No false level commit"), F.Upgrades->CurrentLevel, 0);
    { FMemoryReader Reader(CleanBackup); FALSSaveGameArchive Load(Reader, true); F.Upgrades->Serialize(Load); }
    F.Upgrades->OnComponentLoaded();
    TestFalse(TEXT("Coherent backup clears failed live command state"), F.Upgrades->bUpgradeNeedsRecovery);
    TestTrue(TEXT("Coherent backup restores a ready command, without blind journal clearing"), F.Upgrades->CheckNextUpgrade(F.Pawn).IsSuccess());
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPangeaUpgradePartialFailureTest, "Pangea.ACF.Base.PartialActionFailureIsFenced",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPangeaUpgradePartialFailureTest::RunTest(const FString& Parameters)
{
    FEditorScriptExecutionGuard ScriptGuard;
    FUpgradeFixture F;
    if (!TestNotNull(TEXT("Upgrade fixture"), F.Upgrades) || !TestNotNull(TEXT("ACF inventory"), F.Inventory)) return false;
    F.Inventory->SetMaxInventorySlots(3);
    F.Give(1);
    F.AddGrant(2);
    F.AddGrant(2);
    FPangeaUpgradeItemCost Cost;
    Cost.ItemClass = UACFItem::StaticClass(); Cost.Count = 1;
    F.Progression->Levels[0].ItemCosts.Add(Cost);
    // Both individual grants fit at preflight, but together exceed capacity. The second
    // execution fails after the first succeeded: never pretend arbitrary effects rolled back.
    TestTrue(TEXT("Individual preflight is not an atomic inventory reservation"), F.Upgrades->CheckNextUpgrade(F.Pawn).IsSuccess());
    TestTrue(TEXT("Partial execution is reported explicitly"), F.Upgrades->TryUpgradeChecked(F.Pawn).Result == EPangeaUpgradeResult::RecoveryRequired);
    TestEqual(TEXT("Confirmed first action retained, no payment after failed action"), F.Count(), 3);
    TestEqual(TEXT("Only confirmed action has a receipt"), F.Upgrades->SuccessfulActionReceipts.Num(), 1);
    TestEqual(TEXT("Partial failure never commits a level"), F.Upgrades->CurrentLevel, 0);
    TestTrue(TEXT("Blind retry is fenced"), F.Upgrades->TryUpgradeChecked(F.Pawn).Result == EPangeaUpgradeResult::RecoveryRequired);
    TestEqual(TEXT("Retry cannot duplicate reward or charge cost"), F.Count(), 3);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPangeaUpgradeValidationTest, "Pangea.ACF.Base.RejectsAmbiguousConfiguration",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPangeaUpgradeValidationTest::RunTest(const FString& Parameters)
{
    FUpgradeFixture F;
    if (!TestNotNull(TEXT("Upgrade fixture"), F.Upgrades)) return false;
    UUA_EnableFacility* Action = F.AddFacility(EFacilityEnableMode::Toggle);
    TArray<FText> Errors;
    F.Definition->GetConfigurationErrors(Errors);
    TestFalse(TEXT("Toggle cannot silently replay"), Errors.IsEmpty());
    TestFalse(TEXT("Toggle is not reconstruction-safe"), Action->CanReconstructOnLoad());
    Action->Mode = EFacilityEnableMode::EnableOnly;
    UUA_EnableFacility* Duplicate = F.AddFacility();
    Action->ActionId = Duplicate->ActionId = TEXT("SameIdentity");
    F.Definition->GetConfigurationErrors(Errors);
    TestFalse(TEXT("Duplicate action identities rejected"), Errors.IsEmpty());
    F.Milestone().Actions.Pop();
    F.Definition->GetConfigurationErrors(Errors);
    TestTrue(TEXT("Valid fixture accepted"), Errors.IsEmpty());
    FUpgradeLevelDefinition DuplicateLevel = F.Progression->Levels[0];
    F.Progression->Levels.Add(DuplicateLevel);
    F.Definition->GetConfigurationErrors(Errors);
    TestFalse(TEXT("Duplicate level/milestone rejected"), Errors.IsEmpty());
    return !HasAnyErrors();
}
#endif
