// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Selectors/ACFAIStateSelector.h"

#include "ACFAIDefaultStateSelector.generated.h"

class AACFAIController;

/**
 * @class UACFAIDefaultStateSelector
 *
 * The decision ACF ships with, in order:
 *
 * - an agent that belongs to a group is teleported back near its lead, or sent to FollowLeadState, when it
 *   goes too far from it;
 * - a lone agent is sent to ReturnHomeState when it goes too far from its home, but only while it has no
 *   target: leaving a fight is up to the combat state, which drops the target over LoseTargetDistance;
 * - an agent with a target goes to CombatState.
 *
 * Distances and switches are read from the controller, so they stay per enemy type; only the states this
 * decision leads to are configured here.
 */
UCLASS()
class AIFRAMEWORK_API UACFAIDefaultStateSelector : public UACFAIStateSelector {
    GENERATED_BODY()

protected:
    /*State an agent with a target is sent to*/
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (Categories = "AIState"), Category = ACF)
    FGameplayTag CombatState;

    /*State an agent is sent to when it goes over its max distance from home*/
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (Categories = "AIState"), Category = ACF)
    FGameplayTag ReturnHomeState;

    /*State an agent that belongs to a group is sent to when it goes over its max distance from the lead.
    It is used only when it is also the agent's default state*/
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (Categories = "AIState"), Category = ACF)
    FGameplayTag FollowLeadState;

    virtual FGameplayTag SelectState_Implementation(float deltaTime) override;

    /*Keeps an agent near the lead of its group, teleporting it back when it is over the teleport distance*/
    UFUNCTION(BlueprintCallable, Category = ACF)
    virtual FGameplayTag SelectLeadLeashState();

    /*Keeps a lone agent near its home location*/
    UFUNCTION(BlueprintCallable, Category = ACF)
    virtual FGameplayTag SelectHomeLeashState();
};
