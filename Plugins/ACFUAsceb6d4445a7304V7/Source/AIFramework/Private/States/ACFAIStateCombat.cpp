// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "States/ACFAIStateCombat.h"
#include "ACFAIController.h"
#include "Components/ACFCombatBehaviourComponent.h"

void UACFAIStateCombat::OnStateEnter_Implementation(FGameplayTag previousState)
{
    Super::OnStateEnter_Implementation(previousState);

    AACFAIController* controller = GetAIController();
    if (!controller || !controller->GetCombatBehavior()) {
        return;
    }

    controller->GetCombatBehavior()->InitBehavior(controller);

    /* A path request belongs to the state that issued it, and the behavior tree branch of that state is
    aborted by this very state change while its pending move is not. Combat is where it matters: without
    this the pawn keeps walking to the old destination, typically all the way back home, while it is
    already fighting */
    controller->StopMovement();
}

void UACFAIStateCombat::OnStateExit_Implementation(FGameplayTag nextState)
{
    Super::OnStateExit_Implementation(nextState);

    AACFAIController* controller = GetAIController();
    if (controller && controller->GetCombatBehavior() && controller->GetPerceptionComponent()) {
        controller->GetCombatBehavior()->UninitBehavior();
    }
}

bool UACFAIStateCombat::IsBattleState_Implementation() const
{
    return true;
}
