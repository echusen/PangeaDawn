// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "AI/BTTask_BoatBase.h"
#include "CoreMinimal.h"

#include "BTTask_BoatFollowSpline.generated.h"

struct FBTBoatFollowSplineMemory {
    int32 PointIndex;
};

/**
 * Default patrol behavior: endlessly loops the boat along the AACFSplinePath assigned to the
 * pawn's UACFAIPatrolComponent (used purely as a data holder — no NavMesh involved).
 * Never succeeds on its own; it is meant to be aborted by the combat branch decorator.
 */
UCLASS()
class VEHICLESYSTEM_API UBTTask_BoatFollowSpline : public UBTTask_BoatBase {
    GENERATED_BODY()

public:
    UBTTask_BoatFollowSpline();

    virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
    virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
    virtual uint16 GetInstanceMemorySize() const override { return sizeof(FBTBoatFollowSplineMemory); }

protected:
    /*Distance (2D) at which a spline point is considered reached*/
    UPROPERTY(EditAnywhere, Category = ACF, meta = (ClampMin = "100"))
    float PointAcceptRadius = 800.f;

    /*Thrust scale while patrolling*/
    UPROPERTY(EditAnywhere, Category = ACF, meta = (ClampMin = "0", ClampMax = "1"))
    float CruiseThrust = 0.6f;
};
