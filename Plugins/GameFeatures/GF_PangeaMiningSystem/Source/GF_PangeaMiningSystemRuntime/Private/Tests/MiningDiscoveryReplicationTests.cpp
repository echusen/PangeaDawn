#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Actors/MiningDiscoveryNodeActor.h"
#include "Net/UnrealNetwork.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningDiscoveryReplicationTest,
	"Pangea.ACF.Mining.DiscoveryReferenceReplicationContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningDiscoveryReplicationTest::RunTest(const FString& Parameters)
{
	const FProperty* Site = FindFProperty<FProperty>(AMiningDiscoveryNodeActor::StaticClass(), TEXT("EstablishedSite"));
	// Never enter the fatal lifetime-registration path when the UPROPERTY contract is broken.
	if (!TestTrue(TEXT("EstablishedSite has the reflected replication flag"), Site && Site->HasAnyPropertyFlags(CPF_Net))) return false;
	TestTrue(TEXT("Established site reference retains SaveGame persistence"), Site->HasAnyPropertyFlags(CPF_SaveGame));
	// The network driver normally initializes RepIndex before requesting lifetime properties.
	AMiningDiscoveryNodeActor::StaticClass()->SetUpRuntimeReplicationData();
	TArray<FLifetimeProperty> Lifetime;
	GetDefault<AMiningDiscoveryNodeActor>()->GetLifetimeReplicatedProps(Lifetime);
	TestTrue(TEXT("EstablishedSite is registered in the lifetime replication layout"), Lifetime.ContainsByPredicate([Site](const FLifetimeProperty& Entry) { return Entry.RepIndex == Site->RepIndex; }));
	return !HasAnyErrors();
}
#endif
