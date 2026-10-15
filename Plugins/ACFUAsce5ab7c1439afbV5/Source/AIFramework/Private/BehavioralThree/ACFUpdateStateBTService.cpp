// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "BehavioralThree/ACFUpdateStateBTService.h"
#include "ACFAIController.h"
#include "Components/ACFCommandsManagerComponent.h"
#include <BehaviorTree/BehaviorTreeComponent.h>
#include "Game/ACFFunctionLibrary.h"
#include <Logging.h>
#include <GameplayTagsManager.h>

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

    if (aiController->IsPartOfGroup()) {
        UACFGroupAIComponent* group = aiController->GetGroup();
        const AActor* lead = aiController->GetLeadActorBK();
        if (lead) {
            const float distanceFromLeader = (CharOwner->GetActorLocation() - lead->GetActorLocation()).Size();
            aiController->SetLeadActorDistanceBK(distanceFromLeader);

            // While the enemy group still has an alive agent the AI must stay in battle:
            // it can be teleported back near its lead, but it must never drop the fight
            const bool bMustStayInBattle = aiController->ShouldStayInBattle();

            if (aiController->ShouldTeleportNearLead() && distanceFromLeader >= aiController->GetTeleportLeadDistanceTrigger()) {
                aiController->TeleportNearLead();
                if (!bMustStayInBattle) {
                    aiController->SetTarget(nullptr);
                }
                return;
            } else if (!bMustStayInBattle && distanceFromLeader > aiController->GetMaxDistanceFromHome() &&
                aiController->GetDefaultAIState() == UGameplayTagsManager::Get().RequestGameplayTag(ACF::AIFollowLead)) {
                aiController->SetCurrentAIState(UGameplayTagsManager::Get().RequestGameplayTag(ACF::AIFollowLead));
                return;
            }
        } else {
            aiController->TryUpdateLeadRef();
        }
    } else {
        if (aiController->ShouldReturnHome() && aiController->GetAIState() !=  UGameplayTagsManager::Get().RequestGameplayTag(ACF::AIReturnHome)) {
            const float distanceFromHome = (CharOwner->GetActorLocation() - aiController->GetHomeLocation()).Size();
            aiController->SetHomeDistanceBK(distanceFromHome);
            if (distanceFromHome > aiController->GetMaxDistanceFromHome()) {
                aiController->SetTarget(nullptr);
                aiController->SetCurrentAIState(UGameplayTagsManager::Get().RequestGameplayTag(ACF::AIReturnHome));
                return;
            }
        }
    }
}
