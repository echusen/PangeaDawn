// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "Actions/ACFSetCharacterGravityAction.h"

#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

void UACFSetCharacterGravityAction::ExecuteAction_Implementation(APlayerController* playerController, UAGSGraphNode* nodeOwner)
{
    Super::ExecuteAction_Implementation(playerController, nodeOwner);
    ApplyCharacterGravity();
}

bool UACFSetCharacterGravityAction::ApplyCharacterGravity()
{
    ACharacter* Character = TargetCharacter.Get();
    if (!IsValid(Character)) {
        UE_LOG(LogTemp, Warning, TEXT("ACF Set Character Gravity: target is not loaded: %s"),
            *TargetCharacter.ToSoftObjectPath().ToString());
        return false;
    }

    // AI movement is server-authoritative. This is not a replicated gravity setting
    // for client-predicted player characters, nor a change to skeletal mesh physics.
    if (!Character->HasAuthority()) {
        return false;
    }

    UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
    if (!IsValid(Movement)) {
        UE_LOG(LogTemp, Warning, TEXT("ACF Set Character Gravity: %s has no CharacterMovement."), *GetNameSafe(Character));
        return false;
    }

    if (bEnableGravity && (!FMath::IsFinite(EnabledGravityScale) || EnabledGravityScale < 0.0f)) {
        UE_LOG(LogTemp, Warning, TEXT("ACF Set Character Gravity: enabled gravity scale must be finite and non-negative."));
        return false;
    }

    Movement->GravityScale = bEnableGravity ? EnabledGravityScale : 0.0f;
    if (!bEnableGravity) {
        // Zero acceleration alone would leave an already falling character falling.
        // Preserve lateral motion and support custom gravity directions.
        Movement->Velocity = Movement->ProjectToGravityFloor(Movement->Velocity);
        Movement->UpdateComponentVelocity();
    }

    Character->ForceNetUpdate();
    return true;
}
