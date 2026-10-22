#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Components/PangeaTrackEmitterComponent.h"
#include "Net/UnrealNetwork.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPangeaHuntingConfigReplicationTest,
	"Pangea.ACF.Hunting.ConfigurationReplicationContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPangeaHuntingConfigReplicationTest::RunTest(const FString& Parameters)
{
	const FProperty* Config = FindFProperty<FProperty>(UPangeaTrackEmitterComponent::StaticClass(), TEXT("HuntSpeciesConfig"));
	// Check metadata first: GetLifetimeReplicatedProps would otherwise call LogNet Fatal.
	if (!TestTrue(TEXT("HuntSpeciesConfig has the reflected replication flag"), Config && Config->HasAnyPropertyFlags(CPF_Net))) return false;
	TestTrue(TEXT("Hunting config retains SaveGame persistence"), Config->HasAnyPropertyFlags(CPF_SaveGame));
	// Mirror FRepLayout::InitFromClass before reading FProperty::RepIndex. Without
	// this, an isolated test can register index zero over the base component's
	// push-based property and hit the engine's conflicting-conditions check.
	UPangeaTrackEmitterComponent::StaticClass()->SetUpRuntimeReplicationData();
	TArray<FLifetimeProperty> Lifetime;
	GetDefault<UPangeaTrackEmitterComponent>()->GetLifetimeReplicatedProps(Lifetime);
	TestTrue(TEXT("Hunting config is registered in the lifetime replication layout"), Lifetime.ContainsByPredicate([Config](const FLifetimeProperty& Entry) { return Entry.RepIndex == Config->RepIndex; }));
	return !HasAnyErrors();
}
#endif
