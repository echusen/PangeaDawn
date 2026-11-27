#pragma once

#include "CoreMinimal.h"
#include "GameplayInteractionContext.h"
#include "Tasks/AITask.h"
#include "MiningSmartObjectInteractionTask.generated.h"

class AAIController;
class APawn;
class UAITask_MoveTo;
class USmartObjectSubsystem;

/** Mining's move/use lifecycle glue. Depends only on public Unreal APIs. */
UCLASS()
class UMiningSmartObjectInteractionTask final : public UAITask
{
	GENERATED_BODY()

public:
	UMiningSmartObjectInteractionTask(const FObjectInitializer& ObjectInitializer);

	/** Null leaves claim ownership with the caller. Otherwise ReadyForActivation must follow. */
	UFUNCTION(BlueprintCallable, Category = "Interaction", meta = (BlueprintInternalUseOnly = "true", DefaultToSelf = "Controller"))
	static UMiningSmartObjectInteractionTask* MoveToAndUseClaimedSlot(AAIController* Controller,
		FSmartObjectClaimHandle ClaimHandle,
		bool bMoveToSlot = true,
		bool bLockAILogic = true,
		float MinimumInteractionDuration = 0.0f);

	/** True while the gameplay interaction context of the claimed slot is active. */
	UFUNCTION(BlueprintPure, Category = "Interaction")
	bool IsInteracting() const
	{
		return bContextActivated;
	}

	/** Ends the task as an external abort. Does nothing when the task is already finishing or finished. */
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void RequestAbort();

	virtual void ExternalCancel() override;

	/** Broadcast when the task ends after the interaction completed successfully. */
	UPROPERTY(BlueprintAssignable, Category = "Interaction")
	FGenericGameplayTaskDelegate OnSucceeded;

	/** Broadcast when the task ends without a successful interaction. */
	UPROPERTY(BlueprintAssignable, Category = "Interaction")
	FGenericGameplayTaskDelegate OnFailed;

	/** Broadcast whenever the task ends, after OnSucceeded or OnFailed. */
	UPROPERTY(BlueprintAssignable, Category = "Interaction")
	FGenericGameplayTaskDelegate OnFinished;

	/** True once the task has ended with a successful interaction. */
	UFUNCTION(BlueprintPure, Category = "Interaction")
	bool DidSucceed() const
	{
		return bSucceeded;
	}

protected:
	virtual void Activate() override;
	virtual void TickTask(float DeltaTime) override;
	virtual void OnGameplayTaskDeactivated(UGameplayTask& Task) override;
	virtual void OnDestroy(bool bInOwnerFinished) override;

private:
	UPROPERTY(Transient)
	FGameplayInteractionContext InteractionContext;

	UPROPERTY(Transient)
	TObjectPtr<UAITask_MoveTo> MoveToTask;

	UPROPERTY(Transient)
	FSmartObjectClaimHandle ClaimedHandle;

	TWeakObjectPtr<USmartObjectSubsystem> SmartObjectSubsystem;
	TWeakObjectPtr<APawn> InteractionPawn;
	bool bReachSlot = true;
	bool bContextActivated = false;
	bool bInteractionCompleted = false;
	bool bFinishing = false;
	bool bSucceeded = false;
	bool bInvalidationRegistered = false;
	bool bSlotInvalidated = false;
	float MinimumInteractionDuration = 0.0f;
	float InteractionElapsed = 0.0f;
	FGameplayInteractionAbortContext AbortContext;

	bool StartInteraction();
	void Abort(EGameplayInteractionAbortReason Reason);
	void HandleSlotInvalidated(const FSmartObjectClaimHandle& Handle, ESmartObjectSlotState State);
};
