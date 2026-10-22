#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Interfaces/MiningPresentationAgentInterface.h"
#include "PangeaMiningTransportJobComponent.generated.h"

class AACFCharacter;
class UACFTask;
class UACFAIRoutineComponent;
class UACFAIRoutineDataAsset;
class UAITask_MoveTo;
class UPangeaMiningConvoyComponent;

/** Executes only its own ACF-requested convoy duty. Never chooses combat state or grants items. */
UCLASS(ClassGroup=(Pangea), BlueprintType)
class GF_PANGEAMININGSYSTEMRUNTIME_API UPangeaMiningTransportJobComponent final : public UActorComponent
{
	GENERATED_BODY()
public:
	UPangeaMiningTransportJobComponent();
	bool Configure(UPangeaMiningConvoyComponent* Convoy, AACFCharacter* Character, int32 EscortSlot);
	void BeginRoutineJob(UACFTask* Task);
	void EndRoutineJob(UACFTask* Task);
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Assignment")
	void ReleaseAssignment();
	UFUNCTION(BlueprintPure, Category="Diagnostics")
	FString GetBlockedReason() const { return BlockedReason; }
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
	double Deadline = 0., RetryAt = 0.;
	bool bHolding = false, bLoaded = false, bMoveFinished = false, bMoveSucceeded = false;
	// Draft animals keep their native AnimBP. Only this job's MaxWalkSpeed
	// override is restored when ACF combat/commands take ownership.
	float DraftOriginalSpeed = 0.f, DraftAppliedSpeed = 0.f;
	bool bOwnsDraftSpeed = false;
	EMiningPresentationState HoldState = EMiningPresentationState::Idle;
};
