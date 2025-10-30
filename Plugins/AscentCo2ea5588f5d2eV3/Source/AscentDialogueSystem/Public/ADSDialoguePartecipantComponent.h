// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.

#pragma once

#include "ADSBaseDialoguePartecipantComponent.h"
#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"
#include "CoreMinimal.h"
#include "Engine/TargetPoint.h"
#include "GameplayTagContainer.h"
#include "Graph/ADSDialogue.h"

#include "ADSDialoguePartecipantComponent.generated.h"

class USkeletalMeshComponent;
class UADSDialogue;

UCLASS(Blueprintable, ClassGroup = (ACF), meta = (BlueprintSpawnableComponent))
class ASCENTDIALOGUESYSTEM_API UADSDialoguePartecipantComponent : public UADSBaseDialoguePartecipantComponent {
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UADSDialoguePartecipantComponent();

public:
	// Attempts to start a dialogue given an array of dialogue participant components and a dialogue object*/
	virtual bool TryStartDialogue(const TArray<UADSBaseDialoguePartecipantComponent*>& participants, UADSDialogue* dialogueToStart) override;

	void RegisterDialogueEvents(UADSDialogue* dialogueToStart);

	/** Attempts to start a dialogue using actors instead of participant components*/
	virtual bool TryStartDialogueFromActors(const TArray<AActor*>& participants, UADSDialogue* dialogueToStart) override;


	// Returns the tag associated with the participant (override to use base properties)
	FGameplayTag GetParticipantTag() const override { return PartecipantTag; }

	// Retrieves a dialogue by tag, setting bFound to true if found
	virtual UADSDialogue* GetDialogue(FGameplayTag dialogueTag, bool& bFound) const override;

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


	//Adds an an objective-specific dialogue
	UFUNCTION(BlueprintCallable, Category = ADS)
	void AddObjectiveDialogue(FGuid objectiveID, UADSDialogue* dialogue);

	
    UPROPERTY(BlueprintAssignable, Category = ADS)
    FOnDialogueNodeActivated OnNodeActivated;

	//Adds an an objective-specific dialogue
	UFUNCTION(BlueprintCallable, Category = ADS)
	void RemoveObjectiveDialogue(FGuid objectiveID);

	// Returns the name of the participant (interactable name)
	FText GetInteractableName() const override
	{
		FText name = GetParticipantName();
		UE_LOG(LogTemp, Log, TEXT("GetInteractableName called for %s, returning: %s"),
			GetOwner() ? *GetOwner()->GetName() : TEXT("NULL"),
			*name.ToString());
		if (name.IsEmpty() || name.ToString().Equals(TEXT("Unknown"))) {
			UE_LOG(LogTemp, Error, TEXT("Interactable name is unknown for %s!"), *GetOwner()->GetName());
		}
		return name;
	}

protected:
	// List of dialogues that can be started for this participant
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = ADS)
	TArray<class UADSDialogue*> Dialogues;

	// List of dialogues that are started during specific objectives
	UPROPERTY(BlueprintReadOnly, Category = ADS)
	TMap<FGuid, UADSDialogue*> ObjectiveDialogues;

	// BlendoutTime for stopped animations
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = ADS)
	float AnimationBlendoutTime = .2;

	void OnDialogueStartedEvent_Implementation() override;

	// Event called when a dialogue ends
	void OnDialogueEndedEvent_Implementation() override;

	// Skeletal mesh for the participant
	TObjectPtr<USkeletalMeshComponent> skeletalMesh;

	// Facial skeletal mesh for the participant
	TObjectPtr<USkeletalMeshComponent> facialMesh;

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

	virtual void EndPlay(EEndPlayReason::Type reason) override;

private:
	UFUNCTION()
	void HandleDialogueStarted(UADSDialogue* dialogue);

	UFUNCTION()
	void HandleDialogueEnded(UADSDialogue* dialogue);

	void UnregisterDialogueEvents(UADSDialogue* dialogue);

	UFUNCTION()
	void HandleNodeActivated(const FGuid& nodeId);


	UPROPERTY()
	TArray<TObjectPtr<class UADSDialogue>> instancedDialogues;
};
