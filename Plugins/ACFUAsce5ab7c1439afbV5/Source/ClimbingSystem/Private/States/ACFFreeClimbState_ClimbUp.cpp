// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "ACFFreeClimbState_ClimbUp.h"
#include "ClimbingSystem.h"
#include "RootMotionModifier_SkewWarp.h"
#include "ACFFreeClimbComponent.h"
#include "ACFFreeClimbAnimInstance.h"
#include "ACFFreeClimbNetworkRuntime.h"
#include "ACFWalkingComponent.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimTypes.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

void FACFFreeClimbState_ClimbUp::OnEnter()
{
    SavedCapsuleCollisionEnabled = Owner.CapsuleComponent->GetCollisionEnabled();
    UAnimInstance* AnimInstance = Owner.MeshComponent ? Owner.MeshComponent->GetAnimInstance() : nullptr;

    const bool bHasClimbAnimInstance = Owner.ACFFreeClimbAnimInstance && Owner.ACFFreeClimbAnimInstance->GetClimbUpMontage();
    const bool bHasAnimInstance = AnimInstance != nullptr;
    if (!bHasClimbAnimInstance || !bHasAnimInstance)
    {
        UE_LOG(
            LogACFFreeClimb,
            Warning,
            TEXT("Climb-up aborted for '%s': requires a valid AnimInstance, UACFFreeClimbAnimInstance, and ClimbUpMontage."),
            *GetNameSafe(Owner.OwnerCharacter));

        Owner.bPendingExitToWalking = false;
        Owner.TransitionTo(EACFFreeClimbState::Idle);
        return;
    }

    MotionWarpingComponent = Owner.OwnerCharacter->FindComponentByClass<UMotionWarpingComponent>();
    if (!MotionWarpingComponent.IsValid())
    {
        UMotionWarpingComponent* NewComponent = NewObject<UMotionWarpingComponent>(Owner.OwnerCharacter);
        Owner.OwnerCharacter->AddInstanceComponent(NewComponent);
        NewComponent->RegisterComponent();
        MotionWarpingComponent = NewComponent;
    }
    SavedWarpTargets.Reset();
    for (const FName Name : { FName(TEXT("ReachTarget")), FName(TEXT("ReachPoint")) })
    {
        if (const FMotionWarpingTarget* Target = MotionWarpingComponent->FindWarpTarget(Name))
        {
            SavedWarpTargets.Add(*Target);
        }
    }
    SavedAnimInstance = AnimInstance;
    SavedRootMotionMode = AnimInstance->RootMotionMode;
    AnimInstance->SetRootMotionMode(ERootMotionMode::RootMotionFromMontagesOnly);
    Owner.MovementComponent->StopMovementImmediately();
    Owner.MovementComponent->SetMovementMode(MOVE_Flying);
    Owner.MovementComponent->bIgnoreClientMovementErrorChecksAndCorrection = true;
    Owner.MovementComponent->bClientIgnoreMovementCorrections = true;
    Owner.PreventCharacterMoveCombining();
    Owner.RemoteVisualMoveInput = FVector2D::ZeroVector;
    Owner.CapsuleComponent->SetCollisionEnabled(ECollisionEnabled::ProbeOnly);

    if (UACFWalkingComponent* WalkingComponent = Owner.OwnerCharacter->FindComponentByClass<UACFWalkingComponent>())
    {
        WalkingComponent->PrimeCollisionPatch(true);
    }

    if (Owner.UsesRemoteState())
    {
        FVector AnchorNormal;
        Owner.ResolveAnchorWorld(PreviousAnchorWorldPoint, AnchorNormal);
    }
    else
    {
        FACFSurfaceHitResult Hit;
        Owner.ConsumeTransitionHit(Hit);
        Owner.SetAnchorFromHit(Hit);
        FVector AnchorNormal;
        Owner.ResolveAnchorWorld(PreviousAnchorWorldPoint, AnchorNormal);
    }

    // Match CanStartClimbUp's walkable probe. SkewWarp targets the feet,
    // so use the surface position without adding the capsule half-height.
    const FVector Up = Owner.OwnerCharacter->GetActorUpVector();
    const FVector Forward = Owner.OwnerCharacter->GetActorForwardVector();
    const float ProbeDistance = Owner.ClimbUpWalkableProbeDistance;
    const FVector ProbeStart = PreviousAnchorWorldPoint + Up * ProbeDistance;
    const FVector ProbeCorner = ProbeStart + Forward * ProbeDistance;
    FACFSurfaceHitResult FloorHit;
    const bool bFoundTop = Owner.QuerySurfaceSegment(ProbeStart, ProbeCorner, FloorHit) ||
        Owner.QuerySurfaceSegment(ProbeCorner, ProbeCorner - Up * ProbeDistance * 2.0f, FloorHit);
    if (!bFoundTop)
    {
        Owner.bPendingExitToWalking = false;
        Owner.TransitionTo(EACFFreeClimbState::Idle);
        return;
    }
    TargetOffset = FloorHit.Position - PreviousAnchorWorldPoint;
    Update();
    AnimInstance->OnPlayMontageNotifyBegin.AddUniqueDynamic(&Owner, &UACFFreeClimbComponent::OnClimbUpMontageNotify);

    FOnMontageEnded EndDelegate;
    EndDelegate.BindUObject(&Owner, &UACFFreeClimbComponent::OnClimbUpMontageEnded);

    constexpr float StartPosition = 0.0f;
    const float MontageLength = AnimInstance->Montage_Play(
        Owner.ACFFreeClimbAnimInstance->GetClimbUpMontage(), Owner.ClimbSpeedScale,
        EMontagePlayReturnType::MontageLength, StartPosition, true);

    if (MontageLength <= 0.0f)
    {
        UE_LOG(LogACFFreeClimb, Warning, TEXT("Climb-up montage failed to play."));
        Owner.bPendingExitToWalking = false;
        Owner.TransitionTo(EACFFreeClimbState::Idle);
        return;
    }

    AnimInstance->Montage_SetEndDelegate(EndDelegate, Owner.ACFFreeClimbAnimInstance->GetClimbUpMontage());
    UAnimMontage* Montage = Owner.ACFFreeClimbAnimInstance->GetClimbUpMontage();
    TArray<FMotionWarpingWindowData> Windows;
    UMotionWarpingUtilities::GetMotionWarpingWindowsFromAnimation(Montage, Windows);
    if (Windows.IsEmpty())
    {
        // Curve-driven montages have no warp notify: supply a runtime window.
        const float WarpEnd = FMath::Max(KINDA_SMALL_NUMBER, Montage->GetPlayLength() - Montage->BlendOut.GetBlendTime());
        RuntimeWarpModifier = URootMotionModifier_SkewWarp::AddRootMotionModifierSkewWarp(
            MotionWarpingComponent.Get(), Montage, 0.0f, WarpEnd, TEXT("ReachTarget"),
            EWarpPointAnimProvider::None, FTransform::Identity, NAME_None, true, false, true);
    }
}

void FACFFreeClimbState_ClimbUp::Tick(float)
{
    if (!Owner.ACFFreeClimbAnimInstance)
    {
        UE_LOG(LogACFFreeClimb, Warning, TEXT("Climb-up stopped. UACFFreeClimbAnimInstance is no longer available."));
        Owner.bPendingExitToWalking = false;
        Owner.TransitionTo(EACFFreeClimbState::Idle);
        return;
    }

    if (!Update())
    {
        UE_LOG(LogACFFreeClimb, Warning, TEXT("Lost climb-up anchor."));
        Owner.bPendingExitToWalking = false;
        Owner.TransitionTo(EACFFreeClimbState::Idle);
    }
}

void FACFFreeClimbState_ClimbUp::ApplyRemoteState(const FACFFreeClimbRepState& State)
{
    if (State.StateStartServerTimeSeconds <= 0.0f)
    {
        return;
    }

    UAnimInstance* AnimInstance = Owner.MeshComponent ? Owner.MeshComponent->GetAnimInstance() : nullptr;
    if (!Owner.ACFFreeClimbAnimInstance || !Owner.ACFFreeClimbAnimInstance->GetClimbUpMontage() ||
        !AnimInstance || !AnimInstance->Montage_IsPlaying(Owner.ACFFreeClimbAnimInstance->GetClimbUpMontage()))
    {
        UE_LOG(LogACFFreeClimb, Warning, TEXT("Remote climb-up montage is unavailable."));
        return;
    }

    constexpr float StartPosition = 0.0f;
    const double ServerNow = ACFFreeClimbNetwork::GetServerTimeSeconds(Owner.GetWorld());
    const float Elapsed = FMath::Max(0.0f, static_cast<float>(ServerNow - State.StateStartServerTimeSeconds));
    const float MontagePosition = StartPosition + Elapsed * Owner.ClimbSpeedScale;
    const float MontageLength = Owner.ACFFreeClimbAnimInstance->GetClimbUpMontage()->GetPlayLength();

    if (MontagePosition < MontageLength)
    {
        AnimInstance->Montage_SetPosition(Owner.ACFFreeClimbAnimInstance->GetClimbUpMontage(), MontagePosition);
    }
}

void FACFFreeClimbState_ClimbUp::TickRemote(float)
{
    if (!Owner.ACFFreeClimbAnimInstance)
    {
        UE_LOG(LogACFFreeClimb, Warning, TEXT("Remote climb-up stopped. UACFFreeClimbAnimInstance is no longer available."));
        Owner.TransitionTo(EACFFreeClimbState::Idle);
        return;
    }

    Update();
}

void FACFFreeClimbState_ClimbUp::HandleMontageNotify(FName NotifyName)
{
    if (NotifyName != FName(TEXT("ClimbUpMovementFinished"))) return;

    // Restore collision on exit, after the warped root motion has finished.
}

void FACFFreeClimbState_ClimbUp::OnExit()
{
    UAnimInstance* AnimInstance = Owner.MeshComponent ? Owner.MeshComponent->GetAnimInstance() : nullptr;

    if (AnimInstance)
    {
        AnimInstance->OnPlayMontageNotifyBegin.RemoveDynamic(&Owner, &UACFFreeClimbComponent::OnClimbUpMontageNotify);
    }

    if (AnimInstance && Owner.ACFFreeClimbAnimInstance && Owner.ACFFreeClimbAnimInstance->GetClimbUpMontage() &&
        AnimInstance->Montage_IsPlaying(Owner.ACFFreeClimbAnimInstance->GetClimbUpMontage()))
    {
        FOnMontageEnded EmptyDelegate;
        AnimInstance->Montage_SetEndDelegate(EmptyDelegate, Owner.ACFFreeClimbAnimInstance->GetClimbUpMontage());
        AnimInstance->Montage_Stop(0.0f, Owner.ACFFreeClimbAnimInstance->GetClimbUpMontage());
    }

    if (RuntimeWarpModifier.IsValid())
    {
        RuntimeWarpModifier->SetState(ERootMotionModifierState::MarkedForRemoval);
        RuntimeWarpModifier.Reset();
    }
    if (MotionWarpingComponent.IsValid())
    {
        MotionWarpingComponent->RemoveWarpTarget(TEXT("ReachTarget"));
        MotionWarpingComponent->RemoveWarpTarget(TEXT("ReachPoint"));
        for (const FMotionWarpingTarget& Target : SavedWarpTargets)
        {
            MotionWarpingComponent->AddOrUpdateWarpTarget(Target);
        }
        MotionWarpingComponent.Reset();
    }
    if (SavedAnimInstance.IsValid())
    {
        SavedAnimInstance->SetRootMotionMode(SavedRootMotionMode);
        SavedAnimInstance.Reset();
    }
    Owner.CapsuleComponent->SetCollisionEnabled(SavedCapsuleCollisionEnabled);

    if (UACFWalkingComponent* WalkingComponent = Owner.OwnerCharacter->FindComponentByClass<UACFWalkingComponent>())
    {
        WalkingComponent->PrimeCollisionPatch(false);
    }

    if (Owner.GetTransitionTarget() == EACFFreeClimbState::Idle)
    {
        Owner.ExitClimbState();
    }
}

bool FACFFreeClimbState_ClimbUp::Update()
{
    FVector AnchorPoint;
    FVector AnchorNormal;
    if (!MotionWarpingComponent.IsValid() || !Owner.ResolveAnchorWorld(AnchorPoint, AnchorNormal))
    {
        return false;
    }

    FHitResult MoveHit;
    Owner.MovementComponent->SafeMoveUpdatedComponent(AnchorPoint - PreviousAnchorWorldPoint, Owner.OwnerCharacter->GetActorQuat(), false, MoveHit);
    PreviousAnchorWorldPoint = AnchorPoint;

    const FVector TargetLocation = AnchorPoint + TargetOffset;
    const FVector Forward = FVector::VectorPlaneProject(-AnchorNormal, FVector::UpVector).GetSafeNormal();
    const FRotator TargetRotation = Forward.IsNearlyZero()
        ? FRotator(0.0f, Owner.OwnerCharacter->GetActorRotation().Yaw, 0.0f)
        : Forward.Rotation();
    MotionWarpingComponent->AddOrUpdateWarpTargetFromLocationAndRotation(TEXT("ReachTarget"), TargetLocation, TargetRotation);
    MotionWarpingComponent->AddOrUpdateWarpTargetFromLocationAndRotation(TEXT("ReachPoint"), TargetLocation, TargetRotation);
    if (Owner.bDrawDebug)
    {
        DrawDebugCoordinateSystem(Owner.GetWorld(), TargetLocation, TargetRotation, 20.0f);
    }
    return true;
}
