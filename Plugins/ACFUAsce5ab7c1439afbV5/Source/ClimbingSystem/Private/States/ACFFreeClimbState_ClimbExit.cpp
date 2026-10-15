// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "ACFFreeClimbState_ClimbExit.h"
#include "ClimbingSystem.h"
#include "ACFFreeClimbComponent.h"
#include "ACFWalkingComponent.h"
#include "ACFFreeClimbAnimInstance.h"
#include "ACFFreeClimbNetworkRuntime.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

void FACFFreeClimbState_ClimbExit::OnEnter()
{
    UE_LOG(LogACFFreeClimb, Log, TEXT("Start climb exit"));
    SavedCapsuleCollisionEnabled = Owner.CapsuleComponent->GetCollisionEnabled();

    UAnimInstance* AnimInstance = Owner.MeshComponent ? Owner.MeshComponent->GetAnimInstance() : nullptr;
    const bool bHasClimbAnimInstance = Owner.ACFFreeClimbAnimInstance && Owner.ACFFreeClimbAnimInstance->GetClimbExitMontage();
    const bool bHasAnimInstance = AnimInstance != nullptr;
    if (!bHasClimbAnimInstance || !bHasAnimInstance)
    {
        UE_LOG(
            LogACFFreeClimb,
            Warning,
            TEXT("Climb exit aborted for '%s': requires a valid AnimInstance, UACFFreeClimbAnimInstance, and ClimbExitMontage."),
            *GetNameSafe(Owner.OwnerCharacter));

        Owner.bPendingExitToWalking = false;
        Owner.TransitionTo(EACFFreeClimbState::Idle);
        return;
    }

    Owner.RemoteVisualMoveInput = FVector2D::ZeroVector;

    InitialRotation = Owner.OwnerCharacter->GetActorRotation().GetNormalized();
    InitialUprightDot = FMath::Clamp(FVector::DotProduct(Owner.OwnerCharacter->GetActorUpVector(), FVector::UpVector), 0.0f, 1.0f);
    RotationBlendElapsed = 0.0f;
    AppliedRise = 0.0f;

    Owner.MovementComponent->StopMovementImmediately();
    Owner.MovementComponent->SetMovementMode(MOVE_Flying);

    if (UACFWalkingComponent* WalkingComponent = Owner.OwnerCharacter->FindComponentByClass<UACFWalkingComponent>())
    {
        WalkingComponent->PrimeCollisionPatch(true);
    }

    if (Owner.UsesRemoteState())
    {
        return;
    }

    FVector AnchorNormal;
    bAnchorFollowActive = Owner.ResolveAnchorWorld(PreviousAnchorWorldPoint, AnchorNormal);

    bMontagePlaying = PlayMontage(0.0f);
    if (!bMontagePlaying)
    {
        UE_LOG(LogACFFreeClimb, Warning, TEXT("Climb exit montage failed to play."));
    }
}

void FACFFreeClimbState_ClimbExit::Tick(float DeltaTime)
{
    if (!bMontagePlaying)
    {
        Finish();
        return;
    }

    if (bAnchorFollowActive)
    {
        FVector CurrentAnchorWorldPoint;
        FVector CurrentAnchorWorldNormal;
        if (Owner.ResolveAnchorWorld(CurrentAnchorWorldPoint, CurrentAnchorWorldNormal))
        {
            FHitResult AnchorMoveHit;
            Owner.MovementComponent->SafeMoveUpdatedComponent(CurrentAnchorWorldPoint - PreviousAnchorWorldPoint, Owner.OwnerCharacter->GetActorQuat(), false, AnchorMoveHit);
            PreviousAnchorWorldPoint = CurrentAnchorWorldPoint;
        }
        else
        {
            bAnchorFollowActive = false;
        }
    }

    UpdateRotation(DeltaTime);
}

void FACFFreeClimbState_ClimbExit::ApplyRemoteState(const FACFFreeClimbRepState& State)
{
    bMontagePlaying = PlayMontage(0.0f);
}

bool FACFFreeClimbState_ClimbExit::PlayMontage(float StartPosition)
{
    UAnimInstance* AnimInstance = Owner.MeshComponent ? Owner.MeshComponent->GetAnimInstance() : nullptr;
    if (!Owner.ACFFreeClimbAnimInstance || !Owner.ACFFreeClimbAnimInstance->GetClimbExitMontage())
    {
        UE_LOG(LogACFFreeClimb, Warning, TEXT("Climb exit montage is unavailable."));
        return false;
    }
    if (!AnimInstance)
    {
        UE_LOG(LogACFFreeClimb, Warning, TEXT("Climb exit montage could not play because the character mesh has no AnimInstance."));
        return false;
    }

    const float MontageLength = AnimInstance->Montage_Play(Owner.ACFFreeClimbAnimInstance->GetClimbExitMontage(), 1.0f, EMontagePlayReturnType::MontageLength, StartPosition, true);

    if (MontageLength <= 0.0f)
    {
        return false;
    }

    FOnMontageEnded EndDelegate;
    EndDelegate.BindUObject(&Owner, &UACFFreeClimbComponent::OnClimbExitMontageEnded);
    AnimInstance->Montage_SetEndDelegate(EndDelegate, Owner.ACFFreeClimbAnimInstance->GetClimbExitMontage());

    return true;
}

void FACFFreeClimbState_ClimbExit::TickRemote(float DeltaTime)
{
    if (!bMontagePlaying)
    {
        Finish();
        return;
    }

    UpdateRotation(DeltaTime);
}

void FACFFreeClimbState_ClimbExit::OnExit()
{
    UAnimInstance* AnimInstance = Owner.MeshComponent ? Owner.MeshComponent->GetAnimInstance() : nullptr;

    if (AnimInstance && Owner.ACFFreeClimbAnimInstance && Owner.ACFFreeClimbAnimInstance->GetClimbExitMontage() &&
        AnimInstance->Montage_IsPlaying(Owner.ACFFreeClimbAnimInstance->GetClimbExitMontage()))
    {
        FOnMontageEnded EmptyEndDelegate;
        AnimInstance->Montage_SetEndDelegate(EmptyEndDelegate, Owner.ACFFreeClimbAnimInstance->GetClimbExitMontage());
        AnimInstance->Montage_Stop(0.0f, Owner.ACFFreeClimbAnimInstance->GetClimbExitMontage());
    }

    Owner.CapsuleComponent->SetCollisionEnabled(SavedCapsuleCollisionEnabled);
    bMontagePlaying = false;

    if (UACFWalkingComponent* WalkingComponent = Owner.OwnerCharacter->FindComponentByClass<UACFWalkingComponent>())
    {
        WalkingComponent->PrimeCollisionPatch(false);
    }

    if (Owner.IsNetworkedGame() && Owner.GetTransitionTarget() == EACFFreeClimbState::Idle)
    {
        Owner.ExitClimbState();
    }
}

void FACFFreeClimbState_ClimbExit::Finish()
{
    UE_LOG(LogACFFreeClimb, Log, TEXT("Finish climb exit"));

    const bool bWasWalkableFloor = ResolveExitFloor();

    Owner.bPendingExitToWalking = bWasWalkableFloor;
    if (bWasWalkableFloor)
    {
        Owner.MovementComponent->Velocity = FVector::ZeroVector;
    }
    Owner.TransitionTo(EACFFreeClimbState::Idle);
}

void FACFFreeClimbState_ClimbExit::HandleMontageEnded(UAnimMontage* Montage, bool)
{
    if (Owner.ACFFreeClimbAnimInstance && Owner.ACFFreeClimbAnimInstance->GetClimbExitMontage() &&
        Montage == Owner.ACFFreeClimbAnimInstance->GetClimbExitMontage() && Owner.ClimbState == EACFFreeClimbState::ClimbExit)
    {
        Finish();
    }
}

void FACFFreeClimbState_ClimbExit::UpdateRotation(float DeltaTime)
{
    RotationBlendElapsed += DeltaTime;

    const float Duration = FMath::Max(Owner.ClimbExitRotationBlendDuration, KINDA_SMALL_NUMBER);
    const float Alpha = FMath::Clamp(RotationBlendElapsed / Duration, 0.0f, 1.0f);

    const float BlendedPitch = FMath::Lerp(InitialRotation.Pitch, 0.0f, Alpha);
    const float BlendedRoll = FMath::Lerp(InitialRotation.Roll, 0.0f, Alpha);
    Owner.OwnerCharacter->SetActorRotation(FRotator(BlendedPitch, InitialRotation.Yaw, BlendedRoll));

    const float CapsuleHalfLength = Owner.CapsuleComponent->GetScaledCapsuleHalfHeight() - Owner.CapsuleComponent->GetScaledCapsuleRadius();
    const float UprightDot = FMath::Clamp(FVector::DotProduct(Owner.OwnerCharacter->GetActorUpVector(), FVector::UpVector), 0.0f, 1.0f);
    const float DesiredRise = CapsuleHalfLength * (UprightDot - InitialUprightDot);
    const float RiseDelta = DesiredRise - AppliedRise;

    if (FMath::Abs(RiseDelta) > KINDA_SMALL_NUMBER)
    {
        FHitResult RiseHit;
        Owner.MovementComponent->SafeMoveUpdatedComponent(FVector(0.0f, 0.0f, RiseDelta), Owner.OwnerCharacter->GetActorQuat(), false, RiseHit);
        AppliedRise = DesiredRise;
    }
}

bool FACFFreeClimbState_ClimbExit::ResolveExitFloor()
{
    Owner.CapsuleComponent->SetCollisionEnabled(SavedCapsuleCollisionEnabled);

    UCapsuleComponent* Capsule = Owner.CapsuleComponent;
    const FVector Location = Owner.OwnerCharacter->GetActorLocation();
    const float CapsuleRadius = Capsule->GetScaledCapsuleRadius();
    const float CapsuleHalfHeight = Capsule->GetScaledCapsuleHalfHeight();
    const FVector CapsuleUp = Capsule->GetUpVector();

    const FVector RayStart = Location - CapsuleUp * (CapsuleHalfHeight - CapsuleRadius);
    const FVector RayEnd = RayStart - FVector::UpVector * (CapsuleRadius + Owner.MovementComponent->MaxStepHeight);

    FACFSurfaceHitResult SurfaceHit;
    if (!Owner.QuerySurfaceSegment(RayStart, RayEnd, SurfaceHit))
        return false;

    const FVector TargetLocation = SurfaceHit.Position + FVector::UpVector * CapsuleHalfHeight;
    const FVector Delta = TargetLocation - Location;
    if (!Delta.IsNearlyZero())
    {
        FHitResult MoveHit;
        Owner.MovementComponent->SafeMoveUpdatedComponent(Delta, Owner.OwnerCharacter->GetActorQuat(), false, MoveHit);
    }
    return true;
}
