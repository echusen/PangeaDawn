#include "Components/MiningRoutineJobComponent.h"

#include "ACFAIController.h"
#include "Actors/ACFCharacter.h"
#include "Components/ACFAIRoutineComponent.h"
#include "Components/MiningSitePresentationCoordinatorComponent.h"
#include "Data/ACFAIRoutineDataAsset.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameplayInteractionSmartObjectBehaviorDefinition.h"
#include "SmartObjectBlueprintFunctionLibrary.h"
#include "SmartObjectComponent.h"
#include "SmartObjectRequestTypes.h"
#include "SmartObjectSubsystem.h"
#include "Tasks/AITask_MoveTo.h"
#include "Tasks/MiningRoutineTask.h"
#include "Tasks/MiningSmartObjectInteractionTask.h"
#include "TimerManager.h"

UMiningRoutineJobComponent::UMiningRoutineJobComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UMiningRoutineJobComponent::ConfigureAssignment(UMiningSitePresentationCoordinatorComponent* InCoordinator,
	APawn* Pawn, EMiningPresentationRole InRole, USmartObjectComponent* InStation,
	const FVector& InPrimary, const FVector& InSecondary, float InDuration, float InNavigationTimeout, float InRetryDelay)
{
	AACFAIController* Controller = Cast<AACFAIController>(GetOwner());
	if (!Controller || !Controller->HasAuthority() || !IsValid(Pawn) || !Pawn->IsA<AACFCharacter>() || Controller->GetPawn() != Pawn || !IsValid(InCoordinator)) return false;
	if (InPrimary.ContainsNaN() || InSecondary.ContainsNaN() || !FMath::IsFinite(InDuration) || !FMath::IsFinite(InNavigationTimeout) || !FMath::IsFinite(InRetryDelay)) return false;
	NavigationTimeoutSeconds = FMath::Max(1.0f, InNavigationTimeout);
	BlockedRetryDelaySeconds = FMath::Max(0.1f, InRetryDelay);
	if (RuntimeRoutine)
	{
		// Never restart a duty at the site's movement refresh rate or take over an external routine.
		if (AssignedPawn != Pawn || Coordinator != InCoordinator || Role != InRole || !Routine.IsValid() || Routine->GetRoutineDataAsset() != RuntimeRoutine) return false;
		if (Station != InStation || !PrimaryTarget.Equals(InPrimary) || !SecondaryTarget.Equals(InSecondary)) CancelOperation();
		Station = InStation;
		PrimaryTarget = InPrimary;
		SecondaryTarget = InSecondary;
		InteractionDuration = FMath::Max(0.1f, InDuration);
		return true;
	}
	UACFAIRoutineComponent* RoutineComponent = Controller->FindComponentByClass<UACFAIRoutineComponent>();
	if (!RoutineComponent || !Controller->GetBehaviorTree()) return false;
	Coordinator = InCoordinator;
	AssignedPawn = Pawn;
	Role = InRole;
	Station = InStation;
	PrimaryTarget = InPrimary;
	SecondaryTarget = InSecondary;
	InteractionDuration = FMath::Max(0.1f, InDuration);
	Routine = RoutineComponent;
	OriginalRoutine = RoutineComponent->GetRoutineDataAsset();
	// ACF starts the task object stored in the definition. Give every controller its own
	// definition AND instanced task, rather than sharing mutable UACFTask controller state.
	RuntimeRoutine = NewObject<UACFAIRoutineDataAsset>(this, NAME_None, RF_Transient);
	FAIRoutineTask Entry;
	Entry.TaskTime = FRoutineTime::FromMinutes(0);
	Entry.RoutineTask = NewObject<UMiningRoutineTask>(RuntimeRoutine, NAME_None, RF_Transient);
	RuntimeRoutine->DailyTasks.Add(Entry);
	RoutineComponent->StopAll();
	RoutineComponent->SetRoutineDataAsset(RuntimeRoutine);
	Controller->OnAIStateChanged.AddUniqueDynamic(this, &ThisClass::HandleAIStateChanged);
	Status = EMiningRoutineJobStatus::WaitingForRoutine;
	GetWorld()->GetTimerManager().SetTimer(UpdateTimer, this, &ThisClass::UpdateJob, 0.1f, true);
	return true;
}

bool UMiningRoutineJobComponent::CanExecute() const
{
	const AACFAIController* Controller = Cast<AACFAIController>(GetOwner());
	const AACFCharacter* Character = Controller ? Controller->GetBaseAICharacter() : nullptr;
	return Controller && Controller->HasAuthority() && AssignedPawn.IsValid() && Controller->GetPawn() == AssignedPawn.Get()
		&& Character && Character->IsAlive() && Coordinator.IsValid() && IsValid(Coordinator->GetOwner())
		&& Routine.IsValid() && Routine->GetRoutineDataAsset() == RuntimeRoutine && !Controller->IsExecutingCommand()
		&& Controller->GetAIState().MatchesTagExact(FGameplayTag::RequestGameplayTag(TEXT("AIState.Routine")));
}

void UMiningRoutineJobComponent::BeginRoutineJob(UACFTask* Task)
{
	if (!RuntimeRoutine || RuntimeRoutine->DailyTasks.IsEmpty() || RuntimeRoutine->DailyTasks[0].RoutineTask != Task) return;
	RequestedTask = Task;
}

void UMiningRoutineJobComponent::EndRoutineJob(UACFTask* Task)
{
	if (RequestedTask != Task) return;
	RequestedTask = nullptr;
	CancelOperation();
	Status = EMiningRoutineJobStatus::WaitingForRoutine;
}

void UMiningRoutineJobComponent::HandleAIStateChanged(FGameplayTag State)
{
	if (!CanExecute())
	{
		CancelOperation(); // Release only our child tasks; never stop the controller's combat movement/brain.
		Status = EMiningRoutineJobStatus::Suspended;
	}
}

void UMiningRoutineJobComponent::Present(EMiningPresentationState State, const FVector& Focus)
{
	if (Coordinator.IsValid() && AssignedPawn.IsValid()) Coordinator->ApplyRoutinePresentation(AssignedPawn.Get(), Role, State, Focus);
}

void UMiningRoutineJobComponent::CancelOperation()
{
	UMiningSmartObjectInteractionTask* PendingInteraction = InteractionTask;
	UAITask_MoveTo* PendingMove = MoveTask;
	InteractionTask = nullptr;
	MoveTask = nullptr;
	bTimedInteractionHold = false;
	bWorkerRetrieving = false;
	bOperationFinished = false;
	bOperationSucceeded = false;
	if (PendingInteraction) PendingInteraction->OnFinished.RemoveDynamic(this, &ThisClass::HandleInteractionFinished);
	if (PendingMove) PendingMove->OnMoveTaskFinished.RemoveAll(this);
	if (IsValid(PendingInteraction) && !PendingInteraction->IsFinished()) PendingInteraction->ExternalCancel();
	if (IsValid(PendingMove) && !PendingMove->IsFinished()) PendingMove->ExternalCancel();
	Present(EMiningPresentationState::Idle, AssignedPawn.IsValid() ? AssignedPawn->GetActorLocation() : FVector::ZeroVector);
}

void UMiningRoutineJobComponent::HandleInteractionFinished()
{
	// Gameplay tasks mark themselves as garbage on completion. Cache the outcome
	// synchronously so a GC between completion and the next timer cannot lose it.
	bOperationSucceeded = InteractionTask && InteractionTask->DidSucceed();
	bOperationFinished = true;
}

void UMiningRoutineJobComponent::Block(const TCHAR* Reason)
{
	CancelOperation();
	Status = EMiningRoutineJobStatus::Blocked;
	BlockedReason = Reason;
	RetryAt = GetWorld()->GetTimeSeconds() + BlockedRetryDelaySeconds;
}

float UMiningRoutineJobComponent::GetPhaseDuration(EMiningPresentationState State) const
{
	const float ClipDuration = Coordinator.IsValid() && AssignedPawn.IsValid()
		? Coordinator->GetRoutinePresentationDuration(AssignedPawn.Get(), State, InteractionDuration) : InteractionDuration;
	// Work/guard windows may loop; one-shot pickup/deposit must finish one full clip.
	return State == EMiningPresentationState::Working || State == EMiningPresentationState::Guarding
		? FMath::Max(InteractionDuration, ClipDuration) : FMath::Max(.1f, ClipDuration);
}

void UMiningRoutineJobComponent::CompleteStationOperation()
{
	if (Role == EMiningPresentationRole::Worker && !bSecondary && !bWorkerRetrieving)
	{
		// Retain a running primary station claim during pickup; cancellation/combat
		// releases it normally. Animation completion never grants economic resources.
		bWorkerRetrieving = true;
		bTimedInteractionHold = true;
		Deadline = GetWorld()->GetTimeSeconds() + GetPhaseDuration(EMiningPresentationState::Retrieving);
		Status = EMiningRoutineJobStatus::Interacting;
		Present(EMiningPresentationState::Retrieving, SecondaryTarget);
		return;
	}
	CancelOperation();
	bSecondary = !bSecondary;
	Status = EMiningRoutineJobStatus::WaitingForRoutine;
}

void UMiningRoutineJobComponent::UpdateJob()
{
	if (!CanExecute())
	{
		if (InteractionTask || MoveTask || bTimedInteractionHold) CancelOperation();
		Status = EMiningRoutineJobStatus::Suspended;
		return;
	}
	// Keep ACF as the schedule authority. This is idempotent and does not force an AI state.
	Routine->UpdateByTime();
	if (!RequestedTask) { Status = EMiningRoutineJobStatus::WaitingForRoutine; return; }
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now < RetryAt) return;
	const FVector Target = bSecondary ? SecondaryTarget : PrimaryTarget;
	const FVector Focus = bSecondary ? PrimaryTarget : SecondaryTarget;
	const EMiningPresentationState ActiveState = Role == EMiningPresentationRole::Worker
		? (bWorkerRetrieving ? EMiningPresentationState::Retrieving : bSecondary ? EMiningPresentationState::Depositing : EMiningPresentationState::Working)
		: Role == EMiningPresentationRole::Guard ? EMiningPresentationState::Guarding
		: (bSecondary ? EMiningPresentationState::Unloading : EMiningPresentationState::Loading);
	if (bOperationFinished)
	{
		bOperationFinished = false;
		InteractionTask = nullptr;
		MoveTask = nullptr;
		if (!bOperationSucceeded) { Block(TEXT("Duty navigation or interaction failed")); return; }
		if (Role == EMiningPresentationRole::Courier)
		{
			bTimedInteractionHold = true;
			Deadline = Now + GetPhaseDuration(ActiveState);
			Status = EMiningRoutineJobStatus::Interacting;
			Present(ActiveState, Focus);
		}
		else if (!bTimedInteractionHold) CompleteStationOperation();
		return;
	}
	if (bTimedInteractionHold)
	{
		if (Now >= Deadline) CompleteStationOperation();
		else Present(ActiveState, Focus);
		return;
	}
	if (InteractionTask)
	{
		if (InteractionTask->IsFinished())
		{
			const bool bSuccess = InteractionTask->DidSucceed();
			InteractionTask = nullptr;
			if (bSuccess) CompleteStationOperation();
			else Block(TEXT("Workstation interaction or navigation failed"));
			return;
		}
		const bool bAtStation = InteractionTask->IsInteracting();
		if (!bAtStation && Now >= Deadline) { Block(TEXT("Travel timed out")); return; }
		if (bAtStation && Status != EMiningRoutineJobStatus::Interacting) Deadline = Now + GetPhaseDuration(ActiveState);
		if (bAtStation && Now >= Deadline)
		{
			// Presentation trees may intentionally remain Running (the test trees have
			// no exit transition). The routine owns the timed work window, not the tree.
			// Earlier terminal failures are handled above. Ending this session cancels
			// its context/claim; it does NOT report StateTree success or grant resources.
			CompleteStationOperation();
			return;
		}
		Status = bAtStation ? EMiningRoutineJobStatus::Interacting : EMiningRoutineJobStatus::Traveling;
		Present(bAtStation ? ActiveState : EMiningPresentationState::Traveling, bAtStation ? Focus : Target);
		return;
	}
	if (MoveTask)
	{
		if (!MoveTask->IsFinished()) { if (Now >= Deadline) Block(TEXT("Courier travel timed out")); return; }
		const bool bArrived = MoveTask->WasMoveSuccessful();
		MoveTask = nullptr;
		if (!bArrived) { Block(TEXT("No complete courier navigation path")); return; }
		bTimedInteractionHold = true;
		Deadline = Now + GetPhaseDuration(ActiveState); // Work starts at arrival, not move creation.
		Status = EMiningRoutineJobStatus::Interacting;
		Present(ActiveState, Focus);
		return;
	}
	AACFAIController* Controller = CastChecked<AACFAIController>(GetOwner());
	BlockedReason.Reset();
	Deadline = Now + NavigationTimeoutSeconds; // Independent from the short workstation animation duration.
	if (Role == EMiningPresentationRole::Courier)
	{
		FAIMoveRequest Request(Target);
		Request.SetUsePathfinding(true);
		Request.SetAllowPartialPath(false);
		Request.SetAcceptanceRadius(60.0f);
		MoveTask = UAITask::NewAITask<UAITask_MoveTo>(*Controller, EAITaskPriority::High);
		if (!MoveTask) { Block(TEXT("Cannot create courier movement task")); return; }
		MoveTask->SetUp(Controller, Request);
		MoveTask->OnMoveTaskFinished.AddWeakLambda(this, [this](TEnumAsByte<EPathFollowingResult::Type> Result, AAIController*)
		{
			bOperationSucceeded = Result == EPathFollowingResult::Success;
			bOperationFinished = true;
		});
		Present(EMiningPresentationState::Traveling, Target);
		Status = EMiningRoutineJobStatus::Traveling;
		MoveTask->ReadyForActivation();
		return;
	}
	USmartObjectSubsystem* Subsystem = USmartObjectSubsystem::GetCurrent(GetWorld());
	if (!Subsystem || !Station.IsValid() || !Station->IsSmartObjectEnabled()) { Block(TEXT("Workstation missing or disabled")); return; }
	if (!Station->GetRegisteredHandle().IsValid()) { Block(TEXT("Workstation is not registered with SmartObjectSubsystem")); return; }
	TArray<FSmartObjectSlotHandle> AllSlots, AvailableSlots;
	Subsystem->GetAllSlots(Station->GetRegisteredHandle(), AllSlots);
	FSmartObjectRequestFilter Filter;
	Filter.BehaviorDefinitionClasses.Add(UGameplayInteractionSmartObjectBehaviorDefinition::StaticClass());
	Subsystem->FindSlots(Station->GetRegisteredHandle(), Filter, AvailableSlots);
	const int32 SlotIndex = bSecondary ? 1 : 0;
	// Do not silently perform another duty when the requested slot is busy/misconfigured.
	if (!AllSlots.IsValidIndex(SlotIndex) || !AvailableSlots.Contains(AllSlots[SlotIndex])) { Block(TEXT("Required workstation slot unavailable (primary=0, secondary=1)")); return; }
	const FSmartObjectClaimHandle Claim = USmartObjectBlueprintFunctionLibrary::MarkSmartObjectSlotAsClaimed(this, AllSlots[SlotIndex], AssignedPawn.Get(), ESmartObjectClaimPriority::Normal);
	if (!Claim.IsValid()) { Block(TEXT("Workstation claim failed")); return; }
	InteractionTask = UMiningSmartObjectInteractionTask::MoveToAndUseClaimedSlot(Controller, Claim, true, false, InteractionDuration);
	if (!InteractionTask)
	{
		USmartObjectBlueprintFunctionLibrary::MarkSmartObjectSlotAsFree(this, Claim);
		Block(TEXT("Workstation requires a compiled GameplayInteraction StateTree"));
		return;
	}
	Present(EMiningPresentationState::Traveling, Target);
	Status = EMiningRoutineJobStatus::Traveling;
	InteractionTask->OnFinished.AddDynamic(this, &ThisClass::HandleInteractionFinished);
	InteractionTask->ReadyForActivation();
}

void UMiningRoutineJobComponent::ReleaseAssignment()
{
	if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(UpdateTimer);
	if (AACFAIController* Controller = Cast<AACFAIController>(GetOwner())) Controller->OnAIStateChanged.RemoveDynamic(this, &ThisClass::HandleAIStateChanged);
	RequestedTask = nullptr;
	CancelOperation();
	if (Routine.IsValid() && RuntimeRoutine && Routine->GetRoutineDataAsset() == RuntimeRoutine)
	{
		Routine->StopAll();
		Routine->SetRoutineDataAsset(OriginalRoutine); // Do not overwrite an externally replaced schedule.
	}
	Routine.Reset();
	RuntimeRoutine = nullptr;
	OriginalRoutine = nullptr;
	Coordinator.Reset();
	AssignedPawn.Reset();
	Station.Reset();
	Status = EMiningRoutineJobStatus::Unassigned;
	BlockedReason.Reset();
	RetryAt = 0.0;
	bSecondary = false;
}

void UMiningRoutineJobComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	ReleaseAssignment();
	Super::EndPlay(Reason);
}
