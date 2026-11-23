// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

#include "ACFAIStateSelector.generated.h"

class AACFAIController;
class UACFAIStateManagerComponent;

/**
 * @class UACFAIStateSelector
 *
 * Decides which state an AI should be in, every state machine tick.
 *
 * This is what keeps the rules that apply to every state out of the states themselves: "too far from home,
 * go back" and "too far from the lead, follow it" are decisions about the agent, not behaviors of the state
 * it happens to be in, so neither UACFAIState nor its children know about them.
 *
 * A selector names the states it leads to with tags it reads from its own properties, so it never references
 * a state implementation. It lives in the UACFAIStatesDataAsset, next to the states it chooses between.
 */
UCLASS(Blueprintable, BlueprintType, EditInlineNew, DefaultToInstanced, CollapseCategories, AutoExpandCategories = ("Default"))
class AIFRAMEWORK_API UACFAIStateSelector : public UObject {
    GENERATED_BODY()

public:
    friend class UACFAIStateManagerComponent;

    /*The controller this selector decides for*/
    UFUNCTION(BlueprintPure, Category = ACF)
    AACFAIController* GetAIController() const
    {
        return aiController;
    }

    /*The state machine this selector drives*/
    UFUNCTION(BlueprintPure, Category = ACF)
    UACFAIStateManagerComponent* GetStateMachine() const
    {
        return stateMachine;
    }

    virtual UWorld* GetWorld() const override;

protected:
    /*The state the agent should be in right now. Return an invalid tag, which is what the default
    implementation does, to leave the agent in the state it is already in*/
    UFUNCTION(BlueprintNativeEvent, Category = ACF)
    FGameplayTag SelectState(float deltaTime);
    virtual FGameplayTag SelectState_Implementation(float deltaTime);

private:
    void InternalInitialize(UACFAIStateManagerComponent* machine, AACFAIController* controller);

    /*The state machine running this selector. Transient: a selector configured in the editor must never
    serialize a reference to the AI it was authored on*/
    UPROPERTY(Transient)
    TObjectPtr<UACFAIStateManagerComponent> stateMachine;

    UPROPERTY(Transient)
    TObjectPtr<AACFAIController> aiController;
};
