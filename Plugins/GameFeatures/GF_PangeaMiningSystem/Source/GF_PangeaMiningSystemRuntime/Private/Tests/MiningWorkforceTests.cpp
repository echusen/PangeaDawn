#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "ALSSaveTypes.h"
#include "Components/ACFStorageComponent.h"
#include "Components/MiningSiteComponent.h"
#include "DataAssets/MiningSiteDefinition.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Items/ACFItem.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "UObject/UnrealType.h"

namespace
{
struct FMiningWorkforceFixture
{
	UWorld* World = UWorld::CreateWorld(EWorldType::EditorPreview, false);
	~FMiningWorkforceFixture() { if (World) World->DestroyWorld(false); }
	UMiningSiteComponent* AddSite(float Chance = 1.f, int32 Workers = 2, int32 Capacity = 1000)
	{
		AActor* Owner = World ? World->SpawnActor<AActor>() : nullptr;
		if (!Owner) return nullptr;
		UMiningSiteComponent* Site = NewObject<UMiningSiteComponent>(Owner);
		Owner->AddInstanceComponent(Site);
		Site->SiteDefinition = NewObject<UMiningSiteDefinition>(Site);
		Site->SiteDefinition->SimulatedDaySeconds = 100.f;
		FMiningSiteLevelDefinition Level;
		Level.StorageCapacity = Capacity;
		Level.AutomatedMineralsPerDay = 20;
		Level.WorkerCount = Workers;
		Level.GuardCount = 1;
		Level.WorkerDeathChance = Chance;
		Level.PrimaryOutput.ItemClass = UACFItem::StaticClass();
		Site->SiteDefinition->Levels.Add(Level);
		Site->EstablishSite();
		FindFProperty<FIntProperty>(Site->GetClass(), TEXT("WorkerRiskSeed"))->SetPropertyValue_InContainer(Site, 12345);
		return Site;
	}
};

TArray<uint8> WorkforceSnapshot(UMiningSiteComponent* Site)
{
	Site->OnComponentSaved();
	TArray<uint8> Bytes;
	FMemoryWriter Writer(Bytes);
	FALSSaveGameArchive Archive(Writer, true);
	Site->Serialize(Archive);
	return Bytes;
}

void RestoreWorkforce(UMiningSiteComponent* Site, const TArray<uint8>& Bytes)
{
	FMemoryReader Reader(Bytes);
	FALSSaveGameArchive Archive(Reader, true);
	Site->Serialize(Archive);
	Site->OnComponentLoaded();
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningDayEndLossTest, "Pangea.ACF.Mining.Workforce.DayEndLossAndProduction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningDayEndLossTest::RunTest(const FString& Parameters)
{
	FMiningWorkforceFixture Fixture;
	UMiningSiteComponent* Site = Fixture.AddSite();
	if (!TestNotNull(TEXT("Site"), Site)) return false;
	Site->ProduceForElapsedSeconds(99.f);
	TestEqual(TEXT("No early death before the day boundary"), Site->GetLivingWorkerCount(), 2);
	Site->ProduceForElapsedSeconds(1.f);
	TestEqual(TEXT("All living miners roll at 100% chance"), Site->GetLivingWorkerCount(), 0);
	TestEqual(TEXT("Day production is awarded before its ending loss"), Site->GetStoredUnits(), 20);
	TestEqual(TEXT("One completed day"), Site->GetCompletedMiningDays(), int64(1));
	Site->ProduceForElapsedSeconds(100.f);
	TestEqual(TEXT("No workers means no automated output next day"), Site->GetStoredUnits(), 20);
	TestEqual(TEXT("Vacancies are not respawned by another day"), Site->GetWorkerVacancies(), 2);
	TestFalse(TEXT("Missing recruitment price is not free recruitment"), Site->RecruitWorker(nullptr));
	UMiningSiteComponent* Full = Fixture.AddSite(1.f, 2, 1);
	if (!TestNotNull(TEXT("Full site"), Full)) return false;
	Full->ProduceForElapsedSeconds(100.f);
	TestEqual(TEXT("Full storage does not stop day-end worker risk"), Full->GetLivingWorkerCount(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningAttendanceTest, "Pangea.ACF.Mining.Workforce.AttendanceProtectsMiners",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningAttendanceTest::RunTest(const FString& Parameters)
{
	FMiningWorkforceFixture Fixture;
	UMiningSiteComponent* Site = Fixture.AddSite();
	APlayerController* Player = Fixture.World->SpawnActor<APlayerController>();
	APawn* Pawn = Fixture.World->SpawnActor<APawn>();
	if (!TestNotNull(TEXT("Site"), Site) || !TestNotNull(TEXT("Player"), Player) || !TestNotNull(TEXT("Player pawn"), Pawn)) return false;
	// This preview world has not initialized actors, so PostInitializeComponents
	// has not registered its controller in the world's player-controller iterator.
	Fixture.World->AddController(Player);
	Player->Possess(Pawn);
	if (!TestTrue(TEXT("Attendance fixture possesses its player pawn"), Player->GetPawn() == Pawn)) return false;
	TestTrue(TEXT("Any possessed player in radius attends the site"), Site->IsSiteAttended());
	Site->ProduceForElapsedSeconds(100.f);
	TestEqual(TEXT("Attendance suppresses even 100% unattended loss"), Site->GetLivingWorkerCount(), 2);
	Player->UnPossess();
	TestFalse(TEXT("An unpossessed pawn is not attendance"), Site->IsSiteAttended());
	Site->ProduceForElapsedSeconds(100.f);
	TestEqual(TEXT("Unattended following day applies risk"), Site->GetLivingWorkerCount(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningWorkerRollTest, "Pangea.ACF.Mining.Workforce.IndependentRollsAndCadence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningWorkerRollTest::RunTest(const FString& Parameters)
{
	FMiningWorkforceFixture Fixture;
	UMiningSiteComponent* Frequent = Fixture.AddSite(.25f, 20);
	UMiningSiteComponent* Batched = Fixture.AddSite(.25f, 20);
	if (!TestNotNull(TEXT("Frequent"), Frequent) || !TestNotNull(TEXT("Batched"), Batched)) return false;
	FRandomStream Expected(static_cast<int32>(HashCombineFast(12345u, HashCombineFast(1u, 0u))));
	int32 Losses = 0;
	for (int32 I = 0; I < 20; ++I) Losses += Expected.FRand() < .25f ? 1 : 0;
	Frequent->ProduceForElapsedSeconds(100.f);
	TestEqual(TEXT("One independent draw for each living miner"), Frequent->GetWorkerVacancies(), Losses);
	for (int32 I = 0; I < 200; ++I) Frequent->ProduceForElapsedSeconds(1.f);
	Batched->ProduceForElapsedSeconds(300.f);
	TestEqual(TEXT("Frequent/batched days have identical persistent vacancies"), Frequent->GetWorkerVacancies(), Batched->GetWorkerVacancies());
	TestEqual(TEXT("Production splits at losses regardless of accounting cadence"), Frequent->GetStoredUnits(), Batched->GetStoredUnits());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningWorkerPersistenceTest, "Pangea.ACF.Mining.Workforce.PauseOfflineAndPersistentVacancies",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningWorkerPersistenceTest::RunTest(const FString& Parameters)
{
	FMiningWorkforceFixture Fixture;
	UMiningSiteComponent* Site = Fixture.AddSite(0.f);
	if (!TestNotNull(TEXT("Site"), Site)) return false;
	TestTrue(TEXT("Actual death records one vacancy"), Site->RecordWorkerDeath());
	Site->ProduceForElapsedSeconds(50.f);
	const TArray<uint8> HalfDay = WorkforceSnapshot(Site);
	Site->ProduceForElapsedSeconds(100.f);
	Fixture.World->TimeSeconds = 90000.f;
	RestoreWorkforce(Site, HalfDay);
	Site->OnComponentLoaded();
	TestEqual(TEXT("Loading restores vacancies, not a free new crew"), Site->GetLivingWorkerCount(), 1);
	TestEqual(TEXT("Loading preserves partial online day"), Site->GetMiningDayElapsedSeconds(), 50.0);
	Site->SyncProductionFromWorldTime();
	TestEqual(TEXT("No offline days or loss rolls"), Site->GetCompletedMiningDays(), int64(0));
	TestEqual(TEXT("One of two workers produces half rate"), Site->GetStoredUnits(), 5);
	Site->SiteDefinition->Levels[0].WorkerDeathChance = 1.f;
	Fixture.World->TimeSeconds += 50.f;
	Site->SyncProductionFromWorldTime();
	TestEqual(TEXT("Remaining half-day rolls once after load"), Site->GetLivingWorkerCount(), 0);
	const TArray<uint8> AfterLoss = WorkforceSnapshot(Site);
	RestoreWorkforce(Site, AfterLoss);
	Site->SyncProductionFromWorldTime();
	TestEqual(TEXT("Already completed day cannot reroll on reload"), Site->GetCompletedMiningDays(), int64(1));
	TestEqual(TEXT("All saved vacancies survive"), Site->GetWorkerVacancies(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningRecruitmentTest, "Pangea.ACF.Mining.Workforce.PaidReplacementAndUpgradeVacancies",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningRecruitmentTest::RunTest(const FString& Parameters)
{
	FMiningWorkforceFixture Fixture;
	UMiningSiteComponent* Site = Fixture.AddSite(0.f);
	if (!TestNotNull(TEXT("Site"), Site)) return false;
	Site->RecordWorkerDeath();
	FMiningSiteLevelDefinition Next = Site->SiteDefinition->Levels[0];
	Next.Level = 1;
	Next.WorkerCount = 3;
	FMiningItemQuantity Cost;
	Cost.ItemClass = UACFItem::StaticClass();
	Cost.Quantity = 1;
	Next.WorkerRecruitmentCost = {Cost, Cost}; // Duplicate lines must charge two, not one.
	Site->SiteDefinition->Levels.Add(Next);
	TestTrue(TEXT("Upgrade available"), Site->UpgradeSite());
	TestEqual(TEXT("New upgrade slot fills, but dead worker is not resurrected"), Site->GetLivingWorkerCount(), 2);
	TestEqual(TEXT("Vacancy survives upgrade"), Site->GetWorkerVacancies(), 1);
	AActor* Settlement = Fixture.World->SpawnActor<AActor>();
	UACFStorageComponent* Storage = NewObject<UACFStorageComponent>(Settlement);
	Settlement->AddInstanceComponent(Storage);
	Storage->SetMaxInventorySlots(20);
	Storage->SetMaxInventoryWeight(10000.f);
	Storage->AddItem(FBaseItem(UACFItem::StaticClass(), 1));
	TestFalse(TEXT("Duplicate cost requires the full total"), Site->RecruitWorker(Settlement));
	TestEqual(TEXT("Failed recruitment preserves vacancy"), Site->GetWorkerVacancies(), 1);
	Storage->AddItem(FBaseItem(UACFItem::StaticClass(), 2));
	TestTrue(TEXT("Paid recruitment replaces one vacancy"), Site->RecruitWorker(Settlement));
	TestEqual(TEXT("Crew now full"), Site->GetLivingWorkerCount(), 3);
	TestEqual(TEXT("Actual ACF inventory charged exactly once"), Storage->GetTotalCountOfItemsByClass(UACFItem::StaticClass()), 1);
	TestFalse(TEXT("Repeated recruit cannot overstaff or charge again"), Site->RecruitWorker(Settlement));
	TestEqual(TEXT("No charge at full staffing"), Storage->GetTotalCountOfItemsByClass(UACFItem::StaticClass()), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningWorkforceReplicationTest, "Pangea.ACF.Mining.Workforce.ReplicationAndSaveContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningWorkforceReplicationTest::RunTest(const FString& Parameters)
{
	for (const TCHAR* Name : {TEXT("WorkerVacancies"), TEXT("MiningDayElapsedSeconds"), TEXT("CompletedMiningDays")})
	{
		const FProperty* Property = FindFProperty<FProperty>(UMiningSiteComponent::StaticClass(), Name);
		TestTrue(FString::Printf(TEXT("%s replicated and saved"), Name), Property && Property->HasAllPropertyFlags(CPF_Net | CPF_SaveGame));
	}
	const FProperty* Seed = FindFProperty<FProperty>(UMiningSiteComponent::StaticClass(), TEXT("WorkerRiskSeed"));
	TestTrue(TEXT("Risk seed persists but is not sent to clients"), Seed && Seed->HasAnyPropertyFlags(CPF_SaveGame) && !Seed->HasAnyPropertyFlags(CPF_Net));
	return true;
}
#endif
