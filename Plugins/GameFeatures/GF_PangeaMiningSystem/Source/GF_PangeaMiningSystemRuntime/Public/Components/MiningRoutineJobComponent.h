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

UENUM(BlueprintType)
enum class EMiningRoutineJobStatus : uint8
{
	Unassigned, WaitingForRoutine, Suspended, Traveling, Interacting, Blocked
};

/** Executes an ACF-requested duty, not an AI brain or a source of economic rewards. Server only. */
UCLASS(ClassGroup=(AI), BlueprintType)
class GF_PANGEAMININGSYSTEMRUNTIME_API UMiningRoutineJobComponent final : public UActorComponent
{
	GENERATED_BODY()

public:
	UMiningRoutineJobComponent();

	// Site coordinator configures assignments; the ACF routine task starts/stops execution.
	bool ConfigureAssignment(UMiningSitePresentationCoordinatorComponent* Coordinator, APawn* Pawn,
		EMiningPresentationRole Role, USmartObjectComponent* Station,
		const FVector& PrimaryTarget, const FVector& SecondaryTarget, float InteractionDuration,
		float NavigationTimeoutSeconds = 120.0f, float BlockedRetryDelaySeconds = 3.0f);
	void BeginRoutineJob(UACFTask* Task);
	void EndRoutineJob(UACFTask* Task);

	UFUNCTION(BlueprintCallable, Category="Assignment")
	void ReleaseAssignment();

	UFUNCTION(BlueprintPure, Category="Diagnostics")
	EMiningRoutineJobStatus GetJobStatus() const { return Status; }

	UFUNCTION(BlueprintPure, Category="Diagnostics")
	FString GetBlockedReason() const { return BlockedReason; }

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
