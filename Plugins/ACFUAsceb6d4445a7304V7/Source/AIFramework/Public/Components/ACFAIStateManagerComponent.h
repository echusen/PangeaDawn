// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Templates/SubclassOf.h"

#include "ACFAIStateManagerComponent.generated.h"

class AACFAIController;
class UACFAIState;
class UACFAIStateSelector;
class UACFAIStatesDataAsset;

/**
 * @class UACFAIStateManagerComponent
 *
 * The AI state machine: it owns the state the AI is in, the live object implementing each state and the
 * selector that picks the state the agent should be in.
 *
 * It knows no state tag: which states exist, what implements them and who chooses between them comes
 * entirely from AIStatesConfig. A state with no implementation cannot be entered, the transition to it
 * fails: nothing is invented in its place.
 *
 * Lives on AACFAIController, which forwards its own state API here to stay the single entry point for
 * game code.
 */
UCLASS(ClassGroup = (ACF), Blueprintable, meta = (BlueprintSpawnableComponent))
class AIFRAMEWORK_API UACFAIStateManagerComponent : public UActorComponent {
    GENERATED_BODY()

public:
    UACFAIStateManagerComponent();

    /*Binds this state machine to the controller that drives it and instances the configured states and
    selector. Called by AACFAIController on possess*/
    void Initialize(AACFAIController* controller);

    /*The state the AI is currently in*/
    UFUNCTION(BlueprintPure, Category = ACF)
    FGameplayTag GetCurrentAIState() const
    {
        return CurrentAIState;
    }

    /*Leaves the current state and enters the provided one.
    Fails, changing nothing, when the state has no implementation or when the AI is already in it*/
    UFUNCTION(BlueprintCallable, Category = ACF)
    bool SetCurrentAIState(UPARAM(meta = (Categories = "AIState")) FGameplayTag aiState);

    /*Returns the object implementing the provided state, null when that state has no implementation*/
    UFUNCTION(BlueprintPure, Category = ACF)
    UACFAIState* GetAIStateObject(UPARAM(meta = (Categories = "AIState")) FGameplayTag stateTag) const;

    /*Returns the object implementing the state the AI is currently in*/
    UFUNCTION(BlueprintPure, Category = ACF)
    UACFAIState* GetCurrentAIStateObject() const;

    /*True when the provided state can be entered, which means it has an implementation*/
    UFUNCTION(BlueprintPure, Category = ACF)
    bool CanEnterAIState(UPARAM(meta = (Categories = "AIState")) FGameplayTag stateTag) const;

    /*Replaces the implementation of a single state at runtime with an instance of the provided class.
    If the AI is already in that state, the old implementation is exited and the new one entered*/
    UFUNCTION(BlueprintCallable, Category = ACF)
    UACFAIState* SetAIStateImplementation(UPARAM(meta = (Categories = "AIState")) FGameplayTag stateTag, TSubclassOf<UACFAIState> stateClass);

    /*Asks the selector which state the agent should be in, enters it when it is another one, then ticks the
    current state. Called by the behavior tree state service*/
    UFUNCTION(BlueprintCallable, Category = ACF)
    void TickStateMachine(float deltaTime);

    /*Asks the selector to decide right now instead of waiting for the next tick, and returns true when the
    agent changed state. Call it when something the decision depends on changed, like the current target:
    the state service ticks once per second, far too slow to react to a hit*/
    UFUNCTION(BlueprintCallable, Category = ACF)
    bool EvaluateStateSelector();

    /*Enters the state configured for a dead agent*/
    UFUNCTION(BlueprintCallable, Category = ACF)
    bool EnterDeathState();

    /*True when the state the agent is in is a battle state, which every state answers for itself*/
    UFUNCTION(BlueprintPure, Category = ACF)
    bool IsInBattleState() const;

    /*Tells the current state that the agent was given a new home location*/
    UFUNCTION(BlueprintCallable, Category = ACF)
    void NotifyHomeLocationChanged(const FVector& newHomeLocation);

    /*The states this AI can be in and what implements them*/
    UFUNCTION(BlueprintPure, Category = ACF)
    UACFAIStatesDataAsset* GetAIStatesConfig() const
    {
        return AIStatesConfig;
    }

    /*The selector deciding which state this AI should be in*/
    UFUNCTION(BlueprintPure, Category = ACF)
    UACFAIStateSelector* GetStateSelector() const
    {
        return StateSelector;
    }

    /*Replaces the whole state set at runtime: the AI leaves its current state, the new set is instanced and
    the AI enters the state it was in again, now implemented by the new asset*/
    UFUNCTION(BlueprintCallable, Category = ACF)
    void SetAIStatesConfig(UACFAIStatesDataAsset* newConfig);

    /*The controller driving this state machine*/
    UFUNCTION(BlueprintPure, Category = ACF)
    AACFAIController* GetAIController() const
    {
        return aiController;
    }

protected:
    /*The states this AI can be in, the object implementing each one and the selector choosing between them.
    This is the only place where a state tag is bound to an implementation*/
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = ACF)
    TObjectPtr<UACFAIStatesDataAsset> AIStatesConfig;

    /*Creates one live instance per state configured in AIStatesConfig, plus one for its selector*/
    virtual void InitializeAIStates();

private:
    /*Instances stateClass, stores it as the implementation of stateTag and binds it to the controller*/
    UACFAIState* CreateAIStateForTag(const FGameplayTag& stateTag, TSubclassOf<UACFAIState> stateClass);

    /*Asks the selector and enters what it answers. Returns true when the agent changed state*/
    bool EvaluateSelector(float deltaTime);

    /*The live state objects, one per state tag: instanced from AIStatesConfig, never serialized*/
    UPROPERTY(Transient)
    TMap<FGameplayTag, UACFAIState*> AIStates;

    /*The live selector, instanced from AIStatesConfig*/
    UPROPERTY(Transient)
    TObjectPtr<UACFAIStateSelector> StateSelector;

    UPROPERTY()
    FGameplayTag CurrentAIState;

    /*Set by Initialize. Transient: a state machine configured in the editor must never serialize a
    reference to the controller it was authored on*/
    UPROPERTY(Transient)
    TObjectPtr<AACFAIController> aiController;
};
