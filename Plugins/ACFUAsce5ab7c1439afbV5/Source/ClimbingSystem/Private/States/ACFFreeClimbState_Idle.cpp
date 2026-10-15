// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "ACFFreeClimbState_Idle.h"
#include "ACFFreeClimbComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

void FACFFreeClimbState_Idle::FillRepState(FACFFreeClimbRepState& State) const
{
    State.bExitToWalking = Owner.bPendingExitToWalking;
    State.DropVelocity = Owner.PendingDropVelocity;
}

void FACFFreeClimbState_Idle::ApplyRemoteState(const FACFFreeClimbRepState& State)
{
    if (State.bExitToWalking)
    {
        const FRotator Rotation = Owner.OwnerCharacter->GetActorRotation();
        Owner.OwnerCharacter->SetActorRotation(FRotator(0.0f, Rotation.Yaw, 0.0f));

        Owner.MovementComponent->SetMovementMode(MOVE_Walking);
        return;
    }

    Owner.MovementComponent->SetMovementMode(MOVE_Falling);

    // A drop releases the capsule with a push off the wall. The climb left it stationary, so without
    // replaying that velocity here the authority would start a straight fall and correct the owning
    // client back into the surface.
    if (!State.DropVelocity.IsNearlyZero())
    {
        Owner.MovementComponent->Velocity = State.DropVelocity;
    }
}
