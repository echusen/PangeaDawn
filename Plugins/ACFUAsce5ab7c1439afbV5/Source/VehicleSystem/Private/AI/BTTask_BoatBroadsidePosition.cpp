// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "AI/BTTask_BoatBroadsidePosition.h"
#include "ACFWaterVehicleComponent.h"
#include "AIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "GameFramework/Pawn.h"

UBTTask_BoatBroadsidePosition::UBTTask_BoatBroadsidePosition()
{
    NodeName = TEXT("Boat Broadside Position");
    bNotifyTick = true;
    TargetKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_BoatBroadsidePosition, TargetKey), AActor::StaticClass());
}

void UBTTask_BoatBroadsidePosition::InitializeFromAsset(UBehaviorTree& Asset)
{
    Super::InitializeFromAsset(Asset);
    if (UBlackboardData* bbAsset = GetBlackboardAsset()) {
        TargetKey.ResolveSelectedKey(*bbAsset);
    }
}

EBTNodeResult::Type UBTTask_BoatBroadsidePosition::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    FBTBoatBroadsidePositionMemory* memory = reinterpret_cast<FBTBoatBroadsidePositionMemory*>(NodeMemory);
    memory->SideSign = 0.f;
    memory->Elapsed = 0.f;
    return EBTNodeResult::InProgress;
}

void UBTTask_BoatBroadsidePosition::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
    const AAIController* aiController = OwnerComp.GetAIOwner();
    const APawn* pawn = aiController ? aiController->GetPawn() : nullptr;
    const UBlackboardComponent* blackboard = OwnerComp.GetBlackboardComponent();
    const AActor* target = blackboard ? Cast<AActor>(blackboard->GetValueAsObject(TargetKey.SelectedKeyName)) : nullptr;
    UACFWaterVehicleComponent* waterVehicle = GetWaterVehicle(OwnerComp);
    if (!pawn || !target || !waterVehicle) {
        FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
        return;
    }

    FBTBoatBroadsidePositionMemory* memory = reinterpret_cast<FBTBoatBroadsidePositionMemory*>(NodeMemory);
    memory->Elapsed += DeltaSeconds;
    if (memory->Elapsed > TimeOut) {
        FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
        return;
    }

    FVector toTarget = target->GetActorLocation() - pawn->GetActorLocation();
    toTarget.Z = 0.f;
    const float distance = toTarget.Size();
    if (!toTarget.Normalize()) {
        return;
    }

    // Pick once the side requiring the smaller turn and stick to it
    if (memory->SideSign == 0.f) {
        const FVector portHeading = toTarget.RotateAngleAxis(-90.f, FVector::UpVector);
        const FVector starboardHeading = toTarget.RotateAngleAxis(90.f, FVector::UpVector);
        const float portError = FMath::Abs(GetYawErrorToDirection(pawn, portHeading));
        const float starboardError = FMath::Abs(GetYawErrorToDirection(pawn, starboardHeading));
        memory->SideSign = portError <= starboardError ? -1.f : 1.f;
    }

    // Beam heading, biased inward/outward when outside the range band
    float beamAngle = 90.f;
    if (distance > MaxRange) {
        beamAngle -= RangeBiasAngle;
    } else if (distance < MinRange) {
        beamAngle += RangeBiasAngle;
    }
    const FVector desiredHeading = toTarget.RotateAngleAxis(memory->SideSign * beamAngle, FVector::UpVector);
    SteerTowards(waterVehicle, GetYawErrorToDirection(pawn, desiredHeading), PositionThrust);

    const FVector rightRaw = pawn->GetActorRightVector();
    const FVector right = FVector(rightRaw.X, rightRaw.Y, 0.f).GetSafeNormal();
    const bool bInArc = FMath::Abs(FVector::DotProduct(right, toTarget)) >= FMath::Cos(FMath::DegreesToRadians(FiringArcHalfAngle));
    if (bInArc && distance >= MinRange && distance <= MaxRange) {
        FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
    }
}
