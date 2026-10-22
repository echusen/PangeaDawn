#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "ACFAIController.h"
#include "Actors/ACFCharacter.h"
#include "AIController.h"
#include "ACFTeamsConfigDataAsset.h"
#include "Components/ACFTeamManagerComponent.h"
#include "Components/ACFAIRoutineComponent.h"
#include "Components/MiningRoutineJobComponent.h"
#include "Components/MiningSitePresentationCoordinatorComponent.h"
#include "Data/ACFAIRoutineDataAsset.h"
#include "DataAssets/MiningSitePresentationConfig.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "Misc/ScopeExit.h"
#include "Tasks/MiningRoutineTask.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningRoutineContractTest, "Pangea.ACF.Mining.RoutineBridgeContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningRoutineContractTest::RunTest(const FString& Parameters)
{
	const UMiningSitePresentationConfig* Defaults = GetDefault<UMiningSitePresentationConfig>();
	TestFalse(TEXT("Existing assets require explicit migration"), Defaults->bUseACFRoutineJobs);
	TestTrue(TEXT("No hardcoded controller content dependency"), Defaults->RoutineControllerClass.IsNull());
	TestTrue(TEXT("Project duty integrates through the ACF task base"), UMiningRoutineTask::StaticClass()->IsChildOf(UACFTask::StaticClass()));
	for (const FName Name : { FName(TEXT("RuntimeRoutine")), FName(TEXT("InteractionTask")), FName(TEXT("MoveTask")), FName(TEXT("RequestedTask")) })
	{
		const FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(UMiningRoutineJobComponent::StaticClass(), Name);
		TestTrue(*FString::Printf(TEXT("%s is GC-owned transient execution state, not save data"), *Name.ToString()),
			Property && Property->HasAnyPropertyFlags(CPF_Transient) && !Property->HasAnyPropertyFlags(CPF_SaveGame));
	}
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningRoutineIsolationTest, "Pangea.ACF.Mining.RoutineInstancesArePerController",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningRoutineIsolationTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Editor, false);
	if (!TestNotNull(TEXT("Isolated Editor world"), World)) return false;
	ON_SCOPE_EXIT { World->DestroyWorld(false); };
	// ACF controller possession resolves team IDs through the world's GameState.
	// Supply the real prerequisite in this isolated world instead of suppressing errors.
	AGameStateBase* GameState = World->SpawnActor<AGameStateBase>();
	if (!TestNotNull(TEXT("Fixture GameState"), GameState)) return false;
	World->SetGameState(GameState);
	UACFTeamManagerComponent* Teams = NewObject<UACFTeamManagerComponent>(GameState);
	GameState->AddInstanceComponent(Teams);
	Teams->RegisterComponent();
	Teams->SetTeamConfigDataAsset(NewObject<UACFTeamsConfigDataAsset>(Teams));
	UClass* ControllerClass = LoadClass<AACFAIController>(nullptr, TEXT("/Game/Yarin/BP_Pangea_MiningRoutineController.BP_Pangea_MiningRoutineController_C"));
	if (!TestNotNull(TEXT("Pangea-owned ACF routine controller copy available"), ControllerClass)) return false;
	AActor* Site = World->SpawnActor<AActor>();
	UMiningSitePresentationCoordinatorComponent* Coordinator = NewObject<UMiningSitePresentationCoordinatorComponent>(Site);
	Site->AddInstanceComponent(Coordinator);
	Coordinator->RegisterComponent();
	TArray<UMiningRoutineJobComponent*> Jobs;
	TArray<UACFAIRoutineComponent*> Routines;
	for (int32 Index = 0; Index < 2; ++Index)
	{
		AACFCharacter* Pawn = World->SpawnActor<AACFCharacter>();
		AACFAIController* Controller = World->SpawnActor<AACFAIController>(ControllerClass);
		if (!TestNotNull(TEXT("ACF character"), Pawn) || !TestNotNull(TEXT("ACF controller"), Controller)) return false;
		UACFAIRoutineComponent* Routine = Controller->FindComponentByClass<UACFAIRoutineComponent>();
		if (!TestNotNull(TEXT("Inherited ACF routine component"), Routine)) return false;
		Routine->SetRoutineDataAsset(NewObject<UACFAIRoutineDataAsset>(Controller, NAME_None, RF_Transient));
		Controller->Possess(Pawn);
		UMiningRoutineJobComponent* Job = NewObject<UMiningRoutineJobComponent>(Controller);
		Controller->AddInstanceComponent(Job);
		Job->RegisterComponent();
		if (!TestTrue(TEXT("Bridge accepts real ACF possession"), Job->ConfigureAssignment(Coordinator, Pawn, EMiningPresentationRole::Worker, nullptr, FVector::ZeroVector, FVector(200, 0, 0), 2.0f))) return false;
		Jobs.Add(Job);
		Routines.Add(Routine);
	}
	UACFAIRoutineDataAsset* First = Routines[0]->GetRoutineDataAsset();
	UACFAIRoutineDataAsset* Second = Routines[1]->GetRoutineDataAsset();
	TestTrue(TEXT("Definitions are private to each controller"), First && Second && First != Second);
	if (!First || !Second || First->DailyTasks.IsEmpty() || Second->DailyTasks.IsEmpty()) return false;
	TestTrue(TEXT("Mutable ACF tasks are not shared"), First->DailyTasks[0].RoutineTask != Second->DailyTasks[0].RoutineTask);
	TestEqual(TEXT("All-day default starts at midnight"), First->DailyTasks[0].TaskTime.ToMinutesSinceMidnight(), 0);
	Routines[0]->StartByGuid(First->DailyTasks[0].EntryGuid);
	Routines[1]->StartByGuid(Second->DailyTasks[0].EntryGuid);
	const FObjectPropertyBase* Requested = FindFProperty<FObjectPropertyBase>(Jobs[0]->GetClass(), TEXT("RequestedTask"));
	if (!TestNotNull(TEXT("Requested duty property"), Requested)) return false;
	TestTrue(TEXT("ACF starts the project duty on its own controller"), Requested->GetObjectPropertyValue_InContainer(Jobs[0]) == First->DailyTasks[0].RoutineTask);
	CastChecked<AACFAIController>(Jobs[0]->GetOwner())->SetCurrentAIState(FGameplayTag::RequestGameplayTag(TEXT("AIState.Wait")));
	TestTrue(TEXT("ACF state change immediately suspends the duty"), Jobs[0]->GetJobStatus() == EMiningRoutineJobStatus::Suspended);
	Routines[0]->StopAll();
	TestNull(TEXT("Stopping first routine revokes only its duty"), Requested->GetObjectPropertyValue_InContainer(Jobs[0]));
	TestTrue(TEXT("Second controller's duty remains requested"), Requested->GetObjectPropertyValue_InContainer(Jobs[1]) == Second->DailyTasks[0].RoutineTask);
	UACFAIRoutineDataAsset* External = NewObject<UACFAIRoutineDataAsset>(Routines[1]);
	Routines[1]->StopAll();
	Routines[1]->SetRoutineDataAsset(External);
	Jobs[1]->ReleaseAssignment();
	TestTrue(TEXT("Detach never overwrites an externally replaced schedule"), Routines[1]->GetRoutineDataAsset() == External);
	Jobs[0]->ReleaseAssignment();
	Jobs[0]->ReleaseAssignment();
	TestTrue(TEXT("Release is idempotent"), Jobs[0]->GetJobStatus() == EMiningRoutineJobStatus::Unassigned);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningRoutineRejectsLegacyControllerTest, "Pangea.ACF.Mining.RoutineRejectsLegacyController",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningRoutineRejectsLegacyControllerTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Editor, false);
	if (!World) return false;
	ON_SCOPE_EXIT { World->DestroyWorld(false); };
	AAIController* Legacy = World->SpawnActor<AAIController>();
	AACFCharacter* Pawn = World->SpawnActor<AACFCharacter>();
	AActor* Site = World->SpawnActor<AActor>();
	UMiningRoutineJobComponent* Job = NewObject<UMiningRoutineJobComponent>(Legacy);
	UMiningSitePresentationCoordinatorComponent* Coordinator = NewObject<UMiningSitePresentationCoordinatorComponent>(Site);
	Legacy->Possess(Pawn);
	TestFalse(TEXT("Generic combat controller cannot silently run a mining routine"), Job->ConfigureAssignment(Coordinator, Pawn, EMiningPresentationRole::Worker, nullptr, FVector::ZeroVector, FVector(100, 0, 0), 2.0f));
	TestTrue(TEXT("Rejected assignment remains unassigned"), Job->GetJobStatus() == EMiningRoutineJobStatus::Unassigned);
	return !HasAnyErrors();
}
#endif
