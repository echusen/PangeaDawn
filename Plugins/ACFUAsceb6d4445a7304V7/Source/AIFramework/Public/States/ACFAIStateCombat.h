// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "States/ACFAIState.h"

#include "ACFAIStateCombat.generated.h"

/**
 * @class UACFAIStateCombat
 *
 * AIState.Combat: the agent is fighting its current target. Entering the state initializes the combat
 * behaviour (weapon, combat movement and the actions the combat behavior tree can play) and leaving it
 * uninitializes it. The leash of the base state still applies, so an agent that chases its target too far
 * from its home or from its lead is sent back, unless it is retaliating against something that is hitting it.
 */
UCLASS()
class AIFRAMEWORK_API UACFAIStateCombat : public UACFAIState {
    GENERATED_BODY()

public:
    virtual bool IsBattleState_Implementation() const override;

protected:
    virtual void OnStateEnter_Implementation(FGameplayTag previousState) override;

    virtual void OnStateExit_Implementation(FGameplayTag nextState) override;
};
