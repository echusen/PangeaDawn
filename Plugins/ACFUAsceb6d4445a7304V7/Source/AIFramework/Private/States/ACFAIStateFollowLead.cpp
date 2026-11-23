// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "States/ACFAIStateFollowLead.h"
#include "ACFAIController.h"

void UACFAIStateFollowLead::OnStateEnter_Implementation(FGameplayTag previousState)
{
    Super::OnStateEnter_Implementation(previousState);

    if (AACFAIController* controller = GetAIController()) {
        controller->SetFocus(controller->GetLeadActorBK(), EAIFocusPriority::Gameplay);
    }
}
