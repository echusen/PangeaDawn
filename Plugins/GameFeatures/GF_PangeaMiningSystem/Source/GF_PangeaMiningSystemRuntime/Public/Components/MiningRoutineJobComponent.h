#pragma once

#include "CoreMinimal.h"

#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"

#include "Interfaces/MiningPresentationAgentInterface.h"

#include "MiningRoutineJobComponent.generated.h"

class APawn;
class UACFAIRoutineComponent;
class UACFAIRoutineDataAsset;
class UACFTask;
class UAITask_MoveTo;
class UMiningSitePresentationCoordinatorComponent;
class UMiningSmartObjectInteractionTask;
class USmartObjectComponent;

/** Diagnostic state of the mining duty that a UMiningRoutineJobComponent runs for one AI controller. */
UENUM(BlueprintType)
enum class EMiningRoutineJobStatus : uint8
{
	/** No duty is assigned to this controller. */
	Unassigned,
	/** A duty is assigned, but the ACF routine has not requested it or the previous operation just ended. */
	WaitingForRoutine,
	/** The controller cannot execute the duty right now (for example combat, a command or death); this job's own tasks are cancelled. */
	Suspended,
	/** Moving towards the current duty target. */
	Traveling,
	/** At the duty target, inside the timed work window. */
	Interacting,
	/** The last attempt failed; it is retried after the blocked-retry delay. See GetBlockedReason. */
	Blocked
};

/** Executes an ACF-requested duty, not an AI brain or a source of economic rewards. Server only. */
UCLASS(ClassGroup = (AI), BlueprintType)
class GF_PANGEAMININGSYSTEMRUNTIME_API UMiningRoutineJobComponent final : public UActorComponent
{
	GENERATED_BODY()

public:
	UMiningRoutineJobComponent();

	// Site coordinator configures assignments; the ACF routine task starts/stops execution.
	bool ConfigureAssignment(UMiningSitePresentationCoordinatorComponent* Coordinator,
		APawn* Pawn,
		EMiningPresentationRole Role,
		USmartObjectComponent* Station,
		const FVector& PrimaryTarget,
		const FVector& SecondaryTarget,
		float InteractionDuration,
		float NavigationTimeoutSeconds = 120.0f,
		float BlockedRetryDelaySeconds = 3.0f);
	void BeginRoutineJob(UACFTask* Task);
	void EndRoutineJob(UACFTask* Task);

	/** Cancels the running operation, gives the routine component back the routine it had before the assignment (unless it was replaced externally) and clears the assignment. */
	UFUNCTION(BlueprintCallable, Category = "Assignment")
	void ReleaseAssignment();

	/** Current diagnostic state of the assigned duty. */
	UFUNCTION(BlueprintPure, Category = "Diagnostics")
	EMiningRoutineJobStatus GetJobStatus() const
	{
		return Status;
	}

	/** Why the duty last entered the Blocked state; empty once a new attempt has started. */
	UFUNCTION(BlueprintPure, Category = "Diagnostics")
	FString GetBlockedReason() const
	{
		return BlockedReason;
	}

private:
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	UFUNCTION()
	void HandleAIStateChanged(FGameplayTag State);
	UFUNCTION()
	void HandleInteractionFinished();
	void UpdateJob();
	void CompleteStationOperation();
	float GetPhaseDuration(EMiningPresentationState State) const;
	void CancelOperation();
	void Block(const TCHAR* Reason);
	void Present(EMiningPresentationState State, const FVector& Focus);
	bool CanExecute() const;

	UPROPERTY(Transient)
	TObjectPtr<UACFAIRoutineDataAsset> RuntimeRoutine;
	UPROPERTY(Transient)
	TObjectPtr<UACFAIRoutineDataAsset> OriginalRoutine;
	UPROPERTY(Transient)
	TObjectPtr<UMiningSmartObjectInteractionTask> InteractionTask;
	UPROPERTY(Transient)
	TObjectPtr<UAITask_MoveTo> MoveTask;
	UPROPERTY(Transient)
	TObjectPtr<UACFTask> RequestedTask;
	TWeakObjectPtr<UACFAIRoutineComponent> Routine;
	TWeakObjectPtr<UMiningSitePresentationCoordinatorComponent> Coordinator;
	TWeakObjectPtr<APawn> AssignedPawn;
	TWeakObjectPtr<USmartObjectComponent> Station;
	FTimerHandle UpdateTimer;
	FVector PrimaryTarget = FVector::ZeroVector;
	FVector SecondaryTarget = FVector::ZeroVector;
	EMiningPresentationRole Role = EMiningPresentationRole::Worker;
	EMiningRoutineJobStatus Status = EMiningRoutineJobStatus::Unassigned;
	FString BlockedReason;
	double Deadline = 0.0;
	double RetryAt = 0.0;
	float InteractionDuration = 1.0f;
	float NavigationTimeoutSeconds = 120.0f;
	float BlockedRetryDelaySeconds = 3.0f;
	bool bSecondary = false;
	bool bTimedInteractionHold = false;
	bool bWorkerRetrieving = false;
	bool bOperationFinished = false;
	bool bOperationSucceeded = false;
};
