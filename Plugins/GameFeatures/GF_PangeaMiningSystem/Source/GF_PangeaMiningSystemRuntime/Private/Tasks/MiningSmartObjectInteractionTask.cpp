#include "Tasks/MiningSmartObjectInteractionTask.h"

#include "AIController.h"
#include "GameplayInteractionSmartObjectBehaviorDefinition.h"
#include "GameFramework/Pawn.h"
#include "SmartObjectComponent.h"
#include "SmartObjectSubsystem.h"
#include "StateTree.h"
#include "Tasks/AITask_MoveTo.h"

UMiningSmartObjectInteractionTask::UMiningSmartObjectInteractionTask(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bTickingTask = true;
}

UMiningSmartObjectInteractionTask* UMiningSmartObjectInteractionTask::MoveToAndUseClaimedSlot(
	AAIController* Controller, FSmartObjectClaimHandle ClaimHandle, bool bMoveToSlot, bool bLockAILogic, float MinimumInteractionDuration)
{
	if (!IsValid(Controller) || !Controller->HasAuthority() || !IsValid(Controller->GetPawn()) || !ClaimHandle.IsValid()) return nullptr;
	USmartObjectSubsystem* Subsystem = USmartObjectSubsystem::GetCurrent(Controller->GetWorld());
	if (!Subsystem || !Subsystem->IsClaimedSmartObjectValid(ClaimHandle) || Subsystem->GetSlotState(ClaimHandle.SlotHandle) != ESmartObjectSlotState::Claimed) return nullptr;
	const UGameplayInteractionSmartObjectBehaviorDefinition* Behavior = Subsystem->GetBehaviorDefinition<UGameplayInteractionSmartObjectBehaviorDefinition>(ClaimHandle);
	if (!Behavior || !Behavior->GetStateTree() || !Behavior->GetStateTree()->IsReadyToRun()) return nullptr;

	UMiningSmartObjectInteractionTask* Task = NewAITask<UMiningSmartObjectInteractionTask>(*Controller, EAITaskPriority::High);
	if (!Task) return nullptr;
	Task->ClaimedHandle = ClaimHandle;
	Task->SmartObjectSubsystem = Subsystem;
	Task->InteractionPawn = Controller->GetPawn();
	Task->bReachSlot = bMoveToSlot;
	Task->MinimumInteractionDuration = FMath::Max(0.0f, MinimumInteractionDuration);
	if (bLockAILogic) Task->RequestAILogicLocking();
	return Task;
}

void UMiningSmartObjectInteractionTask::Activate()
{
	Super::Activate();
	USmartObjectSubsystem* Subsystem = SmartObjectSubsystem.Get();
	if (!IsValid(OwnerController) || !InteractionPawn.IsValid() || OwnerController->GetPawn() != InteractionPawn.Get() || !Subsystem || !Subsystem->IsClaimedSmartObjectValid(ClaimedHandle) ||
		Subsystem->GetSlotState(ClaimedHandle.SlotHandle) != ESmartObjectSlotState::Claimed)
	{
		EndTask();
		return;
	}
	Subsystem->RegisterSlotInvalidationCallback(ClaimedHandle,
		FOnSlotInvalidated::CreateUObject(this, &ThisClass::HandleSlotInvalidated));
	bInvalidationRegistered = true;
	if (!bReachSlot)
	{
		if (!StartInteraction() && !IsFinished()) EndTask();
		return;
	}
	const TOptional<FVector> Location = Subsystem->GetSlotLocation(ClaimedHandle);
	if (!Location.IsSet())
	{
		EndTask();
		return;
	}
	FAIMoveRequest Request(Location.GetValue());
	Request.SetUsePathfinding(true);
	Request.SetAllowPartialPath(false);
	MoveToTask = NewAITask<UAITask_MoveTo>(*OwnerController, *this, EAITaskPriority::High, TEXT("MiningSmartObject"));
	if (!MoveToTask)
	{
		EndTask();
		return;
	}
	MoveToTask->SetUp(OwnerController, Request);
	MoveToTask->ReadyForActivation(); // May fail/finish synchronously.
}

bool UMiningSmartObjectInteractionTask::StartInteraction()
{
	USmartObjectSubsystem* Subsystem = SmartObjectSubsystem.Get();
	if (!Subsystem || !IsValid(OwnerController) || !InteractionPawn.IsValid() || OwnerController->GetPawn() != InteractionPawn.Get()) return false;
	const USmartObjectComponent* Component = Subsystem->GetSmartObjectComponent(ClaimedHandle);
	if (!IsValid(Component) || !IsValid(Component->GetOwner())) return false;
	const UGameplayInteractionSmartObjectBehaviorDefinition* Behavior = Subsystem->MarkSlotAsOccupied<UGameplayInteractionSmartObjectBehaviorDefinition>(ClaimedHandle);
	if (!Behavior || !Behavior->StateTreeReference.IsValid()) return false;
	InteractionContext.SetContextActor(OwnerController->GetPawn());
	InteractionContext.SetSmartObjectActor(Component->GetOwner());
	InteractionContext.SetClaimedHandle(ClaimedHandle);
	bContextActivated = true; // Cancellation can occur inside StateTree entry callbacks.
	if (!InteractionContext.Activate(*Behavior))
	{
		bContextActivated = false;
		return false;
	}
	return !IsFinished();
}

void UMiningSmartObjectInteractionTask::TickTask(float DeltaTime)
{
	Super::TickTask(DeltaTime);
	if (bFinishing || IsFinished()) return;
	if (!IsValid(OwnerController) || !InteractionPawn.IsValid() || OwnerController->GetPawn() != InteractionPawn.Get())
	{
		Abort(EGameplayInteractionAbortReason::ExternalAbort);
		return;
	}
	if (MoveToTask || !bContextActivated) return;
	if (InteractionElapsed == 0.0f)
		UE_LOG(LogTemp, Verbose, TEXT("Mining interaction runtime tick after arrival. Task=%s Pawn=%s Delta=%.3f"), *GetName(), *GetNameSafe(InteractionPawn.Get()), DeltaTime);
	InteractionElapsed += FMath::Max(0.0f, DeltaTime);
	const bool bRunning = !bInteractionCompleted && InteractionContext.Tick(DeltaTime);
	if (bFinishing || IsFinished()) return;
	if (!bRunning)
	{
		bInteractionCompleted = true;
		// Keep the claim occupied through the minimum presentation hold, but never
		// hold a failed StateTree or tick a tree again after its terminal status.
		if (InteractionContext.GetLastRunStatus() != EStateTreeRunStatus::Succeeded || InteractionElapsed >= MinimumInteractionDuration) EndTask();
	}
}

void UMiningSmartObjectInteractionTask::OnGameplayTaskDeactivated(UGameplayTask& Task)
{
	if (MoveToTask == &Task && Task.IsFinished())
	{
		const bool bReachedSlot = MoveToTask->WasMoveSuccessful();
		MoveToTask = nullptr; // Clear before interaction entry/cancellation callbacks.
		if (!bFinishing && !IsFinished() && (!bReachedSlot || !StartInteraction())) EndTask();
	}
	Super::OnGameplayTaskDeactivated(Task);
}

void UMiningSmartObjectInteractionTask::ExternalCancel()
{
	RequestAbort();
}

void UMiningSmartObjectInteractionTask::RequestAbort()
{
	Abort(EGameplayInteractionAbortReason::ExternalAbort);
}

void UMiningSmartObjectInteractionTask::Abort(EGameplayInteractionAbortReason Reason)
{
	if (bFinishing || IsFinished()) return;
	AbortContext.Reason = Reason;
	EndTask(); // All exits use the same idempotent cleanup path, including before activation.
}

void UMiningSmartObjectInteractionTask::HandleSlotInvalidated(const FSmartObjectClaimHandle& Handle, ESmartObjectSlotState State)
{
	bSlotInvalidated = true; // The subsystem owns the in-progress invalidation/release.
	Abort(EGameplayInteractionAbortReason::InternalAbort);
}

void UMiningSmartObjectInteractionTask::OnDestroy(bool bInOwnerFinished)
{
	if (bFinishing) return;
	bFinishing = true;
	bSucceeded = !bInOwnerFinished && bInteractionCompleted && AbortContext.Reason == EGameplayInteractionAbortReason::Unset &&
		InteractionContext.GetLastRunStatus() == EStateTreeRunStatus::Succeeded;
	if (!bInteractionCompleted && AbortContext.Reason == EGameplayInteractionAbortReason::Unset)
		AbortContext.Reason = EGameplayInteractionAbortReason::ExternalAbort;
	if (MoveToTask)
	{
		UAITask_MoveTo* PendingMove = MoveToTask;
		MoveToTask = nullptr;
		PendingMove->ExternalCancel();
	}
	if (bContextActivated)
	{
		InteractionContext.SetAbortContext(AbortContext);
		InteractionContext.Deactivate();
		bContextActivated = false;
	}
	if (USmartObjectSubsystem* Subsystem = SmartObjectSubsystem.Get(); Subsystem && Subsystem->IsClaimedSmartObjectValid(ClaimedHandle))
	{
		if (bInvalidationRegistered) Subsystem->UnregisterSlotInvalidationCallback(ClaimedHandle);
		const ESmartObjectSlotState State = Subsystem->GetSlotState(ClaimedHandle.SlotHandle);
		if (!bSlotInvalidated && (State == ESmartObjectSlotState::Claimed || State == ESmartObjectSlotState::Occupied))
			Subsystem->MarkSlotAsFree(ClaimedHandle);
	}
	ClaimedHandle.Invalidate();
	Super::OnDestroy(bInOwnerFinished); // Mark finished/release AI resources before user callbacks.
	if (bSucceeded) OnSucceeded.Broadcast();
	else OnFailed.Broadcast();
	OnFinished.Broadcast();
}
