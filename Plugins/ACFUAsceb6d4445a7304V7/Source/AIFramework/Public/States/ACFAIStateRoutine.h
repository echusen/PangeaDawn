// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "States/ACFAIState.h"

#include "ACFAIStateRoutine.generated.h"

/**
 * @class UACFAIStateRoutine
 *
 * AIState.Routine: the agent runs the tasks of its daily routine. The tasks are driven by the routine component.
 *
 * It has no behavior of its own yet: entering and leaving it does nothing. It exists so that this state has
 * a class of its own, which is where its behavior goes the day it needs one, without touching any other
 * state or the state machine.
 */
UCLASS()
class AIFRAMEWORK_API UACFAIStateRoutine : public UACFAIState {
    GENERATED_BODY()
};
