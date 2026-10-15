// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "AI/BTTask_BoatMoveToTarget.h"
#include "ACFWaterVehicleComponent.h"
#include "AIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "GameFramework/Pawn.h"

UBTTask_BoatMoveToTarget::UBTTask_BoatMoveToTarget()
{
    NodeName = TEXT("Boat Move To Target");
    bNotifyTick = true;
    TargetKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_BoatMoveToTarget, TargetKey), AActor::StaticClass());
}

void UBTTask_BoatMoveToTarget::InitializeFromAsset(UBehaviorTree& Asset)
{
    Super::InitializeFromAsset(Asset);
    if (UBlackboardData* bbAsset = GetBlackboardAsset()) {
        TargetKey.ResolveSelectedKey(*bbAsset);
    }
}

AActor* UBTTask_BoatMoveToTarget::GetTargetActor(UBehaviorTreeComponent& OwnerComp) const
{
    const UBlackboardComponent* blackboard = OwnerComp.GetBlackboardComponent();
    return blackboard ? Cast<AActor>(blackboard->GetValueAsObject(TargetKey.SelectedKeyName)) : nullptr;
}

EBTNodeResult::Type UBTTask_BoatMoveToTarget::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    if (!GetTargetActor(OwnerComp) || !GetWaterVehicle(OwnerComp)) {
        return EBTNodeResult::Failed;
    }
    return EBTNodeResult::InProgress;
}

void UBTTask_BoatMoveToTarget::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
    const AAIController* aiController = OwnerComp.GetAIOwner();
    const APawn* pawn = aiController ? aiController->GetPawn() : nullptr;
    const AActor* target = GetTargetActor(OwnerComp);
    UACFWaterVehicleComponent* waterVehicle = GetWaterVehicle(OwnerComp);
    if (!pawn || !target || !waterVehicle) {
        FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
        return;
    }

    if (FVector::Dist2D(pawn->GetActorLocation(), target->GetActorLocation()) <= AcceptRadius) {
        FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
        return;
    }

    SteerTowards(waterVehicle, GetYawErrorTo(pawn, target->GetActorLocation()), ApproachThrust);
}
