// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "States/ACFAIState.h"
#include "ACFAIController.h"
#include "Actors/ACFCharacter.h"

AACFCharacter* UACFAIState::GetControlledCharacter() const
{
    return aiController ? aiController->GetBaseAICharacter() : nullptr;
}

UWorld* UACFAIState::GetWorld() const
{
    return aiController ? aiController->GetWorld() : nullptr;
}

void UACFAIState::InternalInitialize(UACFAIStateManagerComponent* machine, AACFAIController* controller, const FGameplayTag& inStateTag)
{
    stateMachine = machine;
    aiController = controller;
    stateTag = inStateTag;
}

void UACFAIState::OnStateEnter_Implementation(FGameplayTag previousState)
{
}

void UACFAIState::OnStateExit_Implementation(FGameplayTag nextState)
{
}

void UACFAIState::OnStateTick_Implementation(float deltaTime)
{
}

void UACFAIState::OnHomeLocationChanged_Implementation(const FVector& newHomeLocation)
{
}

bool UACFAIState::IsBattleState_Implementation() const
{
    return false;
}
