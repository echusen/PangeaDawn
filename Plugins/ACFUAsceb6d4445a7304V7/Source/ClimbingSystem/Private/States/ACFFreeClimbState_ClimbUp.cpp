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
#include "GameFramework/SpringArmComponent.h"

FACFFreeClimbState_ClimbUp::~FACFFreeClimbState_ClimbUp()
{
    RestoreCameraSettings();
}

void FACFFreeClimbState_ClimbUp::SuspendCameraCollision()
{
    TInlineComponentArray<USpringArmComponent*> SpringArms(Owner.OwnerCharacter);
    for (USpringArmComponent* SpringArm : SpringArms)
    {
        if (!SpringArm->IsActive() || SpringArm->TargetArmLength <= 0.0f)
        {
            continue;
        }

        FSavedSpringArmSettings& Settings = SavedSpringArmSettings.AddDefaulted_GetRef();
        Settings.SpringArm = SpringArm;
        Settings.bDoCollisionTest = SpringArm->bDoCollisionTest;
        Settings.bEnableCameraLag = SpringArm->bEnableCameraLag;

        // The warped capsule is allowed through the lip (ProbeOnly). Its camera pivot can
        // therefore start the spring-arm sweep inside the ledge, collapsing the arm to zero.
        // Suspend the probe only for the montage, not for the whole climbing session.
        SpringArm->bDoCollisionTest = false;
        // Follow the warped pivot without leaving a lagged camera below/behind the lip when
        // collision is restored. The arm refreshes its lag history normally on every tick.
        SpringArm->bEnableCameraLag = false;
    }
}

void FACFFreeClimbState_ClimbUp::RestoreCameraSettings()
{
    for (const FSavedSpringArmSettings& Settings : SavedSpringArmSettings)
    {
        if (USpringArmComponent* SpringArm = Settings.SpringArm.Get())
        {
            SpringArm->bDoCollisionTest = Settings.bDoCollisionTest;
            SpringArm->bEnableCameraLag = Settings.bEnableCameraLag;
        }
    }
    SavedSpringArmSettings.Reset();
}

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

    /* Repeat CanStartClimbUp's walkable probe, including its walkability test. Taking whichever
    segment happens to hit first is what put the feet in mid-air: the horizontal segment runs a probe
    distance above the grip and on a continuing wall it returns the wall face, which is no floor at
    all. Only a surface the movement component would accept as ground is a valid landing. */
    const FVector Up = Owner.OwnerCharacter->GetActorUpVector();
    const FVector Forward = Owner.OwnerCharacter->GetActorForwardVector();
    const float ProbeDistance = Owner.ClimbUpWalkableProbeDistance;
    const FVector ProbeStart = PreviousAnchorWorldPoint + Up * ProbeDistance;
    const FVector ProbeCorner = ProbeStart + Forward * ProbeDistance;

    FACFSurfaceHitResult UpperHit;
    FACFSurfaceHitResult InnerHit;
    const bool bUpperHit = Owner.QuerySurfaceSegment(ProbeStart, ProbeCorner, UpperHit);
    const bool bInnerHit = Owner.QuerySurfaceSegment(ProbeCorner, ProbeCorner - Up * ProbeDistance * 2.0f, InnerHit);

    const float WalkableFloorZ = Owner.MovementComponent->GetWalkableFloorZ();
    const bool bUpperWalkable = bUpperHit && FVector::DotProduct(UpperHit.Normal.GetSafeNormal(), FVector::UpVector) >= WalkableFloorZ;
    const bool bInnerWalkable = bInnerHit && FVector::DotProduct(InnerHit.Normal.GetSafeNormal(), FVector::UpVector) >= WalkableFloorZ;

    /* The downward segment is the one that reports the top's own height, so it wins when it found
    ground. The horizontal segment stands in only when the downward one found none. */
    FACFSurfaceHitResult FloorHit;
    if (bInnerWalkable)
    {
        FloorHit = InnerHit;
    }
    else if (bUpperWalkable)
    {
        FloorHit = UpperHit;
    }
    else
    {
        UE_LOG(
            LogACFFreeClimb,
            Warning,
            TEXT("Climb-up aborted for '%s': no walkable top at the grip. Upper hit: %d (walkable %d), inner hit: %d (walkable %d)."),
            *GetNameSafe(Owner.OwnerCharacter), bUpperHit, bUpperWalkable, bInnerHit, bInnerWalkable);

        Owner.bPendingExitToWalking = false;
        Owner.TransitionTo(EACFFreeClimbState::Idle);
        return;
    }

    /* SkewWarp moves the feet onto the target, not the capsule centre: bWarpToFeetLocation defaults
    to true and the modifier measures from GetVisualRootLocation, which is the capsule bottom. So the
    floor surface itself is the destination, and adding the capsule half-height here would land the
    character a capsule above the ledge. */
    TargetOffset = FloorHit.Position - PreviousAnchorWorldPoint;

    if (Owner.bDrawDebug)
    {
        const float CapsuleHalfHeight = Owner.CapsuleComponent->GetScaledCapsuleHalfHeight();
        const FVector ActorLocation = Owner.OwnerCharacter->GetActorLocation();
        UE_LOG(
            LogACFFreeClimb,
            Log,
            TEXT("Climb-up for '%s': anchor %s, floor %s (from %s probe, normal %s), target feet %s. Capsule centre %s, feet %s, half height %.1f. Expected rise %.1f cm."),
            *GetNameSafe(Owner.OwnerCharacter),
            *PreviousAnchorWorldPoint.ToCompactString(),
            *FloorHit.Position.ToCompactString(),
            bInnerWalkable ? TEXT("downward") : TEXT("horizontal"),
            *FloorHit.Normal.GetSafeNormal().ToCompactString(),
            *(PreviousAnchorWorldPoint + TargetOffset).ToCompactString(),
            *ActorLocation.ToCompactString(),
            *(ActorLocation - Up * CapsuleHalfHeight).ToCompactString(),
            CapsuleHalfHeight,
            (PreviousAnchorWorldPoint + TargetOffset).Z - (ActorLocation.Z - CapsuleHalfHeight));
    }
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

    SuspendCameraCollision();
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
    // Covers completion, interruption, lost anchors, and failed/aborted entries alike.
    RestoreCameraSettings();

    if (Owner.bDrawDebug)
    {
        /* Where the warp actually left the feet, against where it was aimed. A gap here is the warp
        falling short or overshooting; no gap with the character still looking wrong means the error
        is in the floor the probe picked, logged on entry. */
        const float CapsuleHalfHeight = Owner.CapsuleComponent->GetScaledCapsuleHalfHeight();
        const FVector FeetLocation = Owner.OwnerCharacter->GetActorLocation() - Owner.OwnerCharacter->GetActorUpVector() * CapsuleHalfHeight;
        const FVector TargetLocation = PreviousAnchorWorldPoint + TargetOffset;
        UE_LOG(
            LogACFFreeClimb,
            Log,
            TEXT("Climb-up finished for '%s': feet landed %s, target was %s, vertical error %.1f cm."),
            *GetNameSafe(Owner.OwnerCharacter),
            *FeetLocation.ToCompactString(),
            *TargetLocation.ToCompactString(),
            FeetLocation.Z - TargetLocation.Z);
    }

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
