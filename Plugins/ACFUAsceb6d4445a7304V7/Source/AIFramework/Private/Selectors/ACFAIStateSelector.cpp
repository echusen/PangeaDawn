// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "Selectors/ACFAIStateSelector.h"
#include "ACFAIController.h"

UWorld* UACFAIStateSelector::GetWorld() const
{
    return aiController ? aiController->GetWorld() : nullptr;
}

void UACFAIStateSelector::InternalInitialize(UACFAIStateManagerComponent* machine, AACFAIController* controller)
{
    stateMachine = machine;
    aiController = controller;
}

FGameplayTag UACFAIStateSelector::SelectState_Implementation(float deltaTime)
{
    return FGameplayTag();
}
