// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "Selectors/ACFAIDefaultStateSelector.h"
#include "ACFAIController.h"
#include "Components/ACFAIStateManagerComponent.h"
#include <GameFramework/Pawn.h>

FGameplayTag UACFAIDefaultStateSelector::SelectState_Implementation(float deltaTime)
{
    // IsPartOfGroup comes from the group agent interface, so the controller cannot be const here
    AACFAIController* controller = GetAIController();
    if (!controller || !controller->GetPawn()) {
        return FGameplayTag();
    }

    /* The leash decides first: it is the rule that can take an agent out of whatever it is doing. It
    answers with an invalid tag when it has nothing to say, and it never answers while the agent is
    fighting someone */
    const FGameplayTag leashState = controller->IsPartOfGroup() ? SelectLeadLeashState() : SelectHomeLeashState();
    if (leashState.IsValid()) {
        return leashState;
    }

    /* Nothing is pulling the agent back: if it has someone it can actually fight, it fights. Holding a
    target is not enough: a dead one, or one that is not an enemy, must never drag an agent into a fight,
    which is what would pull a friendly NPC out of whatever it was doing */
    return controller->HasValidEnemyTarget() ? CombatState : FGameplayTag();
}

FGameplayTag UACFAIDefaultStateSelector::SelectLeadLeashState()
{
    AACFAIController* controller = GetAIController();
    const APawn* charOwner = controller ? controller->GetPawn() : nullptr;
    if (!charOwner) {
        return FGameplayTag();
    }

    const AActor* lead = controller->GetLeadActorBK();
    if (!lead) {
        controller->TryUpdateLeadRef();
        return FGameplayTag();
    }

    const float distanceFromLeader = (charOwner->GetActorLocation() - lead->GetActorLocation()).Size();
    controller->SetLeadActorDistanceBK(distanceFromLeader);

    // While the enemy group still has an alive agent the AI must stay in battle:
    // it can be teleported back near its lead, but it must never drop the fight
    const bool bMustStayInBattle = controller->ShouldStayInBattle();

    if (controller->ShouldTeleportNearLead() && distanceFromLeader >= controller->GetTeleportLeadDistanceTrigger()) {
        controller->TeleportNearLead();
        if (!bMustStayInBattle) {
            controller->SetTarget(nullptr);
        }
        return FGameplayTag();
    }

    if (!bMustStayInBattle && distanceFromLeader > controller->GetMaxDistanceFromHome()
        && controller->GetDefaultAIState() == FollowLeadState) {
        return FollowLeadState;
    }

    return FGameplayTag();
}

/* This is a hard cap on how far an agent can get from its home, and it holds while it is fighting too:
an agent disengages when its target goes over LoseTargetDistance OR when it goes over this distance from
home, whichever comes first. Going home drops the target, which UACFAIStateReturnHome does on enter */
FGameplayTag UACFAIDefaultStateSelector::SelectHomeLeashState()
{
    AACFAIController* controller = GetAIController();
    const APawn* charOwner = controller ? controller->GetPawn() : nullptr;
    if (!charOwner || !controller->ShouldReturnHome()) {
        return FGameplayTag();
    }

    // The agent is already on its way home: nothing to decide
    const UACFAIStateManagerComponent* machine = GetStateMachine();
    if (machine && machine->GetCurrentAIState() == ReturnHomeState) {
        return FGameplayTag();
    }

    const float distanceFromHome = (charOwner->GetActorLocation() - controller->GetHomeLocation()).Size();
    controller->SetHomeDistanceBK(distanceFromHome);

    return distanceFromHome > controller->GetMaxDistanceFromHome() ? ReturnHomeState : FGameplayTag();
}
