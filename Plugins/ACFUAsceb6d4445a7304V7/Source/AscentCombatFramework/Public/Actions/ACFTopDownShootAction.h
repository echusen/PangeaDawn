// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "Actions/ACFShootAction.h"
#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "Engine/HitResult.h"

#include "ACFTopDownShootAction.generated.h"

/**
 * Shoot ability for top down games. The parent shoots along the owner view point, which in top down
 * is the fixed camera looking at the ground, so the shot never goes where the mouse is.
 * This one shoots from the muzzle towards the point the control rotation is aiming at, with no
 * parallax between the aim and the projectile: pair it with the cursor aiming of
 * UACFTopDownControllerComponent, which keeps the control rotation pointing at the cursor.
 */
UCLASS()
class ASCENTCOMBATFRAMEWORK_API UACFTopDownShootAction : public UACFShootAction {
    GENERATED_BODY()

public:
    /** Result of the last swipe trace shot fired by this action. Returns false when the trace missed
     * or when the weapon shoots projectiles instead of tracing. */
    UFUNCTION(BlueprintPure, Category = ACF)
    bool GetTopDownSwipeResult(FHitResult& OutResult) const;

protected:
    /** Length of the trace used to find the aim point along the control rotation. The end of the
     * trace is used as aim point when nothing is hit, so the shot always has a direction. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = ACF, meta = (ClampMin = "0.0"))
    float AimTraceDistance = 10000.f;

    /** Collision channel of the aim trace. Keep it on the channel the cursor trace of the top down
     * component uses, so the aim point and the cursor agree on what blocks the aim. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = ACF)
    TEnumAsByte<ECollisionChannel> AimTraceChannel = ECollisionChannel::ECC_Visibility;

    virtual void OnNotablePointReached_Implementation() override;

private:
    /** Traces along the control rotation from the pawn eyes and outputs the point being aimed at.
     * Runs the same way on the server and on the clients: no cursor and no local only data. */
    bool GetAimPoint(const APawn* SourcePawn, FVector& OutAimPoint) const;

    FHitResult TopDownShootResult;

    bool bTopDownSwipeSuccess = false;
};
