#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Actors/MiningDiscoveryNodeActor.h"
#include "Actors/MiningSettlementStockpileActor.h"
#include "Actors/MiningSiteActor.h"
#include "Components/MiningSiteComponent.h"
#include "Components/PangeaMiningTransportStorageComponent.h"
#include "DataAssets/MiningSiteDefinition.h"
#include "Editor.h"
#include "EngineUtils.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningHybridPrototypeContractTest, "Pangea.ACF.Mining.Transport.PrototypeKitContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningHybridPrototypeContractTest::RunTest(const FString& Parameters)
{
	const auto* Baseline = LoadObject<UMiningSiteDefinition>(nullptr, TEXT("/Game/_Game/MiningSystem/DA_MiningSite_Iron_Test.DA_MiningSite_Iron_Test"));
	const auto* Prototype = LoadObject<UMiningSiteDefinition>(nullptr, TEXT("/Game/Yarin/DA_Pangea_MiningSite_HybridPrototype.DA_Pangea_MiningSite_HybridPrototype"));
	UClass* NodeClass = LoadClass<AMiningDiscoveryNodeActor>(nullptr, TEXT("/Game/Yarin/BP_Pangea_MiningDiscovery_HybridPrototype.BP_Pangea_MiningDiscovery_HybridPrototype_C"));
	UClass* SiteClass = LoadClass<AMiningSiteActor>(nullptr, TEXT("/Game/Yarin/BP_Pangea_MiningSite_HybridPrototype.BP_Pangea_MiningSite_HybridPrototype_C"));
	UClass* StockpileClass = LoadClass<AMiningSettlementStockpileActor>(nullptr, TEXT("/Game/Yarin/BP_Pangea_MiningStockpile_HybridPrototype.BP_Pangea_MiningStockpile_HybridPrototype_C"));
	if (!TestNotNull(TEXT("Baseline"), Baseline) || !TestNotNull(TEXT("Prototype"), Prototype) ||
		!TestNotNull(TEXT("Prototype discovery"), NodeClass) || !TestNotNull(TEXT("Prototype site"), SiteClass) ||
		!TestNotNull(TEXT("Prototype stockpile"), StockpileClass)) return false;
	TestFalse(TEXT("Existing demo retains legacy logistics"), Baseline->bUseHybridTransport);
	TestTrue(TEXT("Only new prototype opts into hybrid"), Prototype->bUseHybridTransport);
	TestEqual(TEXT("Copied day duration"), Prototype->SimulatedDaySeconds, Baseline->SimulatedDaySeconds);
	TestEqual(TEXT("Copied level count"), Prototype->Levels.Num(), Baseline->Levels.Num());
	TestTrue(TEXT("Existing Pangea presentation config reused without edits"), Prototype->PresentationConfig == Baseline->PresentationConfig);
	for (const auto& Original : Baseline->Levels)
	{
		FMiningSiteLevelDefinition Actual;
		if (!TestTrue(TEXT("Copied level exists"), Prototype->GetLevelDefinition(Original.Level, Actual))) continue;
		FMiningSiteLevelDefinition Expected = Original;
		if (Original.Level >= 4)
		{
			Expected.TransportChariotClass = TSoftClassPtr<AActor>(FSoftObjectPath(TEXT("/Game/Yarin/BP_Pangea_MiningChariot.BP_Pangea_MiningChariot_C")));
			Expected.TransportDraftAnimalClass = TSoftClassPtr<AActor>(FSoftObjectPath(TEXT("/Game/Yarin/BP_Pangea_KorroDraft.BP_Pangea_KorroDraft_C")));
			Expected.TransportTravelSpeed = 450.f;
		}
		if (Original.Level == 5) Expected.TransportEscortCount = Original.GuardCount;
		TestTrue(*FString::Printf(TEXT("Level %d preserves corrected balance except explicit cart/draft/escort/speed additions"), Original.Level),
			FMiningSiteLevelDefinition::StaticStruct()->CompareScriptStruct(&Expected, &Actual, 0));
	}
	const auto* Node = NodeClass->GetDefaultObject<AMiningDiscoveryNodeActor>();
	const auto* Site = SiteClass->GetDefaultObject<AMiningSiteActor>();
	const auto* Stockpile = StockpileClass->GetDefaultObject<AMiningSettlementStockpileActor>();
	TestTrue(TEXT("Discovery points to prototype profile"), Node->SiteDefinition == Prototype);
	TestTrue(TEXT("Discovery spawns prototype site class"), Node->MiningSiteActorClass.Get() == SiteClass);
	TestTrue(TEXT("Directly placed prototype site also uses prototype profile"), Site->MiningSiteComponent && Site->MiningSiteComponent->SiteDefinition == Prototype);
	TestTrue(TEXT("Prototype site has ledger and physical broker"), Site->TransportComponent && Site->ConvoyComponent);
	TestTrue(TEXT("Prototype stockpile uses exact-acceptance Pangea ACF adapter"),
		Stockpile->StorageComponent && Stockpile->StorageComponent->IsA<UPangeaMiningTransportStorageComponent>());
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!TestNotNull(TEXT("Editor world"), World)) return false;
	AMiningDiscoveryNodeActor* PlacedNode = nullptr;
	int32 PrototypeNodes = 0, LegacyNodes = 0;
	for (TActorIterator<AMiningDiscoveryNodeActor> It(World); It; ++It)
	{
		if (It->ActorHasTag(TEXT("Pangea.Mining.Prototype"))) { PlacedNode = *It; ++PrototypeNodes; }
		else ++LegacyNodes;
	}
	TestEqual(TEXT("Exactly one separate prototype discovery placed"), PrototypeNodes, 1);
	TestTrue(TEXT("Legacy discovery preserved"), LegacyNodes > 0);
	if (TestNotNull(TEXT("Placed prototype"), PlacedNode))
	{
		TestTrue(TEXT("Placed discovery uses prototype class"), PlacedNode->IsA(NodeClass));
		const auto* Destination = Cast<AMiningSettlementStockpileActor>(PlacedNode->SettlementResourceActor);
		if (TestNotNull(TEXT("Prototype destination explicitly linked"), Destination))
		{
			TestTrue(TEXT("Linked stockpile is separate prototype instance"), Destination->IsA(StockpileClass) && Destination->ActorHasTag(TEXT("Pangea.Mining.Prototype")));
			TestTrue(TEXT("Linked destination uses acceptance adapter"), Destination->StorageComponent->IsA<UPangeaMiningTransportStorageComponent>());
			// Invoker-only navmesh is not generated for this area in the editor.
			// Actual linked-endpoint path following is asserted by the PIE convoy test.
		}
		for (TActorIterator<AMiningDiscoveryNodeActor> It(World); It; ++It)
			if (!It->ActorHasTag(TEXT("Pangea.Mining.Prototype")))
				TestTrue(TEXT("Prototype more than 30m from legacy discovery"), FVector::Dist2D(It->GetActorLocation(), PlacedNode->GetActorLocation()) > 3000.);
	}
	return true;
}
#endif
