// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "States/ACFAIState.h"

#include "ACFAIStateFollowLead.generated.h"

/**
 * @class UACFAIStateFollowLead
 *
 * AIState.FollowLead: the agent walks back to the lead of its group. Entering the state focuses the lead
 * actor, so the agent looks at it while it closes the distance.
 */
UCLASS()
class AIFRAMEWORK_API UACFAIStateFollowLead : public UACFAIState {
    GENERATED_BODY()

protected:
    virtual void OnStateEnter_Implementation(FGameplayTag previousState) override;
};
