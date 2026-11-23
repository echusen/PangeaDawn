// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "AI/BTTask_BoatBroadsideFire.h"
#include "AIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "Components/ACFAbilitySystemComponent.h"
#include "GameFramework/Pawn.h"

UBTTask_BoatBroadsideFire::UBTTask_BoatBroadsideFire()
{
    NodeName = TEXT("Boat Broadside Fire (Ability)");
}

EBTNodeResult::Type UBTTask_BoatBroadsideFire::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    const AAIController* aiController = OwnerComp.GetAIOwner();
    APawn* pawn = aiController ? aiController->GetPawn() : nullptr;
    UACFAbilitySystemComponent* abilityComp = pawn ? pawn->FindComponentByClass<UACFAbilitySystemComponent>() : nullptr;
    if (!abilityComp || !FireAbilityTag.IsValid()) {
        return EBTNodeResult::Failed;
    }

    return abilityComp->TriggerAction(FireAbilityTag, Priority) ? EBTNodeResult::Succeeded : EBTNodeResult::Failed;
}
