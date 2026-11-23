// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

#include "ACFAIState.generated.h"

class AACFAIController;
class AACFCharacter;
class UACFAIStateManagerComponent;

/**
 * @class UACFAIState
 *
 * Base class of the implementation of one AI state. Every state has its own class, even one that does
 * nothing yet, so that there is always a place where its behavior goes.
 *
 * The behavior tree still owns the actions an agent plays while it is in a state: a state object owns what
 * happens when the agent enters and leaves that state, and whatever has to run while it is the current one.
 * Which state an implementation serves is declared in a UACFAIStatesDataAsset, not in code.
 *
 * A state never decides to become another state: the rules that apply to every state live in the
 * UACFAIStateSelector of that same data asset, so this class knows nothing about its siblings.
 */
UCLASS(Blueprintable, BlueprintType, abstract, EditInlineNew, DefaultToInstanced, CollapseCategories, AutoExpandCategories = ("Default"))
class AIFRAMEWORK_API UACFAIState : public UObject {
    GENERATED_BODY()

public:
    friend class UACFAIStateManagerComponent;

    /*The controller executing this state*/
    UFUNCTION(BlueprintPure, Category = ACF)
    AACFAIController* GetAIController() const
    {
        return aiController;
    }

    /*The state machine running this state*/
    UFUNCTION(BlueprintPure, Category = ACF)
    UACFAIStateManagerComponent* GetStateMachine() const
    {
        return stateMachine;
    }

    /*The AI state tag this object implements*/
    UFUNCTION(BlueprintPure, Category = ACF)
    FGameplayTag GetStateTag() const
    {
        return stateTag;
    }

    /*The character controlled by this state's controller*/
    UFUNCTION(BlueprintPure, Category = ACF)
    AACFCharacter* GetControlledCharacter() const;

    /*True when being in this state means the agent is in battle. It is what AACFAIController::IsInBattle
    answers, so no one has to compare the current state with a tag*/
    UFUNCTION(BlueprintNativeEvent, Category = ACF)
    bool IsBattleState() const;
    virtual bool IsBattleState_Implementation() const;

    virtual UWorld* GetWorld() const override;

protected:
    /*Called once when the AI enters this state*/
    UFUNCTION(BlueprintNativeEvent, Category = ACF)
    void OnStateEnter(UPARAM(meta = (Categories = "AIState")) FGameplayTag previousState);
    virtual void OnStateEnter_Implementation(FGameplayTag previousState);

    /*Called once when the AI leaves this state*/
    UFUNCTION(BlueprintNativeEvent, Category = ACF)
    void OnStateExit(UPARAM(meta = (Categories = "AIState")) FGameplayTag nextState);
    virtual void OnStateExit_Implementation(FGameplayTag nextState);

    /*Called every state machine tick while this is the current state*/
    UFUNCTION(BlueprintNativeEvent, Category = ACF)
    void OnStateTick(float deltaTime);
    virtual void OnStateTick_Implementation(float deltaTime);

    /*Called while this is the current state when the agent is given a new home location*/
    UFUNCTION(BlueprintNativeEvent, Category = ACF)
    void OnHomeLocationChanged(const FVector& newHomeLocation);
    virtual void OnHomeLocationChanged_Implementation(const FVector& newHomeLocation);

private:
    void InternalInitialize(UACFAIStateManagerComponent* machine, AACFAIController* controller, const FGameplayTag& inStateTag);

    /*The state machine this state belongs to. Transient: a state configured in the editor must never
    serialize a reference to the AI it was authored on*/
    UPROPERTY(Transient)
    TObjectPtr<UACFAIStateManagerComponent> stateMachine;

    UPROPERTY(Transient)
    TObjectPtr<AACFAIController> aiController;

    FGameplayTag stateTag;
};
