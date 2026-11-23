// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "ACFFreeClimbState_Entering.h"
#include "ACFFreeClimbComponent.h"
#include "ACFFreeClimbNetworkRuntime.h"
#include "GameFramework/Character.h"

void FACFFreeClimbState_Entering::OnEnter()
{
    Owner.EnterClimbState();
    
    StartTransform = Owner.OwnerCharacter->GetActorTransform();
    ElapsedTime = 0.0f;
    
    if (Owner.UsesRemoteState()) return;
    if (Owner.ShouldSendState()) { Owner.BeginLocalSession(); }
    
    FACFSurfaceHitResult Hit;
    const bool bHasTransitionHit = Owner.ConsumeTransitionHit(Hit);
    if (bHasTransitionHit)
    {
        if (!Hit.IsValidSurface())
        {
            Owner.StopClimbing(EACFFreeClimbStopReason::LostSurface);
            return;
        }

        Owner.UpdateSurfaceFromHit(Hit);
        Owner.SetAnchorFromHit(Hit);
    }

    if (!Owner.bAnchorValid)
    {
        Owner.StopClimbing(EACFFreeClimbStopReason::LostSurface);
        return;
    }

    Owner.CurrentSurface.bValid = true;
    Owner.BuildSurfaceBasis(Owner.CurrentSurface);

    TargetTransform = Owner.ComputeClimbTransform();
}

void FACFFreeClimbState_Entering::Tick(float DeltaTime)
{
    FVector AnchorPoint;
    FVector AnchorNormal;
    if (!Owner.ResolveAnchorWorld(AnchorPoint, AnchorNormal))
    {
        Owner.StopClimbing(EACFFreeClimbStopReason::LostSurface);
        return;
    }
    
    Owner.CurrentSurface.bValid = true;
    Owner.CurrentSurface.Point = AnchorPoint;
    Owner.CurrentSurface.PoseTargetNormal = Owner.CurrentSurface.RawNormal;
    Owner.CurrentSurface.SmoothedNormal = Owner.CurrentSurface.RawNormal;
    Owner.BuildSurfaceBasis(Owner.CurrentSurface);
    TargetTransform = Owner.ComputeClimbTransform();
    
    ElapsedTime += DeltaTime;
    const float Alpha = Owner.EnteringDuration > KINDA_SMALL_NUMBER
        ? FMath::Clamp(ElapsedTime / Owner.EnteringDuration, 0.0f, 1.0f)
        : 1.0f;
    
    ApplyTransform(Alpha);

    if (Alpha >= 1.0f)
    {
        Owner.OwnerCharacter->SetActorLocationAndRotation(TargetTransform.GetLocation(), TargetTransform.GetRotation());
        Owner.TransitionTo(EACFFreeClimbState::Climb);
    }
}

void FACFFreeClimbState_Entering::ApplyRemoteState(const FACFFreeClimbRepState& State)
{
    RemoteStateStartServerTimeSeconds = State.StateStartServerTimeSeconds;
    Owner.RemoteVisualMoveInput = FVector2D::ZeroVector;
}

void FACFFreeClimbState_Entering::TickRemote(float DeltaTime)
{
    FVector AnchorPoint;
    FVector AnchorNormal;
    if (!Owner.ResolveAnchorWorld(AnchorPoint, AnchorNormal)) { return; }
    
    Owner.CurrentSurface.bValid = true;
    Owner.CurrentSurface.Point = AnchorPoint;
    
    Owner.CurrentSurface.RawNormal = AnchorNormal.GetSafeNormal();
    Owner.CurrentSurface.PoseTargetNormal = Owner.CurrentSurface.RawNormal;
    Owner.CurrentSurface.SmoothedNormal = Owner.CurrentSurface.RawNormal;
    Owner.BuildSurfaceBasis(Owner.CurrentSurface);
    
    TargetTransform = Owner.ComputeClimbTransform();
    
    const double Elapsed = FMath::Max(0.0, ACFFreeClimbNetwork::GetServerTimeSeconds(Owner.GetWorld()) - RemoteStateStartServerTimeSeconds);
    const float Alpha = Owner.EnteringDuration > KINDA_SMALL_NUMBER ? FMath::Clamp(static_cast<float>(Elapsed / Owner.EnteringDuration), 0.0f, 1.0f) : 1.0f;
    
    ApplyTransform(Alpha);
}

void FACFFreeClimbState_Entering::OnExit()
{
    if (Owner.GetTransitionTarget() == EACFFreeClimbState::Idle)
    {
        Owner.ExitClimbState();
    }
}

void FACFFreeClimbState_Entering::ApplyTransform(float Alpha)
{
    
    const float Eased = FMath::SmoothStep(0.0f, 1.0f, Alpha);
    const FVector Location = FMath::Lerp(StartTransform.GetLocation(), TargetTransform.GetLocation(), Eased);
    const FQuat Rotation = FQuat::Slerp(StartTransform.GetRotation(), TargetTransform.GetRotation(), Eased).GetNormalized();
    Owner.OwnerCharacter->SetActorLocationAndRotation(Location, Rotation);
}
