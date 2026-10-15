// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "AGSAction.h"
#include "CoreMinimal.h"

#include "ACFSetCharacterNameAction.generated.h"

class AACFCharacter;
class UADSDialoguePartecipantComponent;

/**
 * Graph action that renames the ACF Character speaking on the node this action belongs to.
 * In a dialogue graph every node is assigned to one of the dialogue participants: the owner of that
 * participant component is the character being renamed, not the actor owning the whole graph.
 * Useful to reveal the real name of an NPC once the conversation reaches a given line.
 */
UCLASS(Blueprintable, BlueprintType, EditInlineNew, HideCategories = ("DoNotShow"), CollapseCategories,
    AutoExpandCategories = ("Default"), meta = (DisplayName = "Set ACF Character Name", Keywords = "ACF Character Name Rename Participant Speaker Quest Dialogue"))
class ASCENTGRAPHINTEGRATIONS_API UACFSetCharacterNameAction : public UAGSAction {
    GENERATED_BODY()

public:
    /** Returns the actor assigned to the node running this action, the dialogue participant owner when there is one. */
    UFUNCTION(BlueprintPure, Category = "ACF | Graph Integrations", meta = (DisplayName = "Get Target Actor"))
    AActor* GetTargetActor() const;

    /** Returns the ACF Character this action will rename, or nullptr when the target is not one. */
    UFUNCTION(BlueprintPure, Category = "ACF | Graph Integrations", meta = (DisplayName = "Get Target ACF Character"))
    AACFCharacter* GetTargetCharacter() const;

    /** Returns the dialogue participant assigned to the node running this action, if the graph is a dialogue. */
    UFUNCTION(BlueprintPure, Category = "ACF | Graph Integrations", meta = (DisplayName = "Get Node Dialogue Participant"))
    UADSDialoguePartecipantComponent* GetNodeParticipant() const;

    /** Applies NewCharacterName to the target. Returns false when there is nothing to rename. */
    UFUNCTION(BlueprintCallable, Category = "ACF | Graph Integrations", meta = (DisplayName = "Apply Character Name"))
    bool ApplyCharacterName();

protected:
    virtual void ExecuteAction_Implementation(class APlayerController* playerController, UAGSGraphNode* nodeOwner) override;

    /** Name displayed by the UI for the character assigned to this node. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default")
    FText NewCharacterName;

    /** Renames the dialogue participant component too, since that is the name dialogue widgets display as speaker. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default")
    bool bAlsoRenameDialogueParticipant = true;

    /** Graphs without participants, quests above all, fall back to the actor owning the graph, or to its pawn when it is a controller. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default")
    bool bFallbackToGraphOwner = true;
};
