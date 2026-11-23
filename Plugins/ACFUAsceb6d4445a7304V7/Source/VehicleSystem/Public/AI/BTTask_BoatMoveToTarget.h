// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "AI/BTTask_BoatBase.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "CoreMinimal.h"

#include "BTTask_BoatMoveToTarget.generated.h"

/**
 * Steers the boat toward the blackboard target actor. Succeeds when within AcceptRadius (2D),
 * fails if the target becomes invalid.
 */
UCLASS()
class VEHICLESYSTEM_API UBTTask_BoatMoveToTarget : public UBTTask_BoatBase {
    GENERATED_BODY()

public:
    UBTTask_BoatMoveToTarget();

    virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
    virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
    virtual void InitializeFromAsset(UBehaviorTree& Asset) override;

protected:
    /*Blackboard key holding the target actor*/
    UPROPERTY(EditAnywhere, Category = ACF)
    FBlackboardKeySelector TargetKey;

    /*Distance (2D) at which the approach is considered complete*/
    UPROPERTY(EditAnywhere, Category = ACF, meta = (ClampMin = "100"))
    float AcceptRadius = 3500.f;

    /*Thrust scale while approaching*/
    UPROPERTY(EditAnywhere, Category = ACF, meta = (ClampMin = "0", ClampMax = "1"))
    float ApproachThrust = 1.f;

private:
    AActor* GetTargetActor(UBehaviorTreeComponent& OwnerComp) const;
};
