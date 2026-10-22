#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "ALSSaveTypes.h"
#include "Actors/MiningSiteActor.h"
#include "Actors/ACFCharacter.h"
#include "Components/ACFStorageComponent.h"
#include "Components/MiningSiteComponent.h"
#include "Components/PangeaMiningTransportComponent.h"
#include "Components/PangeaMiningConvoyComponent.h"
#include "Components/PangeaMiningTransportStorageComponent.h"
#include "Components/SceneComponent.h"
#include "DataAssets/MiningSiteDefinition.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Items/ACFItem.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"

namespace
{
struct FTransportFixture
{
	UWorld* World = UWorld::CreateWorld(EWorldType::EditorPreview, false);
	AMiningSiteActor* Actor = nullptr;
	UMiningSiteComponent* Site = nullptr;
	UPangeaMiningTransportComponent* Transport = nullptr;
	UACFStorageComponent* Destination = nullptr;
	FTransportFixture(float Risk = 0.f)
	{
		if (!World || !GEngine) return;
		GEngine->CreateNewWorldContext(EWorldType::EditorPreview).SetCurrentWorld(World);
		Actor = World->SpawnActor<AMiningSiteActor>();
		if (!Actor) return;
		Site = Actor->MiningSiteComponent;
		Transport = Actor->TransportComponent;
		Site->SiteDefinition = NewObject<UMiningSiteDefinition>(Site);
		Site->SiteDefinition->bUseHybridTransport = true;
		FMiningSiteLevelDefinition Level;
		Level.StorageCapacity = 100;
		Level.bShipmentUnlocked = true;
		Level.MaxShipmentUnits = 5;
		Level.ShipmentIntervalSeconds = 0.f;
		Level.TransportTravelSpeed = 100.f;
		Level.ShipmentLossChance = Risk;
		Level.PrimaryOutput.ItemClass = UACFItem::StaticClass();
		Site->SiteDefinition->Levels.Add(Level);
		Site->EstablishSite();
		FMiningItemQuantity Seed = Level.PrimaryOutput;
		Seed.Quantity = 10;
		Site->AddStoredItem(Seed);
		AActor* Stockpile = World->SpawnActor<AActor>();
		USceneComponent* Root = NewObject<USceneComponent>(Stockpile);
		Stockpile->AddInstanceComponent(Root);
		Stockpile->SetRootComponent(Root);
		Root->RegisterComponent();
		Stockpile->SetActorLocation(FVector(1000, 0, 0));
		Destination = NewObject<UPangeaMiningTransportStorageComponent>(Stockpile);
		Stockpile->AddInstanceComponent(Destination);
		Destination->SetMaxInventorySlots(20);
		Destination->SetMaxInventoryWeight(10000.f);
		Actor->SettlementResourceActor = Stockpile;
	}
	~FTransportFixture()
	{
		if (GEngine && World) GEngine->DestroyWorldContext(World);
		if (World) World->DestroyWorld(false);
	}
	void Step(float Seconds) { Transport->TickComponent(Seconds, LEVELTICK_All, nullptr); }
	int32 Delivered() const { return Destination->GetTotalCountOfItemsByClass(UACFItem::StaticClass()); }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningEscrowTest, "Pangea.ACF.Mining.Transport.EscrowAndArrival",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningEscrowTest::RunTest(const FString& Parameters)
{
	FTransportFixture F;
	if (!TestNotNull(TEXT("Transport"), F.Transport)) return false;
	TestTrue(TEXT("Dispatch succeeds through the single mining entry point"), F.Site->TryAutoShipment());
	TestEqual(TEXT("Exactly one source withdrawal"), F.Site->GetStoredUnits(), 5);
	TestEqual(TEXT("Cargo held in escrow"), F.Transport->GetShipment().Cargo.Quantity, 5);
	TestEqual(TEXT("No instant destination grant"), F.Delivered(), 0);
	TestFalse(TEXT("Repeated dispatch cannot create another manifest"), F.Site->TryAutoShipment());
	F.Step(5.f);
	TestEqual(TEXT("No early arrival grant"), F.Delivered(), 0);
	F.Step(5.f);
	TestEqual(TEXT("Actual arrival delivers once"), F.Delivered(), 5);
	F.Step(0.f);
	TestEqual(TEXT("Repeated same-time update cannot duplicate cargo"), F.Delivered(), 5);
	TestTrue(TEXT("Courier returns before another shipment"), F.Transport->GetShipment().Phase == EPangeaMiningShipmentPhase::Returning);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningDestinationAcceptanceTest, "Pangea.ACF.Mining.Transport.RejectedCargoStaysInEscrow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningDestinationAcceptanceTest::RunTest(const FString& Parameters)
{
	FTransportFixture F;
	if (!TestNotNull(TEXT("Transport"), F.Transport)) return false;
	F.Destination->SetMaxInventoryWeight(0.f);
	TestTrue(TEXT("Dispatch"), F.Transport->TryDispatch());
	F.Step(10.f);
	TestEqual(TEXT("Rejected cargo retained, not deleted"), F.Transport->GetShipment().Cargo.Quantity, 5);
	TestEqual(TEXT("No false delivery count"), F.Transport->GetShipment().DeliveredUnits, 0);
	F.Destination->SetMaxInventoryWeight(10000.f);
	F.Step(1.f);
	TestEqual(TEXT("Retry delivers only actual retained cargo"), F.Delivered(), 5);
	TestEqual(TEXT("Source + destination + escrow conserved"), F.Site->GetStoredUnits() + F.Delivered() + F.Transport->GetShipment().Cargo.Quantity, 10);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningTransportSnapshotTest, "Pangea.ACF.Mining.Transport.PauseOfflineSnapshot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningTransportSnapshotTest::RunTest(const FString& Parameters)
{
	FTransportFixture F;
	if (!TestNotNull(TEXT("Transport"), F.Transport)) return false;
	F.Transport->TryDispatch();
	F.Step(5.f);
	const FPangeaMiningShipment Saved = F.Transport->GetShipment();
	TArray<uint8> Bytes;
	FMemoryWriter Writer(Bytes);
	FALSSaveGameArchive Save(Writer, true);
	F.Transport->Serialize(Save);
	F.Step(1.f);
	F.World->TimeSeconds = 90000.f;
	FMemoryReader Reader(Bytes);
	FALSSaveGameArchive Load(Reader, true);
	F.Transport->Serialize(Load);
	F.Transport->OnComponentLoaded();
	F.Transport->OnComponentLoaded();
	TestTrue(TEXT("Manifest ID survives duplicate load callbacks"), F.Transport->GetShipment().Id == Saved.Id);
	TestTrue(TEXT("Saved online position retained; no offline movement"), F.Transport->GetShipment().Position.Equals(Saved.Position));
	TestEqual(TEXT("No delivery on load"), F.Delivered(), 0);
	F.Step(5.f);
	TestEqual(TEXT("Remaining online journey delivers exactly once"), F.Delivered(), 5);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningMaterializationGateTest, "Pangea.ACF.Mining.Transport.VisibleTripNeedsPhysicalCourier",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningMaterializationGateTest::RunTest(const FString& Parameters)
{
	FTransportFixture F;
	if (!TestNotNull(TEXT("Transport"), F.Transport)) return false;
	F.Transport->TryDispatch();
	APlayerController* PC = F.World->SpawnActor<APlayerController>();
	APawn* Pawn = F.World->SpawnActor<APawn>();
	if (!TestNotNull(TEXT("Observer"), PC) || !TestNotNull(TEXT("Observer pawn"), Pawn)) return false;
	F.World->AddController(PC);
	PC->Possess(Pawn);
	TestTrue(TEXT("Nearby player requests physical presentation"), F.Transport->ShouldMaterializeCourier());
	F.Step(20.f);
	TestEqual(TEXT("Missing physical navigation cannot invisibly grant cargo"), F.Delivered(), 0);
	PC->UnPossess();
	F.Step(10.f);
	TestEqual(TEXT("Offscreen travel can resume from retained ledger"), F.Delivered(), 5);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningTransportRosterSnapshotTest, "Pangea.ACF.Mining.Transport.ManifestRosterSnapshot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningTransportRosterSnapshotTest::RunTest(const FString& Parameters)
{
	FTransportFixture F;
	if (!TestNotNull(TEXT("Transport"), F.Transport)) return false;
	F.Site->SiteDefinition->Levels[0].TransportEscortCount = 2;
	F.Site->SiteDefinition->Levels[0].CourierClass = AACFCharacter::StaticClass();
	F.Site->SiteDefinition->Levels[0].TransportChariotClass = AActor::StaticClass();
	F.Site->SiteDefinition->Levels[0].TransportDraftAnimalClass = AACFCharacter::StaticClass();
	TestTrue(TEXT("Dispatch"), F.Transport->TryDispatch());
	const FGuid Id = F.Transport->GetShipment().Id;
	F.Site->SiteDefinition->Levels[0].TransportEscortCount = 0;
	F.Site->SiteDefinition->Levels[0].TransportTravelSpeed = 999.f;
	F.Site->SiteDefinition->Levels[0].TransportDraftAnimalClass.Reset();
	TestEqual(TEXT("Upgrade cannot remove the active roster"), F.Transport->GetShipment().EscortCount, 2);
	TestEqual(TEXT("Upgrade cannot change active journey speed"), F.Transport->GetShipment().Speed, 100.f);
	TestTrue(TEXT("Upgrade cannot replace active draft animal"), F.Transport->GetShipment().DraftAnimalClass.Get() == AACFCharacter::StaticClass());
	TestFalse(TEXT("Foreign manifest cannot kill an escort"), F.Transport->RecordEscortLoss(FGuid::NewGuid(), 0));
	TestFalse(TEXT("Invalid slot rejected"), F.Transport->RecordEscortLoss(Id, 2));
	TestTrue(TEXT("Real escort death recorded"), F.Transport->RecordEscortLoss(Id, 1));
	TestFalse(TEXT("Duplicate death idempotent"), F.Transport->RecordEscortLoss(Id, 1));
	TArray<uint8> Bytes;
	FMemoryWriter Writer(Bytes);
	FALSSaveGameArchive Save(Writer, true);
	F.Transport->Serialize(Save);
	FMemoryReader Reader(Bytes);
	FALSSaveGameArchive Load(Reader, true);
	F.Transport->Serialize(Load);
	F.Transport->OnComponentLoaded();
	TestTrue(TEXT("Lost slot survives load; rematerialization cannot resurrect it"), F.Transport->GetShipment().LostEscortSlots == TArray<int32>{1});
	TestEqual(TEXT("Death/load never grants cargo"), F.Delivered(), 0);
	TestTrue(TEXT("Draft animal configuration survives ALS archive"), F.Transport->GetShipment().DraftAnimalClass.Get() == AACFCharacter::StaticClass());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningDraftRequiresCartTest, "Pangea.ACF.Mining.Transport.DraftRequiresCart",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningDraftRequiresCartTest::RunTest(const FString& Parameters)
{
	FTransportFixture F;
	if (!TestNotNull(TEXT("Transport"), F.Transport)) return false;
	F.Site->SiteDefinition->Levels[0].TransportDraftAnimalClass = AACFCharacter::StaticClass();
	TestFalse(TEXT("Draft animal without cart cannot withdraw cargo"), F.Transport->TryDispatch());
	TestEqual(TEXT("Invalid convoy leaves source inventory intact"), F.Site->GetStoredUnits(), 10);
	TestFalse(TEXT("No invalid manifest"), F.Transport->HasActiveShipment());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningTransportVisibilitySweepTest, "Pangea.ACF.Mining.Transport.VisibilityCrossingStopsLongFrame",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningTransportVisibilitySweepTest::RunTest(const FString& Parameters)
{
	FTransportFixture F;
	if (!TestNotNull(TEXT("Transport"), F.Transport)) return false;
	F.Transport->MaterializationRadius = 100.f;
	F.Transport->TryDispatch();
	APlayerController* PC = F.World->SpawnActor<APlayerController>();
	APawn* Pawn = F.World->SpawnActor<APawn>();
	if (!PC || !Pawn) return false;
	USceneComponent* Root = NewObject<USceneComponent>(Pawn);
	Pawn->AddInstanceComponent(Root);
	Pawn->SetRootComponent(Root);
	Root->RegisterComponent();
	Pawn->SetActorLocation(FVector(500, 0, 0));
	F.World->AddController(PC);
	PC->Possess(Pawn);
	TestFalse(TEXT("Trip begins offscreen"), F.Transport->ShouldMaterializeCourier());
	F.Step(20.f);
	TestEqual(TEXT("Long frame cannot invisibly cross observer and deliver"), F.Delivered(), 0);
	TestTrue(TEXT("Stops at first visibility boundary"), F.Transport->GetShipment().Position.X >= 400. && F.Transport->GetShipment().Position.X < 401.);
	TestTrue(TEXT("Broker is requested at crossing"), F.Transport->ShouldMaterializeCourier());
	PC->UnPossess();
	F.Step(20.f);
	TestEqual(TEXT("Unobserved remaining route can complete"), F.Delivered(), 5);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningTransportBrokerFailureTest, "Pangea.ACF.Mining.Transport.MissingPresentationRetainsEscrow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningTransportBrokerFailureTest::RunTest(const FString& Parameters)
{
	FTransportFixture F;
	if (!TestNotNull(TEXT("Convoy"), F.Actor ? F.Actor->ConvoyComponent.Get() : nullptr)) return false;
	F.Transport->TryDispatch();
	APlayerController* PC = F.World->SpawnActor<APlayerController>();
	APawn* Pawn = F.World->SpawnActor<APawn>();
	if (!PC || !Pawn) return false;
	F.World->AddController(PC);
	PC->Possess(Pawn);
	F.Actor->ConvoyComponent->TickComponent(.25f, LEVELTICK_All, nullptr);
	TestNull(TEXT("No fallback fake courier without configuration/nav"), F.Actor->ConvoyComponent->GetCourier());
	TestFalse(TEXT("Failure is diagnosable"), F.Actor->ConvoyComponent->GetBlockedReason().IsEmpty());
	F.Actor->ConvoyComponent->ClearPhysicalConvoy();
	F.Actor->ConvoyComponent->ClearPhysicalConvoy();
	TestEqual(TEXT("Repeated rollback/cull retains escrow"), F.Transport->GetShipment().Cargo.Quantity, 5);
	TestEqual(TEXT("No destination grant"), F.Delivered(), 0);
	TestEqual(TEXT("Exactly one source withdrawal"), F.Site->GetStoredUnits(), 5);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningTransportActorRestoreOrderTest, "Pangea.ACF.Mining.Transport.ComponentBeforeActorRestoreOrder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningTransportActorRestoreOrderTest::RunTest(const FString& Parameters)
{
	FTransportFixture F;
	if (!TestNotNull(TEXT("Transport"), F.Transport)) return false;
	F.Transport->TryDispatch();
	F.Step(2.f);
	const FPangeaMiningShipment Saved = F.Transport->GetShipment();
	TArray<uint8> ActorBytes, ComponentBytes;
	FMemoryWriter ActorWriter(ActorBytes), ComponentWriter(ComponentBytes);
	FALSSaveGameArchive ActorSave(ActorWriter, true), ComponentSave(ComponentWriter, true);
	F.Actor->Serialize(ActorSave);
	F.Transport->Serialize(ComponentSave);
	for (int32 Repeat = 0; Repeat < 2; ++Repeat)
	{
		FMemoryReader ComponentReader(ComponentBytes), ActorReader(ActorBytes);
		FALSSaveGameArchive ComponentLoad(ComponentReader, true), ActorLoad(ActorReader, true);
		F.Transport->Serialize(ComponentLoad); // ALS restores components FIRST.
		F.Actor->Serialize(ActorLoad);
		F.Transport->OnComponentLoaded();
		TestTrue(TEXT("Actor restore must preserve component-restored manifest"), F.Transport->GetShipment().Id == Saved.Id);
		TestEqual(TEXT("Actor restore must not clear restored cargo"), F.Transport->GetShipment().Cargo.Quantity, 5);
		TestTrue(TEXT("Saved online position retained"), F.Transport->GetShipment().Position.Equals(Saved.Position));
	}
	FMemoryReader LegacyReader(ActorBytes);
	FALSSaveGameArchive LegacyLoad(LegacyReader, true);
	F.Actor->Serialize(LegacyLoad); // No component record: legacy reset still required.
	F.Transport->OnComponentLoaded();
	TestFalse(TEXT("Legacy actor-only restore cannot retain an unrelated live shipment"), F.Transport->HasActiveShipment());
	TestEqual(TEXT("No delivery on any restore"), F.Delivered(), 0);
	return true;
}
#endif
