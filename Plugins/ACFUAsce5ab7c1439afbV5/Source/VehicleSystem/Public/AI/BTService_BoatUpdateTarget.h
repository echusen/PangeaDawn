// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "BehaviorTree/BTService.h"
#include "CoreMinimal.h"

#include "BTService_BoatUpdateTarget.generated.h"

/**
 * Keeps the boat combat blackboard fresh: writes the target distance and clears the target
 * (also removing it from the threat manager) when it dies or drifts beyond LoseTargetDistance,
 * which sends the tree back to the patrol branch.
 */
UCLASS()
class VEHICLESYSTEM_API UBTService_BoatUpdateTarget : public UBTService {
    GENERATED_BODY()

public:
    UBTService_BoatUpdateTarget();

    virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
    virtual void InitializeFromAsset(UBehaviorTree& Asset) override;

protected:
    /*Blackboard key holding the target actor*/
    UPROPERTY(EditAnywhere, Category = ACF)
    FBlackboardKeySelector TargetKey;

    /*Blackboard float key receiving the 2D distance to the target*/
    UPROPERTY(EditAnywhere, Category = ACF)
    FBlackboardKeySelector TargetDistanceKey;

    /*Distance over which the target is dropped*/
    UPROPERTY(EditAnywhere, Category = ACF, meta = (ClampMin = "0"))
    float LoseTargetDistance = 15000.f;
};
