// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "AI/BTTask_BoatBase.h"
#include "CoreMinimal.h"

#include "BTTask_BoatBroadsidePosition.generated.h"

struct FBTBoatBroadsidePositionMemory {
    // -1 = target kept to port, +1 = starboard, 0 = not chosen yet
    float SideSign;
    float Elapsed;
};

/**
 * Turns the hull so the target sits abeam (bearing +/- 90 degrees) inside the firing range band.
 * Succeeds when the target is within the broadside firing arc and the range band; fails on
 * timeout so the tree can fall back to a new approach.
 */
UCLASS()
class VEHICLESYSTEM_API UBTTask_BoatBroadsidePosition : public UBTTask_BoatBase {
    GENERATED_BODY()

public:
    UBTTask_BoatBroadsidePosition();

    virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
    virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
    virtual void InitializeFromAsset(UBehaviorTree& Asset) override;
    virtual uint16 GetInstanceMemorySize() const override { return sizeof(FBTBoatBroadsidePositionMemory); }

protected:
    /*Blackboard key holding the target actor*/
    UPROPERTY(EditAnywhere, Category = ACF)
    FBlackboardKeySelector TargetKey;

    /*Minimum firing distance (2D)*/
    UPROPERTY(EditAnywhere, Category = ACF, meta = (ClampMin = "0"))
    float MinRange = 1500.f;

    /*Maximum firing distance (2D)*/
    UPROPERTY(EditAnywhere, Category = ACF, meta = (ClampMin = "0"))
    float MaxRange = 4000.f;

    /*Half angle (degrees) of the broadside firing arc around the beam direction*/
    UPROPERTY(EditAnywhere, Category = ACF, meta = (ClampMin = "1", ClampMax = "60"))
    float FiringArcHalfAngle = 15.f;

    /*Degrees added/removed from the 90-degree beam heading to drift back inside the range band*/
    UPROPERTY(EditAnywhere, Category = ACF, meta = (ClampMin = "0", ClampMax = "60"))
    float RangeBiasAngle = 20.f;

    /*Thrust scale while positioning*/
    UPROPERTY(EditAnywhere, Category = ACF, meta = (ClampMin = "0", ClampMax = "1"))
    float PositionThrust = 0.5f;

    /*Seconds before giving up and letting the tree re-approach*/
    UPROPERTY(EditAnywhere, Category = ACF, meta = (ClampMin = "1"))
    float TimeOut = 12.f;
};
