// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Graph/AQSObjectiveNode.h"
#include "WorldPartition/WorldPartitionStreamingSource.h"
// GetObjectiveTargets() calls IsValid() on UAQSQuestTargetComponent inline, which needs
// the complete type, not just the forward declaration below.
#include "AQSQuestTargetComponent.h"

#include "AQSQuestObjective.generated.h"

class UAQSQuestManagerComponent;
class APlayerController;

UENUM(BlueprintType)
enum class ETargetReferenceType : uint8 {
	ESoftRef UMETA(DisplayName = "Soft Reference"),
	ETag UMETA(DisplayName = "Gameplay Tag"),
};

UENUM()
enum class EObjectiveActivationState : uint8 {
	EInactive,
	/*Only while a World Partition load for unresolved soft refs is actually in progress.*/
	ELoadingTargets,
	EActive,
};

/**
 * Base class representing a quest objective in the Ascent Quest System.
 * Provides accessors for objective data, including targets, tags, descriptions,
 * completion state and repetitions. Acts as a bridge between Blueprint logic and
 * the underlying node owner.
 */
UCLASS(Blueprintable, BlueprintType, EditInlineNew)
class ASCENTQUESTSYSTEM_API UAQSQuestObjective : public UObject, public IWorldPartitionStreamingSourceProvider {
	GENERATED_BODY()

public:


	UAQSQuestObjective();

	/**
	 * Returns the list of the actors referenced by this objective as set in the graph node.
	 *
	 * @return Array of AActor pointers representing the objective targets.
	 */
	UFUNCTION(BlueprintPure, Category = AQS)
	TArray<class AActor*> GetObjectiveTargetsActors() const;

	/**
	 * Best-known world locations of this objective's targets.
	 * Prefers live streamed actors; falls back to cached World Partition locations
	 * when the referenced actors have not been loaded yet.
	 */
	UFUNCTION(BlueprintPure, Category = AQS)
	TArray<FVector> GetKnownTargetLocations() const;

	/**
	 * Returns the list of target components referenced by this objective as set in the graph node.
	 *
	 * @return Array of UAQSQuestTargetComponent pointers representing the quest targets.
	 */
	UFUNCTION(BlueprintPure, Category = AQS)
	TArray<class UAQSQuestTargetComponent*> GetObjectiveTargets() const
	{
		TArray<UAQSQuestTargetComponent*> ValidTargets;
		ValidTargets.Reserve(questTargets.Num());
		for (UAQSQuestTargetComponent* Target : questTargets) {
			if (IsValid(Target)) {
				ValidTargets.Add(Target);
			}
		}
		return ValidTargets;
	}

	/**
	 * Returns the type of reference used by this objective to identify its targets.
	 *
	 * @return The reference type of the objective targets.
	 */
	UFUNCTION(BlueprintPure, Category = AQS)
	FORCEINLINE ETargetReferenceType GetTargetRefType() const
	{
		return TargetRefType;
	}

	/**
	 * Returns the gameplay tag associated with this objective as set in the graph node.
	 *
	 * @return GameplayTag identifying the objective.
	 */
	UFUNCTION(BlueprintPure, Category = AQS)
	FORCEINLINE FGameplayTag GetObjectiveTag() const
	{
		return ObjectiveTag;
	}

	/**
	 * Returns the unique identifier of this objective from its owning node.
	 *
	 * @return FGuid representing the node ID of the objective owner.
	 */
	UFUNCTION(BlueprintPure, Category = AQS)
	FORCEINLINE FGuid GetObjectiveId() const
	{
		return nodeOwner ? nodeOwner->GetNodeId() : FGuid();
	}

	/**
	 * Returns the display name of this objective for UI (quest tracker rows etc.).
	 * By default returns the ObjectiveName set in the graph node, appending the
	 * current progress (e.g. "Kill the wolves 1/5") when the objective requires
	 * multiple repetitions. Can be overridden in child classes to provide fully
	 * custom dynamic names (e.g. live inventory counters). UI widgets should
	 * always re-query this function on every objective update instead of
	 * caching the static name.
	 *
	 * @return Localized text representing the objective name for UI display.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = AQS)
	FText GetObjectiveName() const;
	virtual FText GetObjectiveName_Implementation() const;

	/**
	 * Returns the raw ObjectiveName property as set in the graph node, without
	 * any progress decoration. Use this as the base text when overriding
	 * GetObjectiveName.
	 *
	 * @return Localized text representing the plain objective name.
	 */
	UFUNCTION(BlueprintPure, Category = AQS)
	FORCEINLINE FText GetRawName() const
	{
		return ObjectiveName;
	}

	/**
	 * Returns the description of this objective as set in the graph node.
	 *
	 * @return Localized text describing the objective.
	 */
	UFUNCTION(BlueprintPure, Category = AQS)
	FORCEINLINE FText GetDescription() const
	{
		return ObjectiveDescription;
	}

	/**
	 * Returns the description to be displayed in UI for this objective.
	 * By default returns the plain ObjectiveDescription set in the graph node
	 * (progress counters are appended to the name by GetObjectiveName instead).
	 * Can be overridden in child classes to provide dynamic descriptions.
	 *
	 * @return Localized text describing the objective for UI display.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = AQS)
	FText GetObjectiveDescription() const;
	virtual FText GetObjectiveDescription_Implementation() const;

	/**
	 * Retrieves the local player controller associated with this objective.
	 *
	 * @return APlayerController pointer if valid, nullptr otherwise.
	 */
	UFUNCTION(BlueprintPure, Category = AQS)
	APlayerController* GetLocalPlayerController() const;


	/**
	 * Returns the world context object associated with this objective.
	 *
	 * @return UWorld pointer of the current world context.
	 */
	UFUNCTION(BlueprintPure, Category = AQS)
	FORCEINLINE class UWorld* GetWorldContextObject() const
	{
		return GetWorld();
	}

	/**
	 * Returns the current number of repetitions completed for this objective.
	 *
	 * @return Integer count of completed repetitions.
	 */
	UFUNCTION(BlueprintPure, Category = AQS)
	FORCEINLINE int32 GetCurrentRepetitions() const
	{
		return nodeOwner->GetCurrentRepetitions();
	}

	/**
	 * Returns the total number of repetitions required to complete this objective.
	 *
	 * @return Integer count of required repetitions.
	 */
	UFUNCTION(BlueprintPure, Category = AQS)
	FORCEINLINE int32 GetRepetitions() const
	{
		return nodeOwner->GetQuestRepetitions();
	}

	/**
	 * Checks whether this objective has been completed.
	 *
	 * @return True if completed, false otherwise.
	 */
	UFUNCTION(BlueprintPure, Category = AQS)
	FORCEINLINE bool IsObjectiveCompleted() const
	{
		return nodeOwner->IsObjectiveCompleted();
	}

	/**
	 * Checks whether this objective is currently tracked by the quest system.
	 *
	 * @return True if tracked, false otherwise.
	 */
	UFUNCTION(BlueprintPure, Category = AQS)
	FORCEINLINE bool IsTracked() const
	{
		return nodeOwner->IsTracked();
	}

	/**
	 * Marks this objective as completed and notifies the quest system.
	 */
	UFUNCTION(BlueprintCallable, Category = AQS)
	void CompleteObjective();

	/**
	 * Returns the owning node of this objective.
	 *
	 * @return UAQSObjectiveNode pointer to the owner node of this objective.
	 */
	UFUNCTION(BlueprintPure, Category = AQS)
	UAQSObjectiveNode* GetNodeOwner() const { return nodeOwner; }

	friend class UAQSObjectiveNode;
	friend class UAQSQuestManagerComponent;

	TArray<TSoftObjectPtr<AActor>> GetObjectiveTargetsSoftRef() const;

	/**
	 * Offers a target component that just came online to this objective. Objectives snapshot their
	 * targets when they start, so any actor streamed in afterwards would never be picked up.
	 *
	 * @return True when the target is referenced by this objective and was added to its target list.
	 */
	bool TryAddLateLoadedTarget(class UAQSQuestTargetComponent* targetComp);

private:
	void Internal_OnObjectiveStarted(class UAQSObjectiveNode* inNodeOwner);

	void Internal_OnObjectiveEnded(bool bInterrupted);

	void FillTargetsRef();
	void RefreshForcedTargets();
	void ReleaseForcedTargets();
	bool IsStreamingLocationResolved(int32 Index) const;
	bool HasUsableStreamingCache() const;
	bool IsTargetLoadable(int32 Index) const;
	bool IsForcedStreamingCompleted() const;
	void DispatchObjectiveStarted();
	bool TryDispatchObjectiveStarted();
	bool IsWaitingForTargets() const { return ActivationState == EObjectiveActivationState::ELoadingTargets; }
	bool HasDispatchedObjectiveStarted() const { return ActivationState == EObjectiveActivationState::EActive; }

	virtual void BeginDestroy() override;

	// IWorldPartitionStreamingSourceProvider interface
	virtual bool GetStreamingSources(TArray<FWorldPartitionStreamingSource>& OutStreamingSources) const override;
	virtual const UObject* GetStreamingSourceOwner() const override { return this; }
	// End of IWorldPartitionStreamingSourceProvider interface

#if WITH_EDITOR
	virtual void PreSave(FObjectPreSaveContext SaveContext) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	void RefreshReferencedActorLocations();
#endif

	UPROPERTY()
	TObjectPtr<UAQSObjectiveNode> nodeOwner;

	UPROPERTY()
	TArray<TObjectPtr<class UAQSQuestTargetComponent>> questTargets;

	UPROPERTY(Transient)
	EObjectiveActivationState ActivationState = EObjectiveActivationState::EInactive;

	/*True while this objective is registered as a World Partition streaming source.*/
	UPROPERTY(Transient)
	bool bIsStreamingSourceRegistered = false;

	/*World this objective registered its streaming source into. GetWorld() resolves through
	nodeOwner and its player controller, which can already be gone by the time BeginDestroy runs,
	and the subsystem keeps raw provider pointers: this guarantees we can always unregister.*/
	UPROPERTY(Transient)
	TWeakObjectPtr<UWorld> StreamingSourceWorld;

	FTimerHandle ForcedTargetsRefreshTimer;

	/*Poll ticks spent waiting for forced targets, used only to throttle diagnostic logging.*/
	int32 ForcedTargetsWaitTicks = 0;

	/*Indices into ReferencedActors that could not be resolved and are being streamed in
	through World Partition. Empty in the common case where the soft refs are already valid.*/
	UPROPERTY(Transient)
	TArray<int32> ForcedStreamingTargetIndices;

protected:


	/*A description for this objective, can be used for UI*/
	UPROPERTY(EditDefaultsOnly, Category = AQS)
	FText ObjectiveName;

	/*A description for this objective, can be used for UI*/
	UPROPERTY(EditDefaultsOnly, Category = AQS)
	FText ObjectiveDescription;

	/*Unique Tag to identify this objective*/
	UPROPERTY(EditDefaultsOnly, Category = AQS)
	FGameplayTag ObjectiveTag;

	/*Define how you want to identify the targets of this objectives
	if you want tor efer directly hem thorught soft references or thorught
	their GameplayTags*/
	UPROPERTY(EditDefaultsOnly, Category = AQS)
	ETargetReferenceType TargetRefType;

	/*A list of targets that, if they have a QuestTargetComponent, will be
	updated with this objective progresses. All targets with that tag in the component
	will be alerted*/
	UPROPERTY(EditDefaultsOnly, meta = (EditCondition = "TargetRefType == ETargetReferenceType::ETag"), Category = AQS)
	TArray<FGameplayTag> ReferencedTargets;

	/*A list of targets that, if they have a QuestTargetComponent, will be
	updated with this objective progresses. The provided actors need to have
	a QuestTargetComponent to work*/
	UPROPERTY(EditDefaultsOnly, meta = (EditCondition = "TargetRefType == ETargetReferenceType::ESoftRef"), Category = AQS)
	TArray<TSoftObjectPtr<AActor>> ReferencedActors;

	/*Last-resort World Partition streaming when a soft-referenced target does not resolve.
	Ignored if the actor is already loaded, if the map has no World Partition, or if the
	editor cache has no location for that ref. Leave on; the wait state is still rare.*/
	UPROPERTY(EditDefaultsOnly, meta = (EditCondition = "TargetRefType == ETargetReferenceType::ESoftRef"), Category = AQS)
	bool bForceLoadReferencedActors = true;

	/*Radius in centimeters streamed around each referenced actor. Keep this small: it should only
	cover the target and its immediate surroundings (e.g. the ladder the target stands on), never
	whole regions of the map.*/
	UPROPERTY(EditDefaultsOnly, meta = (EditCondition = "TargetRefType == ETargetReferenceType::ESoftRef && bForceLoadReferencedActors",
		EditConditionHides, ClampMin = "1.0", ClampMax = "5000.0", Units = "cm"), Category = AQS)
	float ForcedTargetStreamingRadius = 80.0f;

	/*Cached streaming locations for referenced World Partition actors.
	Automatically refreshed in editor and used by runtime streaming sources.*/
	UPROPERTY(VisibleDefaultsOnly, AdvancedDisplay, meta = (EditCondition = "TargetRefType == ETargetReferenceType::ESoftRef && bForceLoadReferencedActors",
		EditConditionHides), Category = AQS)
	TArray<FVector> ReferencedActorStreamingLocations;

	/*Per-entry flag telling whether the matching cached location was actually resolved from its actor.
	Unresolved entries would otherwise stream the cell at the world origin. Left empty by quest assets
	saved before this flag existed, which are treated as fully resolved until they are saved again.*/
	UPROPERTY(VisibleDefaultsOnly, AdvancedDisplay, meta = (EditCondition = "TargetRefType == ETargetReferenceType::ESoftRef && bForceLoadReferencedActors",
		EditConditionHides), Category = AQS)
	TArray<bool> ReferencedActorStreamingLocationsResolved;

	/*Called when this objective is started*/
	UFUNCTION(BlueprintNativeEvent, Category = AQS)
	void OnObjectiveStarted();
	virtual void OnObjectiveStarted_Implementation();

	UFUNCTION(BlueprintNativeEvent, Category = AQS)
	void ClientOnObjectiveStarted();
	virtual void ClientOnObjectiveStarted_Implementation();

	/*Called when this objective is completed*/
	UFUNCTION(BlueprintNativeEvent, Category = AQS)
	void OnObjectiveCompleted();
	virtual void OnObjectiveCompleted_Implementation();

	/*Called when this objective is completed*/
	UFUNCTION(BlueprintNativeEvent, Category = AQS)
	void ClientOnObjectiveCompleted();
	virtual void ClientOnObjectiveCompleted_Implementation();

	UFUNCTION(BlueprintNativeEvent, Category = AQS)
	void OnObjectiveUpdated();
	virtual void OnObjectiveUpdated_Implementation();

	UFUNCTION(BlueprintNativeEvent, Category = AQS)
	void OnObjectiveInterrupted();
	virtual void OnObjectiveInterrupted_Implementation();

	UFUNCTION(BlueprintNativeEvent, Category = AQS)
	void ClientOnObjectiveInterrupted();
	virtual void ClientOnObjectiveInterrupted_Implementation();

	UFUNCTION(BlueprintCallable, Category = AQS)
	void SetCurrentRepetitions(int32 currentRep);

	UFUNCTION(BlueprintPure, Category = AQS)
	UAQSQuestManagerComponent* GetLocalQuestManager() const;


	// UObject interface
	UWorld* GetWorld() const override { return GetLocalPlayerController() ? GetLocalPlayerController()->GetWorld() : nullptr; }
	// End of UObject interface
};
