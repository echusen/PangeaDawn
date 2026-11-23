// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "Actions/ACFTopDownShootAction.h"
#include "Components/ACFShootingComponent.h"
#include "ItemActors/ACFRangedWeaponActor.h"
#include "Items/ACFRangedWeapon.h"
#include <Engine/World.h>
#include <GameFramework/Pawn.h>

bool UACFTopDownShootAction::GetTopDownSwipeResult(FHitResult& OutResult) const
{
    OutResult = TopDownShootResult;
    return bTopDownSwipeSuccess;
}

void UACFTopDownShootAction::OnNotablePointReached_Implementation()
{
    // The parent implementation is deliberately skipped: that is the camera based shot this action
    // replaces. The grandparent is called explicitly so the generic notable point behaviour of the
    // action ability is kept.
    UACFActionAbility::OnNotablePointReached_Implementation();

    bTopDownSwipeSuccess = false;
    TopDownShootResult = FHitResult();

    AACFRangedWeaponActor* RangedWeapon = GetRangedWeapon();
    if (!RangedWeapon) {
        return;
    }

    UACFShootingComponent* ShootingComponent = RangedWeapon->GetShootingComponent();
    if (!ShootingComponent) {
        return;
    }

    // Both the muzzle position and the shot itself read the shooting mesh, which is only set up once
    // the weapon has an owner.
    if (!ShootingComponent->GetShootingMesh()) {
        return;
    }

    const UACFRangedWeapon* WeaponDefinition = RangedWeapon->GetRangedWeaponDefinition();
    if (!WeaponDefinition) {
        return;
    }

    const APawn* SourcePawn = Cast<APawn>(GetAvatarActorFromActorInfo());
    if (!SourcePawn) {
        return;
    }

    FVector AimPoint;
    if (!GetAimPoint(SourcePawn, AimPoint)) {
        return;
    }

    // Shooting from the muzzle to the aim point instead of along the aim rotation removes the
    // parallax between where the player aims and where the projectile starts.
    const FVector MuzzleLocation = ShootingComponent->GetShootingSocketPosition();
    const FVector MuzzleToAim = AimPoint - MuzzleLocation;
    if (MuzzleToAim.IsNearlyZero()) {
        return;
    }

    const FRotator ShootRotation = MuzzleToAim.Rotation();

    switch (WeaponDefinition->GetShootingType()) {
    case EShootingType::ESwipeTrace:
        bTopDownSwipeSuccess = ShootingComponent->SwipeTraceShootAtDirection(MuzzleLocation, ShootRotation.Vector(), TopDownShootResult, WeaponDefinition->ShootRadius);
        break;
    case EShootingType::EProjectile:
        RangedWeapon->ShootAtDirection(ShootRotation, 1.f, ProjectileOverride);
        break;
    }
}

bool UACFTopDownShootAction::GetAimPoint(const APawn* SourcePawn, FVector& OutAimPoint) const
{
    OutAimPoint = FVector::ZeroVector;

    if (!SourcePawn) {
        return false;
    }

    UWorld* World = SourcePawn->GetWorld();
    if (!World) {
        return false;
    }

    // An unpossessed pawn has no control rotation to aim with.
    if (!SourcePawn->GetController()) {
        return false;
    }

    FVector EyeLocation;
    FRotator EyeRotation;
    SourcePawn->GetActorEyesViewPoint(EyeLocation, EyeRotation);

    const FRotator AimRotation = SourcePawn->GetControlRotation() + DeltaDirection;
    const FVector TraceEnd = EyeLocation + (AimRotation.Vector() * AimTraceDistance);

    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ACFTopDownShootAim), false, SourcePawn);

    // The weapon itself sits right in front of the eyes, so it must never block its own aim.
    const AACFRangedWeaponActor* RangedWeapon = GetRangedWeapon();
    if (RangedWeapon) {
        QueryParams.AddIgnoredActor(RangedWeapon);
    }

    FHitResult Hit;
    if (World->LineTraceSingleByChannel(Hit, EyeLocation, TraceEnd, AimTraceChannel, QueryParams)) {
        OutAimPoint = Hit.ImpactPoint;
        return true;
    }

    // Nothing in the way: the player is aiming at the sky or past the level, so the end of the trace
    // is a good enough aim point and the shot still goes where the mouse points.
    OutAimPoint = TraceEnd;
    return true;
}
