#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "ALSFunctionLibrary.h"
#include "ALSLoadAndSaveComponent.h"
#include "ALSSaveGameSettings.h"
#include "ALSSaveTypes.h"
#include "Components/ACFStorageComponent.h"
#include "Components/MiningSiteComponent.h"
#include "DataAssets/MiningSiteDefinition.h"
#include "Engine/World.h"
#include "Items/ACFItem.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "UObject/UnrealType.h"
#include <limits>

namespace
{
struct FMiningTimingFixture
{
	// Test-only clock control in an isolated preview world; no live PIE or wall-clock sleeps.
	UWorld* World = UWorld::CreateWorld(EWorldType::EditorPreview, false);
	~FMiningTimingFixture() { if (World) World->DestroyWorld(false); }
	UMiningSiteComponent* AddSite(bool bShipment = false, int32 Capacity = 1000)
	{
		AActor* Owner = World ? World->SpawnActor<AActor>() : nullptr;
		if (!Owner) return nullptr;
		UMiningSiteComponent* Site = NewObject<UMiningSiteComponent>(Owner, TEXT("MiningSite"));
		Owner->AddInstanceComponent(Site);
		UALSLoadAndSaveComponent* Save = NewObject<UALSLoadAndSaveComponent>(Owner);
		Owner->AddInstanceComponent(Save);
		Save->AddComponentClassToSave(UMiningSiteComponent::StaticClass());
		Site->SiteDefinition = NewObject<UMiningSiteDefinition>(Site);
		Site->SiteDefinition->SimulatedDaySeconds = 300.f;
		FMiningSiteLevelDefinition Level;
		Level.StorageCapacity = Capacity;
		Level.AutomatedMineralsPerDay = 50;
		Level.PrimaryOutput.ItemClass = UACFItem::StaticClass();
		Level.bShipmentUnlocked = bShipment;
		Level.MaxShipmentUnits = 5;
		Level.ShipmentIntervalSeconds = 60.f;
		Level.ShipmentLossChance = 0.f;
		Site->SiteDefinition->Levels.Add(Level);
		Site->EstablishSite();
		return Site;
	}
};

TArray<uint8> SaveComponent(UMiningSiteComponent* Site)
{
	Site->OnComponentSaved();
	TArray<uint8> Bytes;
	FMemoryWriter Writer(Bytes);
	FALSSaveGameArchive Archive(Writer, true);
	Site->Serialize(Archive);
	return Bytes;
}

bool LoadComponent(UMiningSiteComponent* Site, const TArray<uint8>& Bytes)
{
	FMemoryReader Reader(Bytes);
	FALSSaveGameArchive Archive(Reader, true);
	Site->Serialize(Archive);
	Site->OnComponentLoaded();
	return !Archive.IsError();
}

struct FLegacyMiningArchive : FALSSaveGameArchive
{
	FLegacyMiningArchive(FArchive& Inner) : FALSSaveGameArchive(Inner, true) {}
	virtual bool ShouldSkipProperty(const FProperty* Property) const override
	{
		const FName Name = Property->GetFName();
		return Name == TEXT("MiningTimingVersion") || Name == TEXT("ProductionRemainder") ||
			Name == TEXT("ShipmentCooldownRemaining") || FALSSaveGameArchive::ShouldSkipProperty(Property);
	}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningCadenceTest, "Pangea.ACF.Mining.CadenceInvariantFractionalProduction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningCadenceTest::RunTest(const FString& Parameters)
{
	FMiningTimingFixture Fixture;
	UMiningSiteComponent* Frequent = Fixture.AddSite();
	UMiningSiteComponent* Batched = Fixture.AddSite();
	if (!TestNotNull(TEXT("Frequent site"), Frequent) || !TestNotNull(TEXT("Batched site"), Batched)) return false;
	for (int32 Second = 1; Second <= 300; ++Second)
	{
		Fixture.World->TimeSeconds = Second;
		Frequent->SyncProductionFromWorldTime();
	}
	Batched->SyncProductionFromWorldTime();
	TestEqual(TEXT("300 one-second syncs produce 50"), Frequent->GetStoredUnits(), 50);
	TestEqual(TEXT("One 300-second sync produces 50"), Batched->GetStoredUnits(), 50);
	TestTrue(TEXT("No fractional residue at exact day boundary"), FMath::IsNearlyZero(Frequent->GetProductionRemainder(), 1.e-8));
	Frequent->SyncProductionFromWorldTime();
	TestEqual(TEXT("Repeated sync at the same clock does not produce twice"), Frequent->GetStoredUnits(), 50);
	Fixture.World->TimeSeconds = 303.0;
	Frequent->OnComponentSaved();
	TestTrue(TEXT("Half unit survives online accounting"), FMath::IsNearlyEqual(Frequent->GetProductionRemainder(), 0.5, 1.e-8));
	Frequent->OnStorageChanged.AddDynamic(Frequent, &UMiningSiteComponent::SyncProductionFromWorldTime);
	Fixture.World->TimeSeconds = 306.0;
	Frequent->SyncProductionFromWorldTime();
	TestEqual(TEXT("Storage delegate cannot re-account the same production"), Frequent->GetStoredUnits(), 51);
	Fixture.World->TimeSeconds = 1.0;
	Frequent->SyncProductionFromWorldTime();
	TestEqual(TEXT("Backward clock step never subtracts/adds stored output"), Frequent->GetStoredUnits(), 51);
	Fixture.World->TimeSeconds = 4.0;
	Frequent->OnComponentSaved();
	TestTrue(TEXT("Clock resumes against the new anchor"), FMath::IsNearlyEqual(Frequent->GetProductionRemainder(), 0.5, 1.e-8));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningOfflineTest, "Pangea.ACF.Mining.PauseOfflineALSRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningOfflineTest::RunTest(const FString& Parameters)
{
	FMiningTimingFixture Fixture;
	UMiningSiteComponent* Source = Fixture.AddSite(true);
	UMiningSiteComponent* Restored = Fixture.AddSite(true);
	if (!TestNotNull(TEXT("Source"), Source) || !TestNotNull(TEXT("Restored"), Restored)) return false;
	Fixture.World->TimeSeconds = 15.0;
	// Real ALS collection, reflected pre-save hook and component serialization.
	const FALSActorData Record = UALSFunctionLibrary::SerializeActor(Source->GetOwner());
	TestEqual(TEXT("ALS pre-save settles two whole units without shipping"), Source->GetStoredUnits(), 2);
	TestTrue(TEXT("ALS pre-save captures fractional unit"), FMath::IsNearlyEqual(Source->GetProductionRemainder(), 0.5, 1.e-8));
	Fixture.World->TimeSeconds = 100000.0; // Another session can have a much larger clock.
	UALSFunctionLibrary::FullDeserializeActor(Restored->GetOwner(), Record, false);
	TestEqual(TEXT("No offline production"), Restored->GetStoredUnits(), 2);
	TestTrue(TEXT("Cooldown has 45 seconds left, not an old session timestamp"), FMath::IsNearlyEqual(Restored->GetShipmentCooldownRemaining(), 45.0));
	Fixture.World->TimeSeconds += 3.0;
	Restored->OnComponentLoaded(); // Duplicate callback must not reset the active anchor.
	Restored->OnComponentSaved();
	TestEqual(TEXT("Saved half plus three online seconds produces one unit"), Restored->GetStoredUnits(), 3);
	TestTrue(TEXT("Duplicate restore did not freeze elapsed cooldown"), FMath::IsNearlyEqual(Restored->GetShipmentCooldownRemaining(), 42.0));
	Fixture.World->TimeSeconds = 0.0; // Smaller/new-session clock is equally valid.
	UALSFunctionLibrary::FullDeserializeActor(Restored->GetOwner(), Record, false);
	TestEqual(TEXT("Reload replaces inventory, it does not append it"), Restored->GetStoredUnits(), 2);
	Fixture.World->TimeSeconds = 3.0;
	Restored->OnComponentSaved();
	TestEqual(TEXT("Time zero is a valid initialized anchor"), Restored->GetStoredUnits(), 3);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningUpgradeTimingTest, "Pangea.ACF.Mining.UpgradeSettlesOldRate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningUpgradeTimingTest::RunTest(const FString& Parameters)
{
	FMiningTimingFixture Fixture;
	UMiningSiteComponent* Site = Fixture.AddSite();
	if (!TestNotNull(TEXT("Site"), Site)) return false;
	FMiningSiteLevelDefinition Next = Site->SiteDefinition->Levels[0];
	Next.Level = 1;
	Next.AutomatedMineralsPerDay = 100;
	Next.bShipmentUnlocked = true;
	Site->SiteDefinition->Levels.Add(Next);
	Next.Level = 2;
	Next.PrimaryOutput.ItemClass = UObject::StaticClass(); // Distinct private test resource.
	Site->SiteDefinition->Levels.Add(Next);
	Fixture.World->TimeSeconds = 15.0;
	TestTrue(TEXT("Upgrade succeeds"), Site->UpgradeSite());
	TestEqual(TEXT("Old rate produces two units, not five"), Site->GetStoredUnits(), 2);
	TestTrue(TEXT("Same output retains half-unit progress"), FMath::IsNearlyEqual(Site->GetProductionRemainder(), 0.5, 1.e-8));
	TestTrue(TEXT("Shipment unlock starts a full cooldown"), FMath::IsNearlyEqual(Site->GetShipmentCooldownRemaining(), 60.0));
	Fixture.World->TimeSeconds = 16.5;
	Site->OnComponentSaved();
	TestEqual(TEXT("New rate completes the saved half-unit"), Site->GetStoredUnits(), 3);
	Site->ProduceForElapsedSeconds(1.5f);
	TestTrue(TEXT("Half unit before resource change"), FMath::IsNearlyEqual(Site->GetProductionRemainder(), 0.5, 1.e-8));
	TestTrue(TEXT("Second upgrade succeeds"), Site->UpgradeSite());
	TestTrue(TEXT("A fraction cannot change resource type"), FMath::IsNearlyZero(Site->GetProductionRemainder()));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningCapacityTest, "Pangea.ACF.Mining.CapacityAndInvalidElapsed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningCapacityTest::RunTest(const FString& Parameters)
{
	FMiningTimingFixture Fixture;
	UMiningSiteComponent* Site = Fixture.AddSite(false, 7);
	if (!TestNotNull(TEXT("Site"), Site)) return false;
	TestEqual(TEXT("Long elapsed saturates capacity, no integer overflow"), Site->ProduceForElapsedSeconds(std::numeric_limits<float>::max()), 7);
	TestEqual(TEXT("Full storage rejects more output"), Site->ProduceForElapsedSeconds(300.f), 0);
	TestTrue(TEXT("Full storage does not bank fractional backlog"), FMath::IsNearlyZero(Site->GetProductionRemainder()));
	FMiningItemQuantity Item = Site->SiteDefinition->Levels[0].PrimaryOutput;
	Item.Quantity = 7;
	TestEqual(TEXT("Remove stored output"), Site->RemoveStoredItem(Item), 7);
	Site->ProduceForElapsedSeconds(3.f);
	TestEqual(TEXT("No deferred whole output after freeing space"), Site->GetStoredUnits(), 0);
	const double Before = Site->GetProductionRemainder();
	for (float Invalid : { -1.f, 0.f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN() })
		TestEqual(TEXT("Invalid elapsed does not produce"), Site->ProduceForElapsedSeconds(Invalid), 0);
	TestEqual(TEXT("Invalid elapsed preserves valid fraction"), Site->GetProductionRemainder(), Before);
	const TArray<uint8> Bytes = SaveComponent(Site);
	TestTrue(TEXT("Fraction archive loads"), LoadComponent(Site, Bytes));
	Site->ProduceForElapsedSeconds(3.f);
	TestEqual(TEXT("Fractional progress survives serialization"), Site->GetStoredUnits(), 1);
	TArray<uint8> DefaultBytes;
	{
		FMemoryWriter Writer(DefaultBytes);
		FALSSaveGameArchive Archive(Writer, true);
		const bool bOriginalDoDelta = Archive.DoDelta();
		Site->Serialize(Archive);
		TestEqual(TEXT("Component save restores the caller's archive delta policy"), Archive.DoDelta(), bOriginalDoDelta);
	}
	Site->ProduceForElapsedSeconds(3.f);
	TestTrue(TEXT("Reused component has a different fraction before loading"), FMath::IsNearlyEqual(Site->GetProductionRemainder(), 0.5, 1.e-8));
	TestTrue(TEXT("Default-valued timing archive loads"), LoadComponent(Site, DefaultBytes));
	TestTrue(TEXT("Saved zero replaces reused nonzero fraction"), FMath::IsNearlyZero(Site->GetProductionRemainder()));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningShipmentTimingTest, "Pangea.ACF.Mining.ShipmentCooldownAndReentry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningShipmentTimingTest::RunTest(const FString& Parameters)
{
	FMiningTimingFixture Fixture;
	UMiningSiteComponent* Site = Fixture.AddSite(true);
	if (!TestNotNull(TEXT("Site"), Site)) return false;
	Site->SiteDefinition->Levels[0].AutomatedMineralsPerDay = 0;
	Site->OnStorageChanged.AddDynamic(Site, &UMiningSiteComponent::SyncProductionFromWorldTime);
	FMiningItemQuantity Item = Site->SiteDefinition->Levels[0].PrimaryOutput;
	Item.Quantity = 20;
	Site->AddStoredItem(Item);
	TestFalse(TEXT("No early shipment, including establishment at time zero"), Site->TryAutoShipment());
	Fixture.World->TimeSeconds = 60.0;
	Site->OnComponentSaved();
	TestEqual(TEXT("Saving a due shipment does not remove/deliver items"), Site->GetStoredUnits(), 20);
	TestTrue(TEXT("One due shipment succeeds"), Site->TryAutoShipment());
	TestEqual(TEXT("Storage callback does not recursively ship"), Site->GetStoredUnits(), 15);
	TestFalse(TEXT("Cooldown prevents duplicate dispatch"), Site->TryAutoShipment());
	const TArray<uint8> Bytes = SaveComponent(Site);
	Fixture.World->TimeSeconds = 50000.0;
	TestTrue(TEXT("Shipment archive loads"), LoadComponent(Site, Bytes));
	TestFalse(TEXT("Offline time does not consume cooldown"), Site->TryAutoShipment());
	Fixture.World->TimeSeconds += 60.0;
	TestTrue(TEXT("Online cooldown completes after load"), Site->TryAutoShipment());
	TestEqual(TEXT("Exactly two shipments removed"), Site->GetStoredUnits(), 10);
	Site->SiteDefinition->Levels[0].ShipmentIntervalSeconds = 0.f;
	Fixture.World->TimeSeconds += 60.0;
	TestTrue(TEXT("Zero-interval config still dispatches only once per call"), Site->TryAutoShipment());
	TestEqual(TEXT("Even zero-interval reentry cannot drain everything"), Site->GetStoredUnits(), 5);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningLegacyTimingTest, "Pangea.ACF.Mining.LegacyTimingMigration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningLegacyTimingTest::RunTest(const FString& Parameters)
{
	FMiningTimingFixture Fixture;
	UMiningSiteComponent* Site = Fixture.AddSite(true);
	if (!TestNotNull(TEXT("Site"), Site)) return false;
	Fixture.World->TimeSeconds = 15.0;
	Site->OnComponentSaved();
	TArray<uint8> Bytes;
	{
		FMemoryWriter Writer(Bytes);
		FLegacyMiningArchive Legacy(Writer);
		Site->Serialize(Legacy);
	}
	Fixture.World->TimeSeconds = 1.0;
	AddExpectedError(TEXT("Legacy mining timing"), EAutomationExpectedErrorFlags::Contains, 1);
	TestTrue(TEXT("Legacy archive loads on a previously used component"), LoadComponent(Site, Bytes));
	TestEqual(TEXT("Existing whole inventory survives migration"), Site->GetStoredUnits(), 2);
	TestTrue(TEXT("Missing legacy fraction cannot retain newer runtime fraction"), FMath::IsNearlyZero(Site->GetProductionRemainder()));
	TestTrue(TEXT("Legacy shipment gets conservative full cooldown"), FMath::IsNearlyEqual(Site->GetShipmentCooldownRemaining(), 60.0));
	Site->OnComponentLoaded();
	Site->SyncProductionFromWorldTime();
	TestEqual(TEXT("Migration does not fabricate output"), Site->GetStoredUnits(), 2);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningLinkedStorageTest, "Pangea.ACF.Mining.LinkedACFStorageAccounting",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningLinkedStorageTest::RunTest(const FString& Parameters)
{
	FMiningTimingFixture Fixture;
	UMiningSiteComponent* Site = Fixture.AddSite();
	if (!TestNotNull(TEXT("Site"), Site)) return false;
	UClass* MetalClass = LoadClass<UACFItem>(nullptr, TEXT("/Game/FullSample/Blueprints/Items/Materials/ACFMetalBP.ACFMetalBP_C"));
	if (!TestNotNull(TEXT("Existing ACF metal item"), MetalClass)) return false;
	FMiningItemQuantity BaseItem = Site->SiteDefinition->Levels[0].PrimaryOutput;
	BaseItem.Quantity = 3; // Native ACFItem has stack size one; exercise multiple stacks.
	FMiningItemQuantity Metal;
	Metal.ItemClass = MetalClass;
	Metal.Quantity = 2;
	Site->AddStoredItem(BaseItem);
	Site->AddStoredItem(Metal);
	UACFStorageComponent* Storage = NewObject<UACFStorageComponent>(Site->GetOwner());
	Site->GetOwner()->AddInstanceComponent(Storage);
	Storage->SetMaxInventoryWeight(10000);
	Storage->SetMaxInventorySlots(20);
	Site->SetLinkedStorageComponent(Storage);
	TestEqual(TEXT("All saved resources survive synchronous chest callbacks"), Site->GetStoredUnits(), 5);
	TestEqual(TEXT("Quantity sums every ACF stack"), Site->GetStoredQuantity(BaseItem), 3);
	BaseItem.Quantity = 2;
	TestEqual(TEXT("Reports real ACF acceptance"), Site->AddStoredItem(BaseItem), 2);
	BaseItem.Quantity = 4;
	TestEqual(TEXT("Removal spans multiple ACF stacks"), Site->RemoveStoredItem(BaseItem), 4);
	TestEqual(TEXT("Mirror matches remaining chest units"), Site->GetStoredUnits(), 3);
	Storage->SetMaxInventoryWeight(0);
	TestEqual(TEXT("ACF rejection is not falsely reported as production"), Site->ProduceForElapsedSeconds(6.f), 0);
	TestEqual(TEXT("Rejected output does not appear in mirror"), Site->GetStoredUnits(), 3);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningTimingValidationTest, "Pangea.ACF.Mining.TimingDefinitionValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningTimingValidationTest::RunTest(const FString& Parameters)
{
	UMiningSiteDefinition* Existing = LoadObject<UMiningSiteDefinition>(nullptr,
		TEXT("/Game/_Game/MiningSystem/DA_MiningSite_Iron_Test.DA_MiningSite_Iron_Test"));
	if (!TestNotNull(TEXT("Current iron definition"), Existing)) return false;
	FDataValidationContext CurrentContext;
	TestTrue(TEXT("Existing balancing passes validation unchanged"), Existing->IsDataValid(CurrentContext) == EDataValidationResult::Valid);
	FMiningTimingFixture Fixture;
	UMiningSiteComponent* Site = Fixture.AddSite();
	if (!TestNotNull(TEXT("Private validation fixture"), Site)) return false;
	Site->SiteDefinition->PresentationConfig = Existing->PresentationConfig;
	FDataValidationContext ValidContext;
	TestTrue(TEXT("Valid private definition accepted"), Site->SiteDefinition->IsDataValid(ValidContext) == EDataValidationResult::Valid);
	Site->SiteDefinition->SimulatedDaySeconds = std::numeric_limits<float>::quiet_NaN();
	Site->SiteDefinition->Levels[0].ShipmentIntervalSeconds = -1.f;
	FDataValidationContext InvalidContext;
	TestTrue(TEXT("Invalid timing rejected before authoring"), Site->SiteDefinition->IsDataValid(InvalidContext) == EDataValidationResult::Invalid);
	TestEqual(TEXT("Runtime ignores non-finite production day"), Site->ProduceForElapsedSeconds(300.f), 0);
	TestFalse(TEXT("Runtime ignores invalid shipment interval"), Site->TryAutoShipment());
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningStateContractTest, "Pangea.ACF.Mining.PersistentReplicationContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningStateContractTest::RunTest(const FString& Parameters)
{
	for (const TCHAR* Name : { TEXT("bEstablished"), TEXT("CurrentLevel"), TEXT("StoredItems"), TEXT("SiteDefinition"),
		TEXT("ProductionRemainder"), TEXT("ShipmentCooldownRemaining") })
	{
		const FProperty* Property = FindFProperty<FProperty>(UMiningSiteComponent::StaticClass(), Name);
		if (!TestNotNull(Name, Property)) continue;
		TestTrue(FString(Name) + TEXT(" persists"), Property->HasAnyPropertyFlags(CPF_SaveGame));
		TestTrue(FString(Name) + TEXT(" replicates"), Property->HasAnyPropertyFlags(CPF_Net));
	}
	for (const TCHAR* Name : { TEXT("ItemClass"), TEXT("ItemTag"), TEXT("Quantity") })
	{
		const FProperty* Property = FindFProperty<FProperty>(FMiningStoredItem::StaticStruct(), Name);
		TestTrue(FString(Name) + TEXT(" persists within storage"), Property && Property->HasAnyPropertyFlags(CPF_SaveGame));
	}
	const UALSSaveGameSettings* Settings = GetDefault<UALSSaveGameSettings>();
	TestEqual(TEXT("ALS pre-save hook matches"), Settings->GetOnComponentSavedFunctionName(), FName(TEXT("OnComponentSaved")));
	TestEqual(TEXT("ALS post-load hook matches"), Settings->GetOnComponentLoadedFunctionName(), FName(TEXT("OnComponentLoaded")));
	return !HasAnyErrors();
}
#endif
