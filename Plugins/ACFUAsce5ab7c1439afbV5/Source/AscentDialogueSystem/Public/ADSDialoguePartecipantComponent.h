// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "ADSBaseDialoguePartecipantComponent.h"
#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"
#include "CoreMinimal.h"
#include "Engine/TargetPoint.h"
#include "GameplayTagContainer.h"
#include "Graph/ADSDialogue.h"
#include "Net/UnrealNetwork.h"
#include "Net/Serialization/FastArraySerializer.h"

#include "ADSDialoguePartecipantComponent.generated.h"

class USkeletalMeshComponent;
class UADSDialogue;
class UADSDialoguePartecipantComponent;

// ============================================================================
// Fast Array Serializer for efficient dialogue replication
// ============================================================================

USTRUCT(BlueprintType)
struct ASCENTDIALOGUESYSTEM_API FADSDialogueEntry : public FFastArraySerializerItem
{
	GENERATED_BODY()

	FADSDialogueEntry()
		: DialogueTag(FGameplayTag::EmptyTag)
		, LocalInstance(nullptr)
	{
	}

	FADSDialogueEntry(UADSDialogue* InSourceDialogue, FGameplayTag InTag)
		: SourceDialogue(InSourceDialogue)
		, DialogueTag(InTag)
		, LocalInstance(nullptr)
	{
	}

	// Soft reference to the source dialogue asset - this replicates
	UPROPERTY(BlueprintReadOnly, Category = ADS)
	TSoftObjectPtr<UADSDialogue> SourceDialogue;

	// Cache the tag for faster lookups and replication
	UPROPERTY(BlueprintReadOnly, Category = ADS)
	FGameplayTag DialogueTag;

	// Local instanced copy - NOT replicated, created locally on each machine
	UPROPERTY(NotReplicated, Transient, BlueprintReadOnly, Category = ADS)
	TObjectPtr<UADSDialogue> LocalInstance;

	void PreReplicatedRemove(const struct FADSDialogueArray& InArraySerializer);
	void PostReplicatedAdd(const struct FADSDialogueArray& InArraySerializer);
	void PostReplicatedChange(const struct FADSDialogueArray& InArraySerializer);

	// Get the usable dialogue instance (creates if needed)
	UADSDialogue* GetDialogueInstance(UObject* Outer);
};

USTRUCT(BlueprintType)
struct ASCENTDIALOGUESYSTEM_API FADSDialogueArray : public FFastArraySerializer
{
	GENERATED_BODY()

	FADSDialogueArray()
		: OwnerComponent(nullptr)
	{
	}

	UPROPERTY()
	TArray<FADSDialogueEntry> Items;

	UPROPERTY(NotReplicated)
	TObjectPtr<UADSDialoguePartecipantComponent> OwnerComponent;

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FADSDialogueEntry, FADSDialogueArray>(Items, DeltaParms, *this);
	}

	void AddDialogue(UADSDialogue* SourceDialogue);
	void RemoveDialogue(UADSDialogue* Dialogue);
	void RemoveDialogueByTag(FGameplayTag DialogueTag);
	UADSDialogue* FindDialogueByTag(FGameplayTag DialogueTag) const;
	UADSDialogue* FindOrCreateDialogueByTag(FGameplayTag DialogueTag);
	void ClearAll();
};

template<>
struct TStructOpsTypeTraits<FADSDialogueArray> : public TStructOpsTypeTraitsBase2<FADSDialogueArray>
{
	enum
	{
		WithNetDeltaSerializer = true,
	};
};

// ============================================================================
// Delegate declarations
// ============================================================================

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDialogueListChanged, UADSDialogue*, Dialogue);

// ============================================================================
// Main Component
// ============================================================================

UCLASS(Blueprintable, ClassGroup = (ACF), meta = (BlueprintSpawnableComponent))
class ASCENTDIALOGUESYSTEM_API UADSDialoguePartecipantComponent : public UADSBaseDialoguePartecipantComponent
{
	GENERATED_BODY()

public:
	UADSDialoguePartecipantComponent();

	// ========================================================================
	// UActorComponent Interface
	// ========================================================================
	
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ========================================================================
	// Dialogue Starting
	// ========================================================================

	// Attempts to start a dialogue given an array of dialogue participant components and a dialogue object
	virtual bool TryStartDialogue(const TArray<UADSBaseDialoguePartecipantComponent*>& participants, UADSDialogue* dialogueToStart) override;

	void RegisterDialogueEvents(UADSDialogue* dialogueToStart);

	// Attempts to start a dialogue using actors instead of participant components
	virtual bool TryStartDialogueFromActors(const TArray<AActor*>& participants, UADSDialogue* dialogueToStart) override;

	/**
	 * Starts a world dialogue on every player within World Dialogue Hearing Radius of this participant.
	 * Server only: each nearby player runs the dialogue locally through its own Dialogue Master.
	 * @return True if at least one player was in range and received the dialogue.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = ADS, meta = (DisplayName = "Play Replicated World Dialogue", Keywords = "world dialogue replicated nearby players"))
	bool PlayReplicatedWorldDialogue(class UADSWorldDialogue* dialogue, const TArray<UADSBaseDialoguePartecipantComponent*>& participants);

	/** Same as Play Replicated World Dialogue, taking the participant actors instead of their components. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = ADS, meta = (DisplayName = "Play Replicated World Dialogue From Actors"))
	bool PlayReplicatedWorldDialogueFromActors(class UADSWorldDialogue* dialogue, const TArray<AActor*>& participants);

	/** Players farther than this from the participant do not hear world dialogues started by it. 0 means every player. */
	UFUNCTION(BlueprintPure, Category = ADS)
	float GetWorldDialogueHearingRadius() const { return WorldDialogueHearingRadius; }

	UFUNCTION(BlueprintCallable, Category = ADS)
	void SetWorldDialogueHearingRadius(float NewRadius) { WorldDialogueHearingRadius = FMath::Max(NewRadius, 0.f); }

	/** Pauses every started dialogue on this participant after the current node finishes. Authority replicates to clients. */
	UFUNCTION(BlueprintCallable, Category = ADS, meta = (DisplayName = "Pause Active Dialogues", Keywords = "pause hold wait"))
	void PauseActiveDialogues();

	/** Resumes every paused dialogue on this participant. Authority replicates to clients. */
	UFUNCTION(BlueprintCallable, Category = ADS, meta = (DisplayName = "Resume Active Dialogues", Keywords = "resume continue unpause"))
	void ResumeActiveDialogues();

	// ========================================================================
	// Participant Info
	// ========================================================================

	// Returns the tag associated with the participant (override to use base properties)
	FGameplayTag GetParticipantTag() const override { return PartecipantTag; }

	// ========================================================================
	// Dialogue Management - Runtime Modifiable & Replicated
	// ========================================================================

	// Retrieves a dialogue by tag, setting bFound to true if found
	virtual UADSDialogue* GetDialogue(FGameplayTag dialogueTag, bool& bFound) const override;

	// Adds a dialogue at runtime (replicated)
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "ADS|Runtime")
	void AddDialogue(UADSDialogue* Dialogue);

	// Removes a dialogue at runtime (replicated)
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "ADS|Runtime")
	void RemoveDialogue(UADSDialogue* Dialogue);

	// Removes a dialogue by tag at runtime (replicated)
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "ADS|Runtime")
	void RemoveDialogueByTag(FGameplayTag DialogueTag);

	// Clears all runtime dialogues (replicated)
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "ADS|Runtime")
	void ClearAllDialogues();

	// Returns all currently available dialogues (includes both initial and runtime-added)
	UFUNCTION(BlueprintPure, Category = ADS)
	TArray<UADSDialogue*> GetAllDialogues() const;

	// Returns the number of available dialogues
	UFUNCTION(BlueprintPure, Category = ADS)
	int32 GetDialogueCount() const;

	// Checks if a dialogue with the given tag exists
	UFUNCTION(BlueprintPure, Category = ADS)
	bool HasDialogueWithTag(FGameplayTag DialogueTag) const;

	// ========================================================================
	// Skeletal Mesh Management
	// ========================================================================

	// Retrieves the skeletal mesh component of the owner
	UFUNCTION(BlueprintPure, Category = ADS)
	USkeletalMeshComponent* GetOwnerMesh();

	// Retrieves the skeletal mesh component used for facial animations
	UFUNCTION(BlueprintPure, Category = ADS)
	USkeletalMeshComponent* GetFacialAnimationMesh();

	// Sets the skeletal mesh for the participant
	UFUNCTION(BlueprintCallable, Category = ADS)
	void SetParticipantSkeletalMesh(class USkeletalMeshComponent* mesh)
	{
		skeletalMesh = mesh;
	}

	// ========================================================================
	// Animation
	// ========================================================================

	// Plays an animation montage on the character that owns this participant
	UFUNCTION(BlueprintCallable, Category = ADS)
	virtual void PlayAnimationOnCharacterOwner(UAnimMontage* animation);

	// Plays a facial animation montage on the character that owns this participant
	UFUNCTION(BlueprintCallable, Category = ADS)
	virtual void PlayFacialAnimationOnCharacterOwner(UAnimMontage* animation);

	// Stops a facial animation montage on the character that owns this participant
	UFUNCTION(BlueprintCallable, Category = ADS)
	virtual void StopFacialAnimationOnCharacterOwner(UAnimMontage* animation);

	// Stops an animation montage on the character that owns this participant
	UFUNCTION(BlueprintCallable, Category = ADS)
	virtual void StopAnimationOnCharacterOwner(UAnimMontage* animation);

	// ========================================================================
	// Objective Dialogues
	// ========================================================================

	/**
	 * Returns true if the map contains at least one valid dialogue objective.
	 * @return true if the first element in ObjectiveDialogues is valid, false otherwise
	 */
	UFUNCTION(BlueprintPure, Category = ADS)
	bool HasAnyDialogueObjective() const
	{
		if (ObjectiveDialogues.IsEmpty())
		{
			return false;
		}

		auto It = ObjectiveDialogues.CreateConstIterator();
		return IsValid(It->Value);
	}

	/**
	 * Returns the first valid dialogue objective in the map.
	 * @return pointer to the first valid UADSDialogue in ObjectiveDialogues, or nullptr if none exists
	 */
	UFUNCTION(BlueprintPure, Category = ADS)
	UADSDialogue* GetFirstDialogueObjective() const
	{
		if (!HasAnyDialogueObjective())
		{
			return nullptr;
		}

		auto It = ObjectiveDialogues.CreateConstIterator();
		return It->Value;
	}

	// Adds an objective-specific dialogue
	UFUNCTION(BlueprintCallable, Category = ADS)
	void AddObjectiveDialogue(FGuid objectiveID, UADSDialogue* dialogue);

	// Removes an objective-specific dialogue
	UFUNCTION(BlueprintCallable, Category = ADS)
	void RemoveObjectiveDialogue(FGuid objectiveID);

	// ========================================================================
	// Delegates
	// ========================================================================

	UPROPERTY(BlueprintAssignable, Category = ADS)
	FOnDialogueNodeActivated OnNodeActivated;

	/** Fired on every machine when a dialogue of this participant holds at the end of its current node. */
	UPROPERTY(BlueprintAssignable, Category = ADS)
	FOnDialoguePaused OnDialoguePaused;

	/** Fired on every machine when a dialogue of this participant resumes. */
	UPROPERTY(BlueprintAssignable, Category = ADS)
	FOnDialogueResumed OnDialogueResumed;

	// Called when a dialogue is added at runtime
	UPROPERTY(BlueprintAssignable, Category = "ADS|Runtime")
	FOnDialogueListChanged OnDialogueAdded;

	// Called when a dialogue is removed at runtime
	UPROPERTY(BlueprintAssignable, Category = "ADS|Runtime")
	FOnDialogueListChanged OnDialogueRemoved;

	// ========================================================================
	// Interactable Name
	// ========================================================================

	// Returns the name of the participant (interactable name)
	FText GetInteractableName() const override
	{
		FText name = GetParticipantName();
		UE_LOG(LogTemp, Log, TEXT("GetInteractableName called for %s, returning: %s"),
			GetOwner() ? *GetOwner()->GetName() : TEXT("NULL"),
			*name.ToString());
		if (name.IsEmpty() || name.ToString().Equals(TEXT("Unknown")))
		{
			UE_LOG(LogTemp, Error, TEXT("Interactable name is unknown for %s!"), *GetOwner()->GetName());
		}
		return name;
	}

protected:
	// ========================================================================
	// Properties
	// ========================================================================

	// List of dialogues that can be started for this participant (set in editor, instances created at BeginPlay)
	// DEPRECATED: Use GetAllDialogues() to access dialogues. This array is kept for backward compatibility.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = ADS, meta = (DisplayName = "Initial Dialogues"))
	TArray<class UADSDialogue*> Dialogues;

	// Replicated runtime dialogue array
	UPROPERTY(Replicated, Transient, BlueprintReadOnly, Category = "ADS|Runtime")
	FADSDialogueArray ReplicatedDialogues;

	// List of dialogues that are started during specific objectives
	UPROPERTY(Transient, BlueprintReadOnly, Category = ADS)
	TMap<FGuid, UADSDialogue*> ObjectiveDialogues;

	// BlendoutTime for stopped animations
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = ADS)
	float AnimationBlendoutTime = .2f;

	/* Maximum distance from this participant at which a player hears a replicated world dialogue.
	Set to 0 to reach every player in the level. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ADS, meta = (ClampMin = "0.0", Units = "cm"))
	float WorldDialogueHearingRadius = 3000.f;

	// ========================================================================
	// Event Implementations
	// ========================================================================

	void OnDialogueStartedEvent_Implementation() override;
	void OnDialogueEndedEvent_Implementation() override;

	// ========================================================================
	// Cached References
	// ========================================================================

	// Skeletal mesh for the participant
	TObjectPtr<USkeletalMeshComponent> skeletalMesh;

	// Facial skeletal mesh for the participant
	TObjectPtr<USkeletalMeshComponent> facialMesh;

	// ========================================================================
	// Lifecycle
	// ========================================================================

	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type reason) override;

private:
	// ========================================================================
	// Internal Event Handlers
	// ========================================================================

	UFUNCTION()
	void HandleDialogueStarted(UADSDialogue* dialogue);

	UFUNCTION()
	void HandleDialogueEnded(UADSDialogue* dialogue);

	UFUNCTION()
	void HandleNodeActivated(const FGuid& nodeId);

	UFUNCTION()
	void HandleDialoguePaused(UADSDialogue* dialogue);

	UFUNCTION()
	void HandleDialogueResumed(UADSDialogue* dialogue);

	void UnregisterDialogueEvents(UADSDialogue* dialogue);

	void ApplyPauseActiveDialogues();
	void ApplyResumeActiveDialogues();

	UFUNCTION(Server, Reliable)
	void ServerPauseActiveDialogues();

	UFUNCTION(Server, Reliable)
	void ServerResumeActiveDialogues();

	UFUNCTION(NetMulticast, Reliable)
	void MulticastPauseActiveDialogues();

	UFUNCTION(NetMulticast, Reliable)
	void MulticastResumeActiveDialogues();

	// ========================================================================
	// Internal Helpers
	// ========================================================================

	// Creates an instanced copy of a dialogue for runtime use
	UADSDialogue* CreateDialogueInstance(UADSDialogue* SourceDialogue);

	// ========================================================================
	// Legacy Support
	// ========================================================================

	// Kept for backward compatibility - now managed through ReplicatedDialogues
	UPROPERTY(Transient)
	TArray<TObjectPtr<class UADSDialogue>> instancedDialogues;

	// ========================================================================
	// Friend Declarations
	// ========================================================================

	friend struct FADSDialogueEntry;
	friend struct FADSDialogueArray;

public:
	// Called by FADSDialogueEntry when replication events occur
	void OnDialogueEntryAdded(UADSDialogue* Dialogue);
	void OnDialogueEntryRemoved(UADSDialogue* Dialogue);
};