// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "AI/BTTask_BoatBase.h"
#include "ACFWaterVehicleComponent.h"
#include "AIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/Pawn.h"

UACFWaterVehicleComponent* UBTTask_BoatBase::GetWaterVehicle(UBehaviorTreeComponent& OwnerComp) const
{
    const AAIController* aiController = OwnerComp.GetAIOwner();
    const APawn* pawn = aiController ? aiController->GetPawn() : nullptr;
    return pawn ? pawn->FindComponentByClass<UACFWaterVehicleComponent>() : nullptr;
}

float UBTTask_BoatBase::GetYawErrorTo(const APawn* pawn, const FVector& worldTarget)
{
    return GetYawErrorToDirection(pawn, worldTarget - pawn->GetActorLocation());
}

float UBTTask_BoatBase::GetYawErrorToDirection(const APawn* pawn, const FVector& worldDirection)
{
    FVector toDir(worldDirection.X, worldDirection.Y, 0.f);
    if (!toDir.Normalize()) {
        return 0.f;
    }

    const FVector fwdRaw = pawn->GetActorForwardVector();
    const FVector fwd = FVector(fwdRaw.X, fwdRaw.Y, 0.f).GetSafeNormal();
    return FMath::RadiansToDegrees(FMath::Atan2(FVector::CrossProduct(fwd, toDir).Z, FVector::DotProduct(fwd, toDir)));
}

void UBTTask_BoatBase::SteerTowards(UACFWaterVehicleComponent* waterVehicle, float yawErrorDeg, float thrustScale) const
{
    if (!waterVehicle) {
        return;
    }

    const float steer = FMath::Clamp(yawErrorDeg / SteerSaturationAngle, -1.f, 1.f);
    waterVehicle->AddSteering(steer);

    const float thrust = FMath::Clamp(1.f - FMath::Abs(yawErrorDeg) / ThrustCutoffAngle, 0.f, 1.f) * thrustScale;
    if (thrust > KINDA_SMALL_NUMBER) {
        waterVehicle->AddThrust(thrust);
    }

    if (bDrawDebug) {
        const AActor* owner = waterVehicle->GetOwner();
        DrawDebugString(owner->GetWorld(), owner->GetActorLocation() + FVector(0.f, 0.f, 300.f),
            FString::Printf(TEXT("YawErr %.0f Steer %.2f Thrust %.2f"), yawErrorDeg, steer, thrust),
            nullptr, FColor::Yellow, 0.f, true);
    }
}
