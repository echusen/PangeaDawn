#include "Misc/AutomationTest.h"
#include "SmartObjectTypes.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR && WITH_SMARTOBJECT_DEBUG
#include "AIController.h"
#include "Components/MiningSitePresentationCoordinatorComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameplayInteractionSmartObjectBehaviorDefinition.h"
#include "GameplayInteractionStateTreeSchema.h"
#include "SmartObjectComponent.h"
#include "SmartObjectDefinition.h"
#include "SmartObjectSubsystem.h"
#include "StateTree.h"
#include "StateTreeCompiler.h"
#include "StateTreeCompilerLog.h"
#include "StateTreeEditorData.h"
#include "StateTreeState.h"
#include "Tasks/MiningSmartObjectInteractionTask.h"
#include "UObject/UnrealType.h"

namespace
{
struct FMiningInteractionFixture
{
	// SmartObjectSubsystem supports Editor/Game/PIE, not EditorPreview. An isolated
	// Editor world provides the real subsystem without Game-world input preprocessors.
	UWorld* World = UWorld::CreateWorld(EWorldType::Editor, false);
	USmartObjectSubsystem* Subsystem = nullptr;
	USmartObjectComponent* Station = nullptr;
	UGameplayInteractionSmartObjectBehaviorDefinition* Behavior = nullptr;
	AAIController* Controller = nullptr;
	APawn* Pawn = nullptr;
	TArray<FSmartObjectSlotHandle> Slots;
	TArray<TObjectPtr<UMiningSmartObjectInteractionTask>> Tasks;

	~FMiningInteractionFixture()
	{
		for (UMiningSmartObjectInteractionTask* Task : Tasks)
			if (IsValid(Task) && !Task->IsFinished()) Task->ExternalCancel();
		if (World) World->DestroyWorld(false);
	}

	bool Initialize(FAutomationTestBase& Test)
	{
		const auto Fail = [&Test](const TCHAR* Message)
		{
			Test.AddError(FString(TEXT("Mining interaction fixture: ")) + Message);
			return false;
		};
		if (!World) return Fail(TEXT("world creation failed"));
		Subsystem = USmartObjectSubsystem::GetCurrent(World);
		if (!Subsystem) return Fail(TEXT("SmartObjectSubsystem unavailable"));
		Subsystem->DebugInitializeRuntime();
		Controller = World->SpawnActor<AAIController>();
		Pawn = World->SpawnActor<APawn>();
		AActor* StationOwner = World->SpawnActor<AActor>();
		if (!Controller || !Pawn || !StationOwner) return Fail(TEXT("actor spawning failed"));
		Controller->Possess(Pawn);
		USmartObjectDefinition* Definition = NewObject<USmartObjectDefinition>(StationOwner);
		Behavior = NewObject<UGameplayInteractionSmartObjectBehaviorDefinition>(Definition);
		UStateTree* Tree = NewObject<UStateTree>(Behavior);
		UStateTreeEditorData* EditorData = NewObject<UStateTreeEditorData>(Tree);
		Tree->EditorData = EditorData;
		EditorData->Schema = NewObject<UGameplayInteractionStateTreeSchema>(EditorData);
		UStateTreeState& Root = EditorData->AddRootState();
		Root.AddTransition(EStateTreeTransitionTrigger::OnStateCompleted, EStateTreeTransitionType::Succeeded);
		FStateTreeCompilerLog CompilerLog;
		FStateTreeCompiler Compiler(CompilerLog);
		if (!Compiler.Compile(*Tree))
		{
			CompilerLog.DumpToLog(LogTemp);
			return Fail(TEXT("transient StateTree compilation failed"));
		}
		Behavior->SetStateTree(Tree);
		Definition->DebugAddSlot().BehaviorDefinitions.Add(Behavior);
		TArray<TPair<EMessageSeverity::Type, FText>> DefinitionErrors;
		if (!Definition->Validate(&DefinitionErrors))
		{
			for (const auto& Error : DefinitionErrors) Test.AddError(Error.Value.ToString());
			return Fail(TEXT("transient SmartObject definition validation failed"));
		}
		Station = NewObject<USmartObjectComponent>(StationOwner);
		StationOwner->AddInstanceComponent(Station);
		StationOwner->SetRootComponent(Station);
		Station->SetDefinition(Definition);
		Station->RegisterComponent();
		if (!Station->GetRegisteredHandle().IsValid()) Subsystem->RegisterSmartObject(Station);
		Subsystem->GetAllSlots(Station->GetRegisteredHandle(), Slots);
		if (Slots.IsEmpty()) return Fail(TEXT("station has no registered runtime slots"));
		if (Controller->GetPawn() != Pawn) return Fail(TEXT("controller did not possess fixture pawn"));
		return true;
	}

	FSmartObjectClaimHandle Claim()
	{
		return Slots.IsEmpty() ? FSmartObjectClaimHandle() : Subsystem->MarkSlotAsClaimed(Slots[0], ESmartObjectClaimPriority::Normal);
	}

	UMiningSmartObjectInteractionTask* Create(FSmartObjectClaimHandle Claim)
	{
		UMiningSmartObjectInteractionTask* Task = UMiningSmartObjectInteractionTask::MoveToAndUseClaimedSlot(Controller, Claim, false, false);
		if (Task) Tasks.Add(Task);
		return Task;
	}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningInteractionContractTest, "Pangea.ACF.Mining.PublicInteractionTaskContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningInteractionContractTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("Mining task uses public AITask base"), UMiningSmartObjectInteractionTask::StaticClass()->IsChildOf(UAITask::StaticClass()));
	const UFunction* Factory = UMiningSmartObjectInteractionTask::StaticClass()->FindFunctionByName(TEXT("MoveToAndUseClaimedSlot"));
	TestTrue(TEXT("Factory remains Blueprint callable"), Factory && Factory->HasAnyFunctionFlags(FUNC_BlueprintCallable));
	const FMapProperty* Tasks = FindFProperty<FMapProperty>(UMiningSitePresentationCoordinatorComponent::StaticClass(), TEXT("PresentationSmartObjectTaskMap"));
	if (!TestNotNull(TEXT("Coordinator retains task map through GC"), Tasks)) return false;
	TestTrue(TEXT("Presentation tasks are transient, not save data"), Tasks->HasAnyPropertyFlags(CPF_Transient) && !Tasks->HasAnyPropertyFlags(CPF_SaveGame));
	const FObjectPropertyBase* Value = CastField<FObjectPropertyBase>(Tasks->ValueProp);
	TestTrue(TEXT("Map owns the mining task, not an engine-private class"), Value && Value->PropertyClass == UMiningSmartObjectInteractionTask::StaticClass());
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningInteractionFactoryTest, "Pangea.ACF.Mining.InteractionFactoryAndPreActivationCancel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningInteractionFactoryTest::RunTest(const FString& Parameters)
{
	FMiningInteractionFixture Fixture;
	if (!TestTrue(TEXT("Isolated public SmartObject/StateTree fixture"), Fixture.Initialize(*this))) return false;
	const FSmartObjectClaimHandle Claim = Fixture.Claim();
	if (!TestTrue(TEXT("Slot claimed"), Claim.IsValid())) return false;
	TestNull(TEXT("No controller rejected"), UMiningSmartObjectInteractionTask::MoveToAndUseClaimedSlot(nullptr, Claim));
	TestNull(TEXT("Invalid claim rejected"), Fixture.Create(FSmartObjectClaimHandle()));
	UStateTree* Tree = const_cast<UStateTree*>(Fixture.Behavior->GetStateTree());
	Fixture.Behavior->SetStateTree(nullptr);
	TestNull(TEXT("Missing StateTree rejected"), Fixture.Create(Claim));
	TestTrue(TEXT("Failed factory leaves claim with caller"), Fixture.Subsystem->GetSlotState(Claim.SlotHandle) == ESmartObjectSlotState::Claimed);
	Fixture.Behavior->SetStateTree(Tree);
	UMiningSmartObjectInteractionTask* Task = Fixture.Create(Claim);
	if (!TestNotNull(TEXT("Valid task created"), Task)) return false;
	Task->ExternalCancel(); // Resource queue/activation never started; claim still must be released.
	TestTrue(TEXT("Pre-activation cancel ends task"), Task->IsFinished());
	TestTrue(TEXT("Pre-activation cancel frees claim"), Fixture.Subsystem->GetSlotState(Claim.SlotHandle) == ESmartObjectSlotState::Free);
	Task->ExternalCancel();
	TestNull(TEXT("Stale freed claim rejected"), Fixture.Create(Claim));
	TestFalse(TEXT("Cancellation never reports success"), Task->DidSucceed());
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningInteractionCompletionTest, "Pangea.ACF.Mining.PublicInteractionCompletesAndReclaims",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningInteractionCompletionTest::RunTest(const FString& Parameters)
{
	FMiningInteractionFixture Fixture;
	if (!TestTrue(TEXT("Isolated fixture"), Fixture.Initialize(*this))) return false;
	const FSmartObjectClaimHandle Claim = Fixture.Claim();
	UMiningSmartObjectInteractionTask* Task = Fixture.Create(Claim);
	if (!TestNotNull(TEXT("Task"), Task)) return false;
	Task->ReadyForActivation();
	for (int32 Index = 0; Index < 4 && !Task->IsFinished(); ++Index)
		static_cast<UGameplayTask*>(Task)->TickTask(0.1f);
	TestTrue(TEXT("Real public interaction StateTree completes"), Task->IsFinished() && Task->DidSucceed());
	TestTrue(TEXT("Completed interaction frees slot"), Fixture.Subsystem->GetSlotState(Claim.SlotHandle) == ESmartObjectSlotState::Free);
	const FSmartObjectClaimHandle NextClaim = Fixture.Claim();
	TestTrue(TEXT("Slot can be claimed by the next interaction"), NextClaim.IsValid());
	Task->ExternalCancel(); // Must not free the subsequent claim.
	TestTrue(TEXT("Repeated cancellation leaves new claim intact"), Fixture.Subsystem->GetSlotState(NextClaim.SlotHandle) == ESmartObjectSlotState::Claimed);
	Fixture.Subsystem->MarkSlotAsFree(NextClaim);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningInteractionInvalidationTest, "Pangea.ACF.Mining.InteractionInvalidationReleasesOnce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningInteractionInvalidationTest::RunTest(const FString& Parameters)
{
	FMiningInteractionFixture Fixture;
	if (!TestTrue(TEXT("Isolated fixture"), Fixture.Initialize(*this))) return false;
	const FSmartObjectClaimHandle Claim = Fixture.Claim();
	UMiningSmartObjectInteractionTask* Task = Fixture.Create(Claim);
	if (!TestNotNull(TEXT("Task"), Task)) return false;
	Task->ReadyForActivation();
	TestFalse(TEXT("Awaiting first interaction tick"), Task->IsFinished());
	TestTrue(TEXT("Station can be disabled for new users"), Fixture.Station->K2_SetSmartObjectEnabled(false));
	TestFalse(TEXT("Disabling admission alone does not evict the existing interaction"), Task->IsFinished());
	Fixture.Station->K2_SetSmartObjectEnabled(true);
	// Removing runtime state invokes the real slot-invalidation/abort path.
	TestTrue(TEXT("Station runtime removed"), Fixture.Subsystem->RemoveSmartObject(Fixture.Station));
	TestTrue(TEXT("Slot invalidation ends interaction"), Task->IsFinished());
	TestFalse(TEXT("Invalidation is failure, not successful completion"), Task->DidSucceed());
	TestTrue(TEXT("Station can be registered again"), Fixture.Subsystem->RegisterSmartObject(Fixture.Station));
	Fixture.Slots.Reset();
	Fixture.Subsystem->GetAllSlots(Fixture.Station->GetRegisteredHandle(), Fixture.Slots);
	const FSmartObjectClaimHandle NextClaim = Fixture.Claim();
	TestTrue(TEXT("Re-registered station can be claimed again"), NextClaim.IsValid());
	if (NextClaim.IsValid()) Fixture.Subsystem->MarkSlotAsFree(NextClaim);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningInteractionCoordinatorCleanupTest, "Pangea.ACF.Mining.CoordinatorCancelsOrphanedInteractions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningInteractionCoordinatorCleanupTest::RunTest(const FString& Parameters)
{
	FMiningInteractionFixture Fixture;
	if (!TestTrue(TEXT("Isolated fixture"), Fixture.Initialize(*this))) return false;
	const FSmartObjectClaimHandle Claim = Fixture.Claim();
	UMiningSmartObjectInteractionTask* Task = Fixture.Create(Claim);
	if (!TestNotNull(TEXT("Task"), Task)) return false;
	UMiningSitePresentationCoordinatorComponent* Coordinator = NewObject<UMiningSitePresentationCoordinatorComponent>(Fixture.Controller);
	Fixture.Controller->AddInstanceComponent(Coordinator);
	const FMapProperty* Map = FindFProperty<FMapProperty>(Coordinator->GetClass(), TEXT("PresentationSmartObjectTaskMap"));
	if (!TestNotNull(TEXT("Task map"), Map)) return false;
	FScriptMapHelper Helper(Map, Map->ContainerPtrToValuePtr<void>(Coordinator));
	const int32 Index = Helper.AddDefaultValue_Invalid_NeedsRehash(); // Invalid weak actor key simulates a disappeared worker.
	CastFieldChecked<FObjectPropertyBase>(Map->ValueProp)->SetObjectPropertyValue(Helper.GetValuePtr(Index), Task);
	Helper.Rehash();
	TArray<TObjectPtr<AActor>> Workers, Guards;
	TObjectPtr<AActor> Courier = nullptr;
	Coordinator->ClearPresentationActors(Workers, Guards, Courier);
	TestTrue(TEXT("Empty actor arrays cannot strand a task in the coordinator"), Task->IsFinished());
	TestTrue(TEXT("Coordinator cleanup frees the accepted claim"), Fixture.Subsystem->GetSlotState(Claim.SlotHandle) == ESmartObjectSlotState::Free);
	TestEqual(TEXT("Task map cleared"), Helper.Num(), 0);
	Coordinator->ClearPresentationActors(Workers, Guards, Courier);
	return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningInteractionMinimumHoldTest, "Pangea.ACF.Mining.InteractionMinimumHoldRetainsClaim",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningInteractionMinimumHoldTest::RunTest(const FString& Parameters)
{
	FMiningInteractionFixture Fixture;
	if (!Fixture.Initialize(*this)) return false;
	const FSmartObjectClaimHandle Claim = Fixture.Claim();
	UMiningSmartObjectInteractionTask* Task = UMiningSmartObjectInteractionTask::MoveToAndUseClaimedSlot(Fixture.Controller, Claim, false, false, 2.0f);
	if (!TestNotNull(TEXT("Task with minimum interaction hold"), Task)) return false;
	Fixture.Tasks.Add(Task);
	TestFalse(TEXT("Travel/queue time is not interaction time"), Task->IsInteracting());
	Task->ReadyForActivation();
	static_cast<UGameplayTask*>(Task)->TickTask(0.1f);
	TestTrue(TEXT("StateTree entered"), Task->IsInteracting());
	TestFalse(TEXT("Immediate StateTree success retains minimum hold"), Task->IsFinished());
	TestTrue(TEXT("Claim stays occupied during hold"), Fixture.Subsystem->GetSlotState(Claim.SlotHandle) == ESmartObjectSlotState::Occupied);
	for (int32 Index = 0; Index < 25 && !Task->IsFinished(); ++Index) static_cast<UGameplayTask*>(Task)->TickTask(0.1f);
	TestTrue(TEXT("Minimum hold eventually completes successfully"), Task->IsFinished() && Task->DidSucceed());
	TestTrue(TEXT("Hold completion frees claim"), Fixture.Subsystem->GetSlotState(Claim.SlotHandle) == ESmartObjectSlotState::Free);
	return !HasAnyErrors();
}
#endif
