// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.

#pragma once

#include "AGSGraphNode.h"
#include "CoreMinimal.h"
#include <GameplayTagContainer.h>

#include "ADSGraphNode.generated.h"

class UADSDialoguePartecipantComponent;
class APlayerController;
class UAudioComponent;
class USoundCue;
class UAGSCondition;
class UAGSAction;

/**
 * Base class for all Dialogue Graph Nodes inside the Ascent Dialogue System.
 * Represents a single node of dialogue with associated participant, text, and optional sound.
 */
UCLASS(Blueprintable, abstract)
class ASCENTDIALOGUESYSTEM_API UADSGraphNode : public UAGSGraphNode {
    GENERATED_BODY()

    friend class UADSDialogue;

public:
    /**
     * Returns the dialogue participant associated with this node.
     *
     * @return The participant component for this dialogue node.
     */
    UFUNCTION(BlueprintPure, Category = ADS)
    UADSDialoguePartecipantComponent* GetDialogueParticipant() const;

    /**
     * Returns the dialogue participant voice associated with this node.
     *
     * @return The participant voice  for this dialogue node.
     */
    UFUNCTION(BlueprintPure, Category = ADS)
    bool TryGetParticipantVoiceConfig(FADSVoiceSettings& outVoiceConfig) const;

    /**
     * Returns the sound asset that should be played for this dialogue node.
     *
     * @return The sound to play, or nullptr if none is assigned.
     */
    UFUNCTION(BlueprintPure, Category = ADS)
    USoundBase* GetSoundToPlay() const
    {
        return SoundToPlay;
    }

    /**
     * Checks if the participant associated with this node is the local player.
     *
     * @return True if the participant is controlled by the local player, false otherwise.
     */
    UFUNCTION(BlueprintPure, Category = ADS)
    bool IsLocalPlayerPartecipant() const;

    /**
     * Determines if this node can be activated by a given player controller.
     *
     * @param inController The player controller attempting to activate this node.
     * @return True if the node can be activated, false otherwise.
     */
    UFUNCTION(BlueprintCallable, Category = ADS)
    virtual bool CanBeActivated(class APlayerController* inController);

    /**
     * Sets the sound asset to play for this dialogue line.
     * @param InSound - The sound (SoundWave or SoundCue) to assign.
     */
    UFUNCTION(BlueprintCallable, Category = ADS)
    void SetSoundToPlay(USoundBase* InSound) { SoundToPlay = InSound; }

    /**
     * Sets the main animation montage associated with this dialogue line.
     * @param InAnimation - The animation montage to assign.
     */
    UFUNCTION(BlueprintCallable, Category = ADS)
    void SetAnimation(UAnimMontage* InAnimation) { Animation = InAnimation; }

    /**
     * Sets the facial animation montage for this dialogue line.
     * @param InFacialAnimation - The facial animation montage to assign.
     */
    UFUNCTION(BlueprintCallable, Category = ADS)
    void SetFacialAnimation(UAnimMontage* InFacialAnimation) { FacialAnimation = InFacialAnimation; }

    /**
     * Sets the dialogue text.
     * @param InText - The text content of the dialogue line.
     */
    UFUNCTION(BlueprintCallable, Category = ADS)
    void SetText(const FText& InText) { Text = InText; }
    /**
     * Returns the dialogue text associated with this node.
     *
     * @return The dialogue text.
     */
    UFUNCTION(BlueprintPure, Category = ADS)
    FORCEINLINE FText GetDialogueText() const { return Text; }

    /**
     * Returns the participant tag.
     * @return The participant gameplay tag.
     */
    UFUNCTION(BlueprintPure, Category = "ADS", meta = (DisplayName = "Get Participant Tag"))
    FGameplayTag GetParticipantTag() const { return PartecipantTag; }

#if WITH_EDITOR
    virtual FText GetNodeTitle() const override;
    virtual FText GetParagraphTitle() const override;

    virtual void InitializeNode() override;
#endif
protected:
    /** Participant gameplay tag. */
    UPROPERTY(EditAnywhere, meta = (DisplayName = "Participant", Categories = "Character"), BlueprintReadOnly, Category = "ADS")
    FGameplayTag PartecipantTag;

    /** Actions triggered when this node activates. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Instanced, Category = "ADS|Events")
    TArray<UAGSAction*> ActivationActions;

    /*Conditions that must be met for the dialogue or interaction to be activated. These are instances of UAGSCondition and can be customized per instance*/
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Instanced, Category = "ADS|Events")
    TArray<UAGSCondition*> ActivationConditions;

    /** Sound asset to play (SoundWave or SoundCue). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ADS|Dialogue", meta = (AllowedClasses = "/Script/Engine.SoundWave,/Script/Engine.SoundCue"))
    class USoundBase* SoundToPlay;

    /** Character animation montage. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ADS|Dialogue")
    class UAnimMontage* Animation;

    /** Facial animation montage. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ADS|Dialogue")
    class UAnimMontage* FacialAnimation;

    /** The dialogue text displayed and/or spoken in this node. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (MultiLine = "true"), Category = "ADS|Dialogue")
    FText Text;

    virtual void ActivateNode() override;

    virtual void DeactivateNode() override;

    UADSDialoguePartecipantComponent* GatherPartecipant() const;

    FText ContextMenuName;

    // References
    TObjectPtr<UADSDialoguePartecipantComponent> partecipant;

    TObjectPtr<UAudioComponent> audioPlayer;
    TObjectPtr<APlayerController> controller;
};
