// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "Components/ACFAIStateManagerComponent.h"
#include "ACFAIController.h"
#include "Data/ACFAIStatesDataAsset.h"
#include "Selectors/ACFAIStateSelector.h"
#include "States/ACFAIState.h"
#include <Logging.h>

UACFAIStateManagerComponent::UACFAIStateManagerComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UACFAIStateManagerComponent::Initialize(AACFAIController* controller)
{
    aiController = controller;
    InitializeAIStates();
}

void UACFAIStateManagerComponent::InitializeAIStates()
{
    AIStates.Reset();
    StateSelector = nullptr;

    // A missing or incomplete configuration is reported by the controller, which knows the pawn it drives
    if (!AIStatesConfig) {
        return;
    }

    for (const auto& configuredState : AIStatesConfig->AIStates) {
        if (!configuredState.Key.IsValid() || !configuredState.Value) {
            continue;
        }

        /* A class that is abstract has no business being in the data: it cannot be instanced, so the state
        would end up without an implementation. It happens to an asset authored before that class became
        abstract, which is why it is checked here and not only when the picker offers the classes */
        if (configuredState.Value->GetClass()->HasAnyClassFlags(CLASS_Abstract)) {
            UE_LOG(ACFAILog, Error, TEXT("%s configures %s with the abstract class %s: that state has no implementation and cannot be entered - UACFAIStateManagerComponent::InitializeAIStates"),
                *AIStatesConfig->GetName(), *configuredState.Key.ToString(), *configuredState.Value->GetClass()->GetName());
            continue;
        }

        /* Every AI needs its own instance: a state keeps a reference to the controller it drives, so the
        objects stored in the asset are templates and are never used directly */
        UACFAIState* newState = DuplicateObject<UACFAIState>(configuredState.Value, this);
        if (!newState) {
            continue;
        }

        newState->InternalInitialize(this, aiController, configuredState.Key);
        AIStates.Add(configuredState.Key, newState);
    }

    if (AIStatesConfig->StateSelector) {
        if (AIStatesConfig->StateSelector->GetClass()->HasAnyClassFlags(CLASS_Abstract)) {
            UE_LOG(ACFAILog, Error, TEXT("%s configures its selector with the abstract class %s: this AI decides nothing on its own - UACFAIStateManagerComponent::InitializeAIStates"),
                *AIStatesConfig->GetName(), *AIStatesConfig->StateSelector->GetClass()->GetName());
        } else {
            StateSelector = DuplicateObject<UACFAIStateSelector>(AIStatesConfig->StateSelector, this);
            if (StateSelector) {
                StateSelector->InternalInitialize(this, aiController);
            }
        }
    }
}

void UACFAIStateManagerComponent::SetAIStatesConfig(UACFAIStatesDataAsset* newConfig)
{
    if (AIStatesConfig == newConfig) {
        return;
    }

    const FGameplayTag stateToRestore = CurrentAIState;

    if (UACFAIState* currentState = GetCurrentAIStateObject()) {
        currentState->OnStateExit(stateToRestore);
    }

    AIStatesConfig = newConfig;
    CurrentAIState = FGameplayTag();
    InitializeAIStates();

    SetCurrentAIState(stateToRestore);
}

bool UACFAIStateManagerComponent::CanEnterAIState(FGameplayTag stateTag) const
{
    return GetAIStateObject(stateTag) != nullptr;
}

bool UACFAIStateManagerComponent::SetCurrentAIState(FGameplayTag aiState)
{
    if (CurrentAIState == aiState) {
        return false;
    }

    /* A state with no implementation cannot be entered: the transition fails and the agent stays in the
    state it is in, which is a working one, instead of being left with an implementation invented here */
    UACFAIState* newStateObject = GetAIStateObject(aiState);
    if (!newStateObject) {
        UE_LOG(ACFAILog, Error, TEXT("%s cannot enter %s: no implementation for it in %s. The AI stays in %s - UACFAIStateManagerComponent::SetCurrentAIState"),
            aiController ? *aiController->GetName() : *GetName(),
            *aiState.ToString(),
            AIStatesConfig ? *AIStatesConfig->GetName() : TEXT("no AI States Data Asset"),
            *CurrentAIState.ToString());
        return false;
    }

    const FGameplayTag previousState = CurrentAIState;

    /* What entering and leaving a state does belongs to the state itself: see UACFAIState and its children */
    if (UACFAIState* previousStateObject = GetAIStateObject(previousState)) {
        previousStateObject->OnStateExit(aiState);
    }

    CurrentAIState = aiState;
    newStateObject->OnStateEnter(previousState);

    if (aiController) {
        aiController->HandleAIStateChanged(aiState);
    }

    return true;
}

UACFAIState* UACFAIStateManagerComponent::CreateAIStateForTag(const FGameplayTag& stateTag, TSubclassOf<UACFAIState> stateClass)
{
    if (!stateTag.IsValid() || !stateClass) {
        return nullptr;
    }

    if (stateClass->HasAnyClassFlags(CLASS_Abstract)) {
        UE_LOG(ACFAILog, Error, TEXT("%s is abstract and cannot implement %s - UACFAIStateManagerComponent::CreateAIStateForTag"),
            *stateClass->GetName(), *stateTag.ToString());
        return nullptr;
    }

    UACFAIState* newState = NewObject<UACFAIState>(this, stateClass);
    if (!newState) {
        return nullptr;
    }

    newState->InternalInitialize(this, aiController, stateTag);
    AIStates.Add(stateTag, newState);
    return newState;
}

UACFAIState* UACFAIStateManagerComponent::GetAIStateObject(FGameplayTag stateTag) const
{
    UACFAIState* const* liveState = AIStates.Find(stateTag);
    return liveState && IsValid(*liveState) ? *liveState : nullptr;
}

UACFAIState* UACFAIStateManagerComponent::GetCurrentAIStateObject() const
{
    return GetAIStateObject(CurrentAIState);
}

UACFAIState* UACFAIStateManagerComponent::SetAIStateImplementation(FGameplayTag stateTag, TSubclassOf<UACFAIState> stateClass)
{
    if (!stateTag.IsValid() || !stateClass) {
        UE_LOG(ACFAILog, Error, TEXT("Invalid state tag or class - UACFAIStateManagerComponent::SetAIStateImplementation"));
        return nullptr;
    }

    const bool bIsCurrentState = CurrentAIState == stateTag;

    if (UACFAIState* previousState = GetAIStateObject(stateTag)) {
        if (bIsCurrentState) {
            previousState->OnStateExit(stateTag);
        }
    }

    UACFAIState* newState = CreateAIStateForTag(stateTag, stateClass);
    if (newState && bIsCurrentState) {
        newState->OnStateEnter(stateTag);
    }

    return newState;
}

void UACFAIStateManagerComponent::TickStateMachine(float deltaTime)
{
    /* The behavior tree keeps ticking on a dead agent, and a dead agent must not pick a new state: its
    target is gone but anything the selector reads could still put it back into a fight it cannot fight.
    The state it died in is the one it keeps */
    if (aiController && !aiController->IsPawnAlive()) {
        return;
    }

    EvaluateSelector(deltaTime);

    if (UACFAIState* currentState = GetCurrentAIStateObject()) {
        currentState->OnStateTick(deltaTime);
    }
}

bool UACFAIStateManagerComponent::EvaluateStateSelector()
{
    return EvaluateSelector(0.f);
}

bool UACFAIStateManagerComponent::EvaluateSelector(float deltaTime)
{
    if (!StateSelector) {
        return false;
    }

    const FGameplayTag selectedState = StateSelector->SelectState(deltaTime);
    return selectedState.IsValid() && selectedState != CurrentAIState && SetCurrentAIState(selectedState);
}

bool UACFAIStateManagerComponent::EnterDeathState()
{
    const FGameplayTag deathState = AIStatesConfig ? AIStatesConfig->DeathState : FGameplayTag();
    if (!deathState.IsValid()) {
        UE_LOG(ACFAILog, Error, TEXT("%s has no Death State configured in %s: a dead agent keeps the state it died in - UACFAIStateManagerComponent::EnterDeathState"),
            aiController ? *aiController->GetName() : *GetName(),
            AIStatesConfig ? *AIStatesConfig->GetName() : TEXT("no AI States Data Asset"));
        return false;
    }

    return SetCurrentAIState(deathState);
}

bool UACFAIStateManagerComponent::IsInBattleState() const
{
    const UACFAIState* currentState = GetCurrentAIStateObject();
    return currentState && currentState->IsBattleState();
}

void UACFAIStateManagerComponent::NotifyHomeLocationChanged(const FVector& newHomeLocation)
{
    if (UACFAIState* currentState = GetCurrentAIStateObject()) {
        currentState->OnHomeLocationChanged(newHomeLocation);
    }
}
