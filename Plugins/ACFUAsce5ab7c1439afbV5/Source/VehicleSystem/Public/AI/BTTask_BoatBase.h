// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "BehaviorTree/BTTaskNode.h"
#include "CoreMinimal.h"

#include "BTTask_BoatBase.generated.h"

class UACFWaterVehicleComponent;

/**
 * Shared steering math for physics-boat BT tasks. Movement is not NavMesh based:
 * every tick the tasks compute the signed heading error toward the desired direction
 * and convert it into AddSteering/AddThrust calls on the pawn's UACFWaterVehicleComponent.
 * Thrust falls off as the heading error grows to prevent orbiting around the goal.
 */
UCLASS(Abstract)
class VEHICLESYSTEM_API UBTTask_BoatBase : public UBTTaskNode {
    GENERATED_BODY()

protected:
    UACFWaterVehicleComponent* GetWaterVehicle(UBehaviorTreeComponent& OwnerComp) const;

    /*Signed yaw error in degrees between the pawn's flat forward and the direction to worldTarget. Positive = target on the right*/
    static float GetYawErrorTo(const APawn* pawn, const FVector& worldTarget);

    /*Signed yaw error in degrees between the pawn's flat forward and the provided world direction*/
    static float GetYawErrorToDirection(const APawn* pawn, const FVector& worldDirection);

    /*Steers toward the heading and applies thrust scaled down as the heading error grows (anti-orbiting)*/
    void SteerTowards(UACFWaterVehicleComponent* waterVehicle, float yawErrorDeg, float thrustScale = 1.f) const;

    /*Heading error (degrees) at which the steering input saturates to full*/
    UPROPERTY(EditAnywhere, Category = "ACF|Steering", meta = (ClampMin = "1"))
    float SteerSaturationAngle = 30.f;

    /*Heading error (degrees) at which thrust is fully cut. Thrust scales linearly down to it*/
    UPROPERTY(EditAnywhere, Category = "ACF|Steering", meta = (ClampMin = "1"))
    float ThrustCutoffAngle = 90.f;

    UPROPERTY(EditAnywhere, Category = "ACF|Steering")
    bool bDrawDebug = false;
};
