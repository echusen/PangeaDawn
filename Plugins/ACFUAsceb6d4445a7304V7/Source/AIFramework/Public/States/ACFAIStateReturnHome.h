// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "States/ACFAIState.h"

#include "ACFAIStateReturnHome.generated.h"

/**
 * @class UACFAIStateReturnHome
 *
 * AIState.ReturnHome: the agent walks back to its home location, which is published on the blackboard when
 * the state is entered. Entering it also drops the current target: an agent going home is not fighting
 * anyone, and the target must be dropped here and not by whoever asked for the transition, because that
 * transition can be refused.
 */
UCLASS()
class AIFRAMEWORK_API UACFAIStateReturnHome : public UACFAIState {
    GENERATED_BODY()

protected:
    virtual void OnStateEnter_Implementation(FGameplayTag previousState) override;

    virtual void OnHomeLocationChanged_Implementation(const FVector& newHomeLocation) override;
};
