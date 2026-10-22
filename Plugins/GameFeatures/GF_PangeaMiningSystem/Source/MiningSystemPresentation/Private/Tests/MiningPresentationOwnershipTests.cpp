#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Components/MiningPresentationAgentComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/ScopeExit.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningPresentationSpeedOwnershipTest, "Pangea.ACF.Mining.PresentationYieldsLocomotionOwnership",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningPresentationSpeedOwnershipTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Editor, false);
	if (!World) return false;
	ON_SCOPE_EXIT { World->DestroyWorld(false); };
	ACharacter* Character = World->SpawnActor<ACharacter>();
	if (!Character) return false;
	UMiningPresentationAgentComponent* Agent = NewObject<UMiningPresentationAgentComponent>(Character);
	Character->AddInstanceComponent(Agent);
	Agent->RegisterComponent();
	UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	Movement->MaxWalkSpeed = 600.0f;
	Agent->SetTravelSpeedOverride(EMiningPresentationRole::Worker, 220.0f);
	Agent->SetMiningPresentationState_Implementation(EMiningPresentationState::Traveling);
	TestEqual(TEXT("Duty applies travel speed"), Movement->MaxWalkSpeed, 220.0f);
	Movement->MaxWalkSpeed = 900.0f; // ACF combat state won locomotion ownership before its event broadcast.
	Agent->SetMiningPresentationState_Implementation(EMiningPresentationState::Idle);
	TestEqual(TEXT("Yield preserves combat locomotion"), Movement->MaxWalkSpeed, 900.0f);
	Agent->SetMiningPresentationState_Implementation(EMiningPresentationState::Traveling);
	Agent->SetMiningPresentationState_Implementation(EMiningPresentationState::Idle);
	TestEqual(TEXT("Next duty captures and restores the current speed, not startup defaults"), Movement->MaxWalkSpeed, 900.0f);
	return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningPresentationReplicationContractTest, "Pangea.ACF.Mining.PresentationReplicationContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningPresentationReplicationContractTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Editor, false);
	if (!World) return false;
	ON_SCOPE_EXIT { World->DestroyWorld(false); };
	ACharacter* ServerActor = World->SpawnActor<ACharacter>();
	ACharacter* ClientActor = World->SpawnActor<ACharacter>();
	if (!ServerActor || !ClientActor) return false;
	auto AddAgent = [](ACharacter* Actor)
	{
		UMiningPresentationAgentComponent* Agent = NewObject<UMiningPresentationAgentComponent>(Actor);
		Actor->AddInstanceComponent(Agent);
		Agent->RegisterComponent();
		return Agent;
	};
	UMiningPresentationAgentComponent* Server = AddAgent(ServerActor);
	UMiningPresentationAgentComponent* Client = AddAgent(ClientActor);
	TestTrue(TEXT("Dynamic presentation components replicate by default"), Server->GetIsReplicated());
	const FStructProperty* Snapshot = FindFProperty<FStructProperty>(Server->GetClass(), TEXT("PresentationSnapshot"));
	UFunction* Notify = Client->FindFunction(TEXT("OnRep_PresentationSnapshot"));
	if (!Snapshot || !Notify) return false;
	TestTrue(TEXT("Snapshot is replicated with a RepNotify"), Snapshot->HasAllPropertyFlags(CPF_Net | CPF_RepNotify));
	Server->SetMiningPresentationRole_Implementation(EMiningPresentationRole::Courier);
	Server->SetMiningPresentationState_Implementation(EMiningPresentationState::Loading);
	Server->SetMiningPresentationFocus_Implementation(FVector(100, 200, 300));
	ClientActor->SetRole(ROLE_SimulatedProxy); // Isolated contract test, NOT evidence of real transport.
	const float OriginalClientSpeed = ClientActor->GetCharacterMovement()->MaxWalkSpeed;
	Snapshot->CopyCompleteValue_InContainer(Client, Server);
	Client->ProcessEvent(Notify, nullptr);
	TestTrue(TEXT("A single snapshot reconstructs all cosmetic fields"), Client->GetCurrentRole() == EMiningPresentationRole::Courier && Client->GetCurrentState() == EMiningPresentationState::Loading && Client->HasPresentationFocus() && Client->GetPresentationFocusLocation().Equals(FVector(100, 200, 300)));
	Client->SetMiningPresentationRole_Implementation(EMiningPresentationRole::Guard);
	Client->SetMiningPresentationState_Implementation(EMiningPresentationState::Traveling);
	Client->SetMiningPresentationFocus_Implementation(FVector::ZeroVector);
	Client->SetTravelSpeedOverride(EMiningPresentationRole::Courier, 9999);
	TestTrue(TEXT("Client-side authoring is rejected"), Client->GetCurrentRole() == EMiningPresentationRole::Courier && Client->GetCurrentState() == EMiningPresentationState::Loading && Client->GetPresentationFocusLocation().Equals(FVector(100, 200, 300)));
	TestEqual(TEXT("Client snapshot/setters do not steer movement"), ClientActor->GetCharacterMovement()->MaxWalkSpeed, OriginalClientSpeed);
	return !HasAnyErrors();
}
#endif
