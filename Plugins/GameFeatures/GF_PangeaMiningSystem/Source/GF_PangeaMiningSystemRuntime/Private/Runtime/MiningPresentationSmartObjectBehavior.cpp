#include "Components/MiningSitePresentationCoordinatorComponent.h"

#include "Actors/MiningSiteActor.h"
#include "Tasks/MiningSmartObjectInteractionTask.h"
#include "AIController.h"
#include "Components/MiningSiteComponent.h"
#include "Components/MiningRoutineJobComponent.h"
#include "SmartObjectComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameplayInteractionSmartObjectBehaviorDefinition.h"
#include "SmartObjectBlueprintFunctionLibrary.h"
#include "SmartObjectRequestTypes.h"
#include "SmartObjectSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogPangeaMiningPresentationSmartObject, Log, All);

bool UMiningSitePresentationCoordinatorComponent::HasSmartObjectInteractionTimedOut(AActor* Actor, float MaxDuration) const
{
	if (!Actor || MaxDuration <= 0.0f)
	{
		return false;
	}

	const TObjectPtr<UMiningSmartObjectInteractionTask>* ExistingTask = PresentationSmartObjectTaskMap.Find(Actor);
	if (!ExistingTask || !ExistingTask->Get() || ExistingTask->Get()->IsFinished())
	{
		return false;
	}

	const float* StartTime = PresentationSmartObjectStartTimeMap.Find(Actor);
	if (!StartTime || !GetWorld())
	{
		return false;
	}

	return (GetWorld()->GetTimeSeconds() - *StartTime) >= MaxDuration;
}

bool UMiningSitePresentationCoordinatorComponent::TryStartSmartObjectBehavior(AMiningSiteActor& SiteActor, AActor* Actor, USmartObjectComponent* SmartObjectComponent, EMiningPresentationRole Role, int32 PreferredSlotIndex)
{
	const auto SetSmartObjectStatus = [this, &SiteActor, Actor](const TCHAR* Status)
	{
		if (!Actor)
		{
			return;
		}

		const FString NewStatus(Status);
		if (const FString* ExistingStatus = PresentationSmartObjectLastStatusMap.Find(Actor))
		{
			if (*ExistingStatus == NewStatus)
			{
				return;
			}
		}

		PresentationSmartObjectLastStatusMap.Add(Actor, NewStatus);
		UE_LOG(LogPangeaMiningPresentationSmartObject, Log, TEXT("Mining site GI status. Site=%s Actor=%s Status=%s"), *GetNameSafe(&SiteActor), *GetNameSafe(Actor), *NewStatus);
	};

	if (bClearingSmartObjectTasks || !SiteActor.HasAuthority() || !Actor || !SmartObjectComponent || !SmartObjectComponent->IsSmartObjectEnabled())
	{
		SetSmartObjectStatus(TEXT("SmartObject disabled"));
		return false;
	}

	if (TObjectPtr<UMiningSmartObjectInteractionTask>* ExistingTask = PresentationSmartObjectTaskMap.Find(Actor))
	{
		if (ExistingTask->Get() && !ExistingTask->Get()->IsFinished())
		{
			SetSmartObjectStatus(TEXT("GameplayInteraction active"));
			return true;
		}

		const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
		const float StartedAt = PresentationSmartObjectStartTimeMap.FindRef(Actor);
		const float HoldUntil = FMath::Max(Now, StartedAt + GetInteractionDurationForActor(SiteActor.MiningSiteComponent, Role, Actor));
		PresentationSmartObjectTaskMap.Remove(Actor);
		PresentationClaimHandleMap.Remove(Actor);
		PresentationSmartObjectStartTimeMap.Remove(Actor);
		if (HoldUntil > Now)
		{
			PresentationHoldUntilTimeMap.Add(Actor, HoldUntil);
			SetSmartObjectStatus(TEXT("GameplayInteraction presentation hold"));
			return true;
		}

		PresentationHoldUntilTimeMap.Remove(Actor);
		PresentationRoutePhaseMap.Add(Actor, !PresentationRoutePhaseMap.FindRef(Actor));
		SetSmartObjectStatus(TEXT("GameplayInteraction finished"));
		return false;
	}

	if (HasPresentationHoldActive(Actor))
	{
		SetSmartObjectStatus(TEXT("GameplayInteraction presentation hold"));
		return true;
	}

	PresentationHoldUntilTimeMap.Remove(Actor);

	APawn* Pawn = Cast<APawn>(Actor);
	AAIController* AIController = Pawn ? Cast<AAIController>(Pawn->GetController()) : nullptr;
	if (!Pawn || !AIController)
	{
		SetSmartObjectStatus(TEXT("Missing pawn or AI controller"));
		return false;
	}

	USmartObjectSubsystem* SmartObjectSubsystem = USmartObjectSubsystem::GetCurrent(GetWorld());
	if (!SmartObjectSubsystem)
	{
		SetSmartObjectStatus(TEXT("Missing SmartObject subsystem"));
		return false;
	}

	const FSmartObjectHandle RegisteredHandle = SmartObjectComponent->GetRegisteredHandle();
	if (!RegisteredHandle.IsValid())
	{
		SetSmartObjectStatus(TEXT("SmartObject handle invalid"));
		return false;
	}

	TArray<FSmartObjectSlotHandle> AllSlots;
	SmartObjectSubsystem->GetAllSlots(RegisteredHandle, AllSlots);

	TArray<FSmartObjectSlotHandle> RequestSlots;
	FSmartObjectRequestFilter Filter;
	Filter.BehaviorDefinitionClasses.Add(UGameplayInteractionSmartObjectBehaviorDefinition::StaticClass());
	SmartObjectSubsystem->FindSlots(RegisteredHandle, Filter, RequestSlots);
	const bool bUseSecondarySlot = PresentationRoutePhaseMap.FindRef(Actor);

	auto TryClaimFromSlots = [this, Pawn, bUseSecondarySlot, PreferredSlotIndex](const TArray<FSmartObjectSlotHandle>& CandidateSlots, int32& OutChosenIndex) -> FSmartObjectClaimHandle
	{
		if (CandidateSlots.IsEmpty())
		{
			return FSmartObjectClaimHandle();
		}

		TArray<int32> SlotOrder;
		SlotOrder.Reserve(CandidateSlots.Num());
		const int32 PreferredIndex = PreferredSlotIndex != INDEX_NONE
			? FMath::Clamp(PreferredSlotIndex, 0, CandidateSlots.Num() - 1)
			: ((CandidateSlots.Num() > 1 && bUseSecondarySlot) ? 1 : 0);
		SlotOrder.Add(PreferredIndex);
		for (int32 Index = 0; Index < CandidateSlots.Num(); ++Index)
		{
			if (Index != PreferredIndex)
			{
				SlotOrder.Add(Index);
			}
		}

		for (const int32 CandidateIndex : SlotOrder)
		{
			const FSmartObjectClaimHandle CandidateClaimHandle = USmartObjectBlueprintFunctionLibrary::MarkSmartObjectSlotAsClaimed(this, CandidateSlots[CandidateIndex], Pawn, ESmartObjectClaimPriority::Normal);
			if (CandidateClaimHandle.IsValid())
			{
				OutChosenIndex = CandidateIndex;
				return CandidateClaimHandle;
			}
		}

		return FSmartObjectClaimHandle();
	};

	int32 ChosenSlotIndex = INDEX_NONE;
	const FSmartObjectClaimHandle ClaimHandle = TryClaimFromSlots(RequestSlots, ChosenSlotIndex);

	if (!ClaimHandle.IsValid())
	{
		FSmartObjectRequestFilter NoConditionFilter = Filter;
		NoConditionFilter.bShouldEvaluateConditions = false;
		TArray<FSmartObjectSlotHandle> NoConditionSlots;
		SmartObjectSubsystem->FindSlots(RegisteredHandle, NoConditionFilter, NoConditionSlots);
		if (!NoConditionSlots.IsEmpty())
		{
			SetSmartObjectStatus(*FString::Printf(TEXT("No SmartObject request results (conditions failed, total_slots=%d)"), AllSlots.Num()));
			return false;
		}

		FSmartObjectRequestFilter NoBehaviorClassFilter = NoConditionFilter;
		NoBehaviorClassFilter.BehaviorDefinitionClasses.Reset();
		TArray<FSmartObjectSlotHandle> NoBehaviorClassSlots;
		SmartObjectSubsystem->FindSlots(RegisteredHandle, NoBehaviorClassFilter, NoBehaviorClassSlots);
		if (!NoBehaviorClassSlots.IsEmpty())
		{
			SetSmartObjectStatus(*FString::Printf(TEXT("No SmartObject request results (behavior class mismatch, total_slots=%d)"), AllSlots.Num()));
			return false;
		}

		SetSmartObjectStatus(*FString::Printf(TEXT("No SmartObject request results (total_slots=%d)"), AllSlots.Num()));
		return false;
	}

	UMiningSmartObjectInteractionTask* SmartObjectTask = UMiningSmartObjectInteractionTask::MoveToAndUseClaimedSlot(AIController, ClaimHandle, true, true);
	if (!SmartObjectTask)
	{
		USmartObjectBlueprintFunctionLibrary::MarkSmartObjectSlotAsFree(this, ClaimHandle);
		SetSmartObjectStatus(TEXT("GameplayInteraction task creation failed"));
		return false;
	}

	PresentationClaimHandleMap.Add(Actor, ClaimHandle);
	PresentationSmartObjectTaskMap.Add(Actor, SmartObjectTask);
	PresentationSmartObjectStartTimeMap.Add(Actor, GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f);
	PresentationMoveTargetMap.Remove(Actor);
	SmartObjectTask->ReadyForActivation();
	SetSmartObjectStatus(*FString::Printf(TEXT("GameplayInteraction started slot=%d results=%d total_slots=%d"), ChosenSlotIndex, RequestSlots.Num(), AllSlots.Num()));
	return true;
}

void UMiningSitePresentationCoordinatorComponent::ReleaseSmartObjectForActor(AActor* Actor)
{
	if (!Actor)
	{
		return;
	}

	// Once a task accepted a claim it owns cleanup, including synchronous completion.
	// A finished/GC-cleared task entry is not permission to release its old claim again.
	const bool bTaskOwnedClaim = PresentationSmartObjectTaskMap.Contains(Actor);
	if (TObjectPtr<UMiningSmartObjectInteractionTask>* ExistingTask = PresentationSmartObjectTaskMap.Find(Actor))
	{
		if (ExistingTask->Get() && !ExistingTask->Get()->IsFinished())
		{
			ExistingTask->Get()->ExternalCancel();
		}
	}

	if (!bTaskOwnedClaim)
	{
		if (const FSmartObjectClaimHandle* ExistingClaim = PresentationClaimHandleMap.Find(Actor))
		{
			if (ExistingClaim->IsValid())
			{
				USmartObjectBlueprintFunctionLibrary::MarkSmartObjectSlotAsFree(this, *ExistingClaim);
			}
		}
	}

	PresentationSmartObjectTaskMap.Remove(Actor);
	PresentationClaimHandleMap.Remove(Actor);
	PresentationSmartObjectLastStatusMap.Remove(Actor);
	PresentationSmartObjectStartTimeMap.Remove(Actor);
	PresentationHoldUntilTimeMap.Remove(Actor);
}

void UMiningSitePresentationCoordinatorComponent::CancelAllSmartObjectTasks()
{
	for (const TWeakObjectPtr<UMiningRoutineJobComponent>& Job : RoutineJobs)
		if (Job.IsValid()) Job->ReleaseAssignment();
	RoutineJobs.Reset();
	TGuardValue<bool> Guard(bClearingSmartObjectTasks, true);
	TArray<TObjectPtr<UMiningSmartObjectInteractionTask>> Tasks;
	PresentationSmartObjectTaskMap.GenerateValueArray(Tasks);
	PresentationSmartObjectTaskMap.Reset();
	// Tasks own their accepted claims, even when the actor key is already invalid.
	for (UMiningSmartObjectInteractionTask* Task : Tasks)
	{
		if (IsValid(Task) && !Task->IsFinished()) Task->ExternalCancel();
	}
	PresentationClaimHandleMap.Reset();
	PresentationSmartObjectStartTimeMap.Reset();
	PresentationSmartObjectLastStatusMap.Reset();
	PresentationHoldUntilTimeMap.Reset();
}

void UMiningSitePresentationCoordinatorComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CancelAllSmartObjectTasks();
	Super::EndPlay(EndPlayReason);
}
