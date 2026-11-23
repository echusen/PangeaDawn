// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"

#include "ACFAIStatesDataAsset.generated.h"

class UACFAIState;
class UACFAIStateSelector;

/**
 * @class UACFAIStatesDataAsset
 *
 * The states an AI can be in, the object implementing each one, and the selector that decides which one the
 * agent should be in.
 *
 * This asset is the only place where a state tag is bound to its implementation: neither the state machine
 * component nor the AI controller knows a single state tag. Assign it on the AI controller
 * (UACFAIStateManagerComponent::AIStatesConfig) and swap one entry to reimplement one state.
 *
 * A state that is not in here cannot be entered: the transition to it is refused and logged, so a missing
 * entry shows up as an error instead of an agent that quietly does nothing.
 */
UCLASS(BlueprintType)
class AIFRAMEWORK_API UACFAIStatesDataAsset : public UPrimaryDataAsset {
    GENERATED_BODY()

public:
    /*Implementation of each AI state, by state tag*/
    UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, meta = (Categories = "AIState"), Category = "ACF|AI State")
    TMap<FGameplayTag, UACFAIState*> AIStates;

    /*Decides which of the states above the agent should be in, every state machine tick. This is where the
    rules that apply to every state live, the leash included*/
    UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category = "ACF|AI State")
    TObjectPtr<UACFAIStateSelector> StateSelector;

    /*State a dead agent is put in. The controller asks for it when its pawn dies, so not even death
    needs a tag written in code*/
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (Categories = "AIState"), Category = "ACF|AI State")
    FGameplayTag DeathState;

    /*The implementation configured for the provided state, null if that state has no entry*/
    UFUNCTION(BlueprintPure, Category = "ACF|AI State")
    UACFAIState* GetStateImplementation(UPARAM(meta = (Categories = "AIState")) FGameplayTag stateTag) const
    {
        UACFAIState* const* configuredState = AIStates.Find(stateTag);
        return configuredState ? *configuredState : nullptr;
    }
};
