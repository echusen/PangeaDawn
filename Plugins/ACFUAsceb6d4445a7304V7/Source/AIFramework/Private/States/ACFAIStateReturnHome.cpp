// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "States/ACFAIStateReturnHome.h"
#include "ACFAIController.h"

void UACFAIStateReturnHome::OnStateEnter_Implementation(FGameplayTag previousState)
{
    Super::OnStateEnter_Implementation(previousState);

    AACFAIController* controller = GetAIController();
    if (!controller) {
        return;
    }

    /* Only when there is one: clearing a target that is already null sends the controller back to its
    default state, which would undo this very transition */
    if (controller->GetTargetActorBK()) {
        controller->SetTarget(nullptr);
    }

    controller->SetTargetLocationBK(controller->GetHomeLocation());
}

void UACFAIStateReturnHome::OnHomeLocationChanged_Implementation(const FVector& newHomeLocation)
{
    Super::OnHomeLocationChanged_Implementation(newHomeLocation);

    // The agent is walking home right now: it has to walk to the new one
    if (AACFAIController* controller = GetAIController()) {
        controller->SetTargetLocationBK(newHomeLocation);
    }
}
