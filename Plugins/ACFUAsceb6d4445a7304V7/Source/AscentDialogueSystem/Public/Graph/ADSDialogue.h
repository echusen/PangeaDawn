// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "ADSBaseDialoguePartecipantComponent.h"
#include "ADSTypes.h"
#include "AGSGraph.h"
#include "CoreMinimal.h"
#include <Engine/DataTable.h>
#include <GameFramework/Character.h>

#include "ADSDialogue.generated.h"

class UADSDialoguePartecipantComponent;
class UADSStartDialogueNode;
class UADSGraphNode;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDialogueNodeActivated, const FGuid&, nodeId);

/**
 * Represents a dialogue asset built on top of UAGSGraph, managing the flow and logic of dialogue nodes.
 * Handles dialogue participants, node transitions, and broadcasting dialogue-related events.
 */
UCLASS()
class ASCENTDIALOGUESYSTEM_API UADSDialogue : public UAGSGraph {
	GENERATED_BODY()

public:
	UADSDialogue();


	/**
	 * Starts the dialogue sequence with the provided participants and player controller.
	 * Broadcasts OnDialogueStarted and activates the first node of the dialogue.
	 *
	 * @param inController The player controller starting the dialogue.
	 * @param participants The array of dialogue participants involved in this conversation.
	 * @return The first dialogue node that gets activated.
	 */
	UFUNCTION(BlueprintCallable, Category = ADS)
	class UAGSGraphNode* StartDialogue(class APlayerController* inController, const TArray<UADSBaseDialoguePartecipantComponent*>& participants);

	/**
	 * Retrieves all response nodes available for the current active dialogue node.
	 *
	 * @return An array of dialogue response nodes representing the player's possible answers.
	 */
	UFUNCTION(BlueprintCallable, Category = ADS)
	TArray<class UADSDialogueResponseNode*> GetAllButtonAnswersForCurrentNode() const;

	/**
	 * Moves the dialogue to the next node based on the current node's output or selected response.
	 * Broadcasts OnDialogueNodeActivated when a new node is reached.
	*
	* @return The next dialogue node, or nullptr if the dialogue has ended.
	*/
	UFUNCTION(BlueprintCallable, Category = ADS)
	class UADSDialogueNode* MoveToNextNode();

	/**
	 * Checks whether the dialogue has a participant with the specified tag.
	 *
	 * @param partecipantTag The gameplay tag identifying the participant.
	 * @return True if the participant is part of this dialogue, false otherwise.
	 */
	UFUNCTION(BlueprintPure, Category = ADS)
	bool HasPartecipant(const FGameplayTag& partecipantTag) const { return partecipantsRef.Contains(partecipantTag); }

	/**
	 * Returns whether the dialogue has started.
	 *
	 * @return True if the dialogue has been initiated, false otherwise.
	 */
	UFUNCTION(BlueprintPure, Category = ADS)
	bool IsDialogueStarted() const { return bIsStarted; }

	/**
	 * Requests a pause. The current node keeps playing; when it finishes the dialogue holds
	 * instead of advancing. Call Resume Dialogue to continue from the next node.
	 * @return True if the dialogue is started and will pause (or is already paused).
	 */
	UFUNCTION(BlueprintCallable, Category = ADS, meta = (DisplayName = "Pause Dialogue", Keywords = "pause hold wait"))
	bool PauseDialogue();

	/**
	 * Cancels a pending pause, or continues from the held node.
	 * If the current node is still playing, it is allowed to finish normally.
	 * @return True if a pause was pending or the dialogue was held.
	 */
	UFUNCTION(BlueprintCallable, Category = ADS, meta = (DisplayName = "Resume Dialogue", Keywords = "resume continue unpause"))
	bool ResumeDialogue();

	/** True while the dialogue is holding at the end of the current node. */
	UFUNCTION(BlueprintPure, Category = ADS, meta = (DisplayName = "Is Dialogue Paused"))
	bool IsDialoguePaused() const { return bIsPaused; }

	/** True after Pause Dialogue was called, while the current node is still playing. */
	UFUNCTION(BlueprintPure, Category = ADS, meta = (DisplayName = "Is Dialogue Pause Requested"))
	bool IsDialoguePauseRequested() const { return bPauseRequested; }

	/**
	* Finds and returns a dialogue participant component by gameplay tag.
	*
	* @param partecipantTag The gameplay tag identifying the participant.
	* @return The participant component if found, nullptr otherwise.
	*/
	UFUNCTION(BlueprintCallable, Category = ADS)
	UADSBaseDialoguePartecipantComponent* FindPartecipant(FGameplayTag partecipantTag) const;

	/** Event triggered when the dialogue starts. */
	UPROPERTY(BlueprintAssignable, Category = ADS)
	FOnDialogueStarted OnDialogueStarted;

	/** Event triggered when the dialogue ends. */
	UPROPERTY(BlueprintAssignable, Category = ADS)
	FOnDialogueEnded OnDialogueEnded;

	/** Event triggered when a dialogue node is activated. */
	UPROPERTY(BlueprintAssignable, Category = ADS)
	FOnDialogueNodeActivated OnDialogueNodeActivated;

	/** Fired when the current node has finished and the dialogue is holding. */
	UPROPERTY(BlueprintAssignable, Category = ADS)
	FOnDialoguePaused OnDialoguePaused;

	/** Fired when Resume Dialogue cancels a pending pause or continues from a hold. */
	UPROPERTY(BlueprintAssignable, Category = ADS)
	FOnDialogueResumed OnDialogueResumed;


	/**
	 * Returns the current active dialogue node.
	 *
	 * @return The currently active node in the dialogue graph.
	 */
	UFUNCTION(BlueprintPure, Category = ADS)
	FORCEINLINE class UADSGraphNode* GetCurrentNode() const { return currentNode; }

	/**
	 * Returns the gameplay tag identifying this dialogue.
	 *
	 * @return The dialogue tag.
	 */
	UFUNCTION(BlueprintPure, Category = ADS)
	FORCEINLINE FGameplayTag GetDialogueTag() const { return DialogueTag; }

	/**
	 * Returns the default participant tag used when no explicit participant is defined.
	 *
	 * @return The default participant tag.
	 */
	UFUNCTION(BlueprintPure, Category = ADS)
	FORCEINLINE FGameplayTag GetDefaultParticipantTag() const { return DefaultParticipantTag; }

#if WITH_EDITOR
	/** /Game/.../ package dir for TTS audio generated from this dialogue. Falls back to Ascent Dialogue Settings. */
	FString ResolveVoiceOutputPackagePath() const;

	/** /Game/.../ package dir for facial animations generated from this dialogue. Falls back to Ascent Dialogue Settings. */
	FString ResolveAnimationOutputPackagePath() const;

	/** /Game/.../ package dir for ARKit animations. Uses this dialogue's animation folder when set. */
	FString ResolveARKitOutputPackagePath() const;
#endif

	FORCEINLINE bool operator==(const FGameplayTag& OtherTag) const
	{
		return this->DialogueTag == OtherTag;
	}

	FORCEINLINE bool operator!=(const FGameplayTag& OtherTag) const
	{
		return this->DialogueTag != OtherTag;
	}
	FORCEINLINE bool operator==(const UADSDialogue*& Other) const
	{
		return DialogueTag == Other->DialogueTag;
	}

	FORCEINLINE bool operator!=(const UADSDialogue*& Other) const
	{
		return DialogueTag != Other->DialogueTag;
	}

	
#if WITH_EDITORONLY_DATA
	/** Directory path for exporting/importing dialogue screenplay files */
	UPROPERTY(EditDefaultsOnly, Category = "Yarin|Export Import")
	FDirectoryPath ExportImportDirectory;

	/**
	 * Content folder where TTS audio for this dialogue is saved.
	 * Leave empty to use the project default in Ascent Dialogue Settings.
	 * A subfolder named after this dialogue asset is always created inside the chosen folder.
	 */
	UPROPERTY(EditAnywhere, Category = "Yarin|Generation Output",
		meta = (ContentDir, LongPackageName, RelativeToGameContentDir,
			DisplayName = "Voice Output Folder"))
	FDirectoryPath VoiceOutputFolder;

	/**
	 * Content folder where facial and ARKit animations for this dialogue are saved.
	 * Leave empty to use the project defaults in Ascent Dialogue Settings.
	 * A subfolder named after this dialogue asset is always created inside the chosen folder.
	 */
	UPROPERTY(EditAnywhere, Category = "Yarin|Generation Output",
		meta = (ContentDir, LongPackageName, RelativeToGameContentDir,
			DisplayName = "Animation Output Folder"))
	FDirectoryPath AnimationOutputFolder;

	/** Textual description / constraints of what this conversation should be about.
	 *  Used as the main prompt when generating the entire dialogue with AI.
	 *  To add player branching, explicitly mention it here, e.g.:
	 *  "The player can decide whether to help the guard or refuse." */
	UPROPERTY(EditDefaultsOnly, Category = "Yarin", meta = (MultiLine = true))
	FString AIConversationDescription;

	/** Characters that must appear in the generated dialogue.
	 *  The AI will use ONLY these tags as speakers — it will never invent new characters.
	 *  Only the tag containing "Player" can have response options (branching);
	 *  all other characters always speak in linear dialogue nodes. */
	UPROPERTY(EditDefaultsOnly, Category = "Yarin")
	TArray<FGameplayTag> AIParticipantTags;

	/** Approximate number of dialogue beats (speaker turns) to generate.
	 *  This is a soft hint to the AI — the actual length may vary slightly
	 *  depending on the conversation description and branching. */
	UPROPERTY(EditAnywhere, Category = "Yarin")
	int32 AIDialogueTreeLength = 8;

	/** Controls the node type used for player lines:
	 *  - Player Answers: all player lines are button-answer nodes (1 button per beat by default).
	 *  - Player Nodes: all player lines are normal dialogue nodes (no button answers).
	 *  - Mixed: AI picks button-answer or dialogue node per beat.
	 *  In ALL modes, branches (multiple options) are generated ONLY when the
	 *  AI Conversation Description explicitly describes a player choice. */
	UPROPERTY(EditDefaultsOnly, Category = "Yarin")
	EADSAIPlayerResponseMode AIPlayerResponseMode = EADSAIPlayerResponseMode::Mixed;
#endif

	bool ActivateNode(class UAGSGraphNode* node) override;
	void EndDialogue();
	bool GetUseDefaultCameraSettings() const { return UseDefaultCameraSettings; }
	void SetUseDefaultCameraSettings(bool val) { UseDefaultCameraSettings = val; }
protected:
	/*Unique Tag for this Dialogue*/
	UPROPERTY(EditDefaultsOnly, Category = ADS)
	FGameplayTag DialogueTag;

	UPROPERTY(EditDefaultsOnly, meta = (Categories = "Character"), Category = ADS)
	FGameplayTag DefaultParticipantTag;

	UPROPERTY(EditDefaultsOnly, Category = ADS)
	bool UseDefaultCameraSettings;

private:
	TMap<FGameplayTag, TObjectPtr<UADSBaseDialoguePartecipantComponent>> partecipantsRef;

	TObjectPtr<UADSGraphNode> currentNode;

	TObjectPtr<UADSStartDialogueNode> currentDialogueStart;

	bool bIsStarted = false;
	bool bPauseRequested = false;
	bool bIsPaused = false;

	bool IsCurrentNodeWaitingToFinish() const;
	void EnterPausedState();
	void ClearPauseState();
};
