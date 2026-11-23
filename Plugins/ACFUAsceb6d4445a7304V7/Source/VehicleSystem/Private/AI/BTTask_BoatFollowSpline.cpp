// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "AI/BTTask_BoatFollowSpline.h"
#include "ACFSplinePath.h"
#include "ACFWaterVehicleComponent.h"
#include "AIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "Components/ACFAIPatrolComponent.h"
#include "Components/SplineComponent.h"
#include "GameFramework/Pawn.h"

namespace
{
const USplineComponent* GetPatrolSpline(const APawn* pawn)
{
    const UACFAIPatrolComponent* patrolComp = pawn ? pawn->FindComponentByClass<UACFAIPatrolComponent>() : nullptr;
    const AACFSplinePath* path = patrolComp ? patrolComp->GetPathToFollow() : nullptr;
    return path ? path->GetSplineComponent() : nullptr;
}
}

UBTTask_BoatFollowSpline::UBTTask_BoatFollowSpline()
{
    NodeName = TEXT("Boat Follow Spline");
    bNotifyTick = true;
}

EBTNodeResult::Type UBTTask_BoatFollowSpline::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    const AAIController* aiController = OwnerComp.GetAIOwner();
    const APawn* pawn = aiController ? aiController->GetPawn() : nullptr;
    const USplineComponent* spline = GetPatrolSpline(pawn);
    if (!spline || spline->GetNumberOfSplinePoints() == 0 || !GetWaterVehicle(OwnerComp)) {
        return EBTNodeResult::Failed;
    }

    // Start from the closest spline point so the boat doesn't cross the whole loop to reach point 0
    FBTBoatFollowSplineMemory* memory = reinterpret_cast<FBTBoatFollowSplineMemory*>(NodeMemory);
    const FVector pawnLocation = pawn->GetActorLocation();
    float bestDistSq = TNumericLimits<float>::Max();
    memory->PointIndex = 0;
    for (int32 i = 0; i < spline->GetNumberOfSplinePoints(); ++i) {
        const float distSq = FVector::DistSquared2D(pawnLocation, spline->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::World));
        if (distSq < bestDistSq) {
            bestDistSq = distSq;
            memory->PointIndex = i;
        }
    }

    return EBTNodeResult::InProgress;
}

void UBTTask_BoatFollowSpline::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
    const AAIController* aiController = OwnerComp.GetAIOwner();
    const APawn* pawn = aiController ? aiController->GetPawn() : nullptr;
    const USplineComponent* spline = GetPatrolSpline(pawn);
    UACFWaterVehicleComponent* waterVehicle = GetWaterVehicle(OwnerComp);
    if (!spline || spline->GetNumberOfSplinePoints() == 0 || !waterVehicle) {
        FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
        return;
    }

    FBTBoatFollowSplineMemory* memory = reinterpret_cast<FBTBoatFollowSplineMemory*>(NodeMemory);
    memory->PointIndex = memory->PointIndex % spline->GetNumberOfSplinePoints();

    const FVector targetPoint = spline->GetLocationAtSplinePoint(memory->PointIndex, ESplineCoordinateSpace::World);
    if (FVector::Dist2D(pawn->GetActorLocation(), targetPoint) <= PointAcceptRadius) {
        memory->PointIndex = (memory->PointIndex + 1) % spline->GetNumberOfSplinePoints();
        return;
    }

    SteerTowards(waterVehicle, GetYawErrorTo(pawn, targetPoint), CruiseThrust);
}
