// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "BehavioralThree/ACFUpdateStateBTService.h"
#include "ACFAIController.h"
#include "Components/ACFAIStateManagerComponent.h"
#include "Components/ACFCommandsManagerComponent.h"
#include <BehaviorTree/BehaviorTreeComponent.h>
#include "Game/ACFFunctionLibrary.h"
#include <Logging.h>

void UACFUpdateStateBTService::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{

    aiController = Cast<AACFAIController>(OwnerComp.GetAIOwner());
    if (aiController == nullptr) {
        UE_LOG(ACFAILog, Warning, TEXT("UACFUpdateCombatBTService get controller failed"));
        return;
    }

    CharOwner = aiController->GetPawn();
    if (CharOwner == nullptr) {
        UE_LOG(ACFAILog, Error, TEXT("GetPawn() is NULL - UACFUpdateCombatBTService"));
        aiController->Destroy();
        return;
    }

    // The state machine picks the state the agent should be in and ticks it: see UACFAIStateManagerComponent
    if (UACFAIStateManagerComponent* stateManager = aiController->GetStateManager()) {
        stateManager->TickStateMachine(DeltaSeconds);
    }
}
