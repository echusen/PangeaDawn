// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "AI/BTService_BoatUpdateTarget.h"
#include "AI/ACFBoatAIController.h"
#include "AIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Float.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "Components/ACFThreatManagerComponent.h"
#include "GameFramework/Pawn.h"
#include "Interfaces/ACFEntityInterface.h"

UBTService_BoatUpdateTarget::UBTService_BoatUpdateTarget()
{
    NodeName = TEXT("Boat Update Target");
    Interval = 0.5f;
    RandomDeviation = 0.1f;

    TargetKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UBTService_BoatUpdateTarget, TargetKey), AActor::StaticClass());
    TargetDistanceKey.AddFloatFilter(this, GET_MEMBER_NAME_CHECKED(UBTService_BoatUpdateTarget, TargetDistanceKey));
}

void UBTService_BoatUpdateTarget::InitializeFromAsset(UBehaviorTree& Asset)
{
    Super::InitializeFromAsset(Asset);
    if (UBlackboardData* bbAsset = GetBlackboardAsset()) {
        TargetKey.ResolveSelectedKey(*bbAsset);
        TargetDistanceKey.ResolveSelectedKey(*bbAsset);
    }
}

void UBTService_BoatUpdateTarget::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
    Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);

    AAIController* aiController = OwnerComp.GetAIOwner();
    const APawn* pawn = aiController ? aiController->GetPawn() : nullptr;
    UBlackboardComponent* blackboard = OwnerComp.GetBlackboardComponent();
    if (!pawn || !blackboard) {
        return;
    }

    AActor* target = Cast<AActor>(blackboard->GetValueAsObject(TargetKey.SelectedKeyName));
    if (!target) {
        return;
    }

    const float distance = FVector::Dist2D(pawn->GetActorLocation(), target->GetActorLocation());
    if (TargetDistanceKey.SelectedKeyName != NAME_None) {
        blackboard->SetValueAsFloat(TargetDistanceKey.SelectedKeyName, distance);
    }

    bool bTargetLost = distance > LoseTargetDistance;
    if (!bTargetLost && target->Implements<UACFEntityInterface>()) {
        bTargetLost = !IACFEntityInterface::Execute_IsEntityAlive(target);
    }

    if (bTargetLost) {
        // Through the controller when possible, so battle registration (music state) stays in sync
        if (AACFBoatAIController* boatController = Cast<AACFBoatAIController>(aiController)) {
            boatController->SetTargetActorBK(nullptr);
        } else {
            blackboard->ClearValue(TargetKey.SelectedKeyName);
        }
        if (UACFThreatManagerComponent* threatComp = aiController->FindComponentByClass<UACFThreatManagerComponent>()) {
            threatComp->RemoveThreatening(target);
        }
    }
}
