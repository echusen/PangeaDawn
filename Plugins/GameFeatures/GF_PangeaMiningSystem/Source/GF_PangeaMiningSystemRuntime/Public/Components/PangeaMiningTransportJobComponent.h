#pragma once

#include "CoreMinimal.h"

#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"

#include "Interfaces/MiningPresentationAgentInterface.h"

#include "PangeaMiningTransportJobComponent.generated.h"

class AACFCharacter;
class UACFAIRoutineComponent;
class UACFAIRoutineDataAsset;
class UACFTask;
class UAITask_MoveTo;
class UPangeaMiningConvoyComponent;

/** Executes only its own ACF-requested convoy duty. Never chooses combat state or grants items. */
UCLASS(ClassGroup = (Pangea), BlueprintType)
class GF_PANGEAMININGSYSTEMRUNTIME_API UPangeaMiningTransportJobComponent final : public UActorComponent
{
	GENERATED_BODY()
public:
	UPangeaMiningTransportJobComponent();

	/** Binds this job to one convoy member (courier: INDEX_NONE, escort: 0 and up, draft animal: DraftAnimalSlot) and installs its runtime ACF routine. Returns false when the setup is not valid. */
	bool Configure(UPangeaMiningConvoyComponent* Convoy, AACFCharacter* Character, int32 EscortSlot);

	/** Called by the ACF routine task when the schedule requests this duty. */
	void BeginRoutineJob(UACFTask* Task);

	/** Called by the ACF routine task when the schedule ends this duty; cancels the running operation. */
	void EndRoutineJob(UACFTask* Task);

	/** Cancels the running operation, gives the routine component back its original routine (unless it was replaced externally) and clears the assignment. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Assignment")
	void ReleaseAssignment();

	/** Why the convoy duty was last blocked or is waiting; empty once a new move has started. */
	UFUNCTION(BlueprintPure, Category = "Diagnostics")
	FString GetBlockedReason() const
	{
		return BlockedReason;
	}

	/** Advances the convoy duty state machine (loading, travel, guarding, unloading) while the ACF routine requests it. */
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	UFUNCTION()
	void HandleAIStateChanged(FGameplayTag State);
	bool CanExecute() const;
	void CancelOperation();
	void Present(EMiningPresentationState State, const FVector& Focus);
	void Block(const TCHAR* Reason);
	void BeginHold(EMiningPresentationState State, const FVector& Focus);
	UPROPERTY(Transient)
	TObjectPtr<UACFAIRoutineDataAsset> RuntimeRoutine;
	UPROPERTY(Transient)
	TObjectPtr<UACFAIRoutineDataAsset> OriginalRoutine;
	UPROPERTY(Transient)
	TObjectPtr<UACFTask> RequestedTask;
	UPROPERTY(Transient)
	TObjectPtr<UAITask_MoveTo> MoveTask;
	TWeakObjectPtr<UACFAIRoutineComponent> Routine;
	TWeakObjectPtr<UPangeaMiningConvoyComponent> Convoy;
	TWeakObjectPtr<AACFCharacter> Character;
	FGuid ManifestId;
	int32 EscortSlot = INDEX_NONE;
	FString BlockedReason;
	float NavigationTimeout = 120.f;
	float RetryDelay = 3.f;
	double Deadline = 0.;
	double RetryAt = 0.;
	bool bHolding = false;
	bool bLoaded = false;
	bool bMoveFinished = false;
	bool bMoveSucceeded = false;

	// Draft animals keep their native AnimBP. Only this job's MaxWalkSpeed
	// override is restored when ACF combat/commands take ownership.
	float DraftOriginalSpeed = 0.f;
	float DraftAppliedSpeed = 0.f;
	bool bOwnsDraftSpeed = false;
	EMiningPresentationState HoldState = EMiningPresentationState::Idle;
};
