// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "ACFFreeClimbState_Cling.h"

#include "ACFFreeClimbableComponent.h"
#include "ACFFreeClimbComponent.h"
#include "ACFFreeClimbAnimInstance.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

#include "ACFWalkingComponent.h"

void FACFFreeClimbState_Cling::FillRepState(FACFFreeClimbRepState& State) const
{
    State.MoveInput = Owner.ClimbInput;

    FVector AnchorPoint;
    FVector AnchorNormal;
    if (!Owner.ResolveAnchorWorld(AnchorPoint, AnchorNormal)) return;

    State.bHasAnchorRelativeLocation = true;
    State.AnchorRelativeLocation = ACFFreeClimbNetwork::MakeAnchorRelativeLocation(Owner.OwnerCharacter->GetActorLocation(), AnchorPoint, AnchorNormal);
}

void FACFFreeClimbState_Cling::OnEnter()
{
    RemoteSnapshotBuffer.Reset();

    if (Owner.UsesRemoteState())
    {
        Owner.EnterClimbState();
        Owner.RemoteVisualMoveInput = FVector2D::ZeroVector;
    }

    Cling.Pivot = Owner.CurrentSurface.Point;
    Cling.PrePivot = Cling.Pivot;
    Cling.HangPos = Owner.OwnerCharacter->GetActorLocation();
    Cling.HangVel = FVector::ZeroVector;
    Cling.ArmLength = Owner.CapsuleComponent->GetScaledCapsuleHalfHeight() * Owner.ArmLengthScale;
    Cling.PrevAnchorVel = FVector::ZeroVector;

    PreviousCapsuleCollision = Owner.CapsuleComponent->GetCollisionEnabled();
    Owner.CapsuleComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    PreviousMeshCollision = Owner.MeshComponent->GetCollisionEnabled();
    Owner.MeshComponent->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
    for (const FName& Bone : Owner.LegRootBones)
    {
        Owner.MeshComponent->SetAllBodiesBelowSimulatePhysics(Bone, true, true);
        Owner.MeshComponent->SetAllBodiesBelowPhysicsBlendWeight(Bone, Owner.ClingLegBlend);
    }

    if (UACFWalkingComponent* WalkingComponent = Owner.OwnerCharacter->FindComponentByClass<UACFWalkingComponent>())
    {
        WalkingComponent->PrimeCollisionPatch(true);
    }

    bResourcesActive = true;
}

void FACFFreeClimbState_Cling::Tick(float DeltaTime)
{
    UpdateMovement(DeltaTime);

    if (Owner.ClimbState != EACFFreeClimbState::Cling) return;

    if (Owner.ShouldSendState())
    {
        if (Owner.ConsumeMovementSendTick(DeltaTime))
        {
            Owner.SendRepState(false);
        }
        Owner.PreventCharacterMoveCombining();
    }
}

void FACFFreeClimbState_Cling::ApplyRemoteState(const FACFFreeClimbRepState& State)
{
    if (!State.bHasAnchorRelativeLocation) return;

    FRemoteSnapshot Snapshot;
    Snapshot.Source = State.Source;
    Snapshot.AnchorRelativeLocation = State.AnchorRelativeLocation;
    Snapshot.MoveInput = State.MoveInput;

    UObject* StreamObject = nullptr;

    if (State.Source == EACFSurfaceSource::Skeletal)
    {
        UACFFreeClimbableComponent* Climbable = Owner.AnchorClimbable;
        if (!Climbable) return;
        Snapshot.Climbable = Climbable;
        Snapshot.AnchorTriangleIndex = State.AnchorTriangleIndex;
        Snapshot.AnchorBarycentric = State.AnchorBarycentric;
        StreamObject = Climbable;
    }
    else
    {
        UPrimitiveComponent* Primitive = State.StaticPrimitive;
        if (!Primitive) return;
        Snapshot.Primitive = Primitive;
        Snapshot.LocalPosition = State.StaticLocalPosition;
        Snapshot.LocalNormal = State.StaticLocalNormal;
        StreamObject = Primitive;
    }

    RemoteSnapshotBuffer.Add(State.SessionId, State.Sequence, State.ServerTimeSeconds, StreamObject, Owner.GetRemoteInterpolationDelay(), MoveTemp(Snapshot));
}

bool FACFFreeClimbState_Cling::ResolveSnapshot(const FRemoteSnapshot& Snapshot, FVector& OutPoint, FVector& OutNormal)
{
    if (Snapshot.Source == EACFSurfaceSource::Skeletal)
    {
        UACFFreeClimbableComponent* Climbable = Snapshot.Climbable.Get();
        return Climbable && Climbable->ResolveAnchor(Snapshot.AnchorTriangleIndex, Snapshot.AnchorBarycentric, OutPoint, OutNormal);
    }

    UPrimitiveComponent* Primitive = Snapshot.Primitive.Get();
    if (!Primitive) return false;

    const FTransform ComponentTransform = Primitive->GetComponentTransform();
    OutPoint = ComponentTransform.TransformPosition(Snapshot.LocalPosition);
    OutNormal = ComponentTransform.TransformVectorNoScale(Snapshot.LocalNormal).GetSafeNormal();
    return true;
}

void FACFFreeClimbState_Cling::TickRemote(float DeltaTime)
{
    const FRemoteSnapshot* From = nullptr;
    const FRemoteSnapshot* To = nullptr;
    float Alpha = 0.0f;
    
    if (!RemoteSnapshotBuffer.Evaluate(DeltaTime, From, To, Alpha)) return;

    if (From->Source != To->Source) return;

    FVector FromAnchorPoint;
    FVector FromAnchorNormal;
    if (!ResolveSnapshot(*From, FromAnchorPoint, FromAnchorNormal)) return;

    FVector ToAnchorPoint;
    FVector ToAnchorNormal;
    if (!ResolveSnapshot(*To, ToAnchorPoint, ToAnchorNormal)) return;

    const FVector FromRootLocation = ACFFreeClimbNetwork::MakeWorldLocationFromAnchor(From->AnchorRelativeLocation, FromAnchorPoint, FromAnchorNormal);
    const FVector ToRootLocation = ACFFreeClimbNetwork::MakeWorldLocationFromAnchor(To->AnchorRelativeLocation, ToAnchorPoint, ToAnchorNormal);
    const FVector RootLocation = FMath::Lerp(FromRootLocation, ToRootLocation, Alpha);
    const FVector AnchorPoint = FMath::Lerp(FromAnchorPoint, ToAnchorPoint, Alpha);
    const FVector AnchorNormal = FMath::Lerp(FromAnchorNormal.GetSafeNormal(), ToAnchorNormal.GetSafeNormal(), Alpha).GetSafeNormal();
    
    Owner.CurrentSurface.bValid = true;
    Owner.CurrentSurface.Point = AnchorPoint;
    Owner.CurrentSurface.RawNormal = AnchorNormal;
    Owner.CurrentSurface.PoseTargetNormal = AnchorNormal;
    Owner.CurrentSurface.SmoothedNormal = AnchorNormal;
    Owner.BuildSurfaceBasis(Owner.CurrentSurface);

    Owner.AnchorSource = From->Source;
    if (From->Source == EACFSurfaceSource::Skeletal)
    {
        Owner.AnchorClimbable = From->Climbable.Get();
        Owner.AnchorTriangleIndex = From->AnchorTriangleIndex;
        Owner.AnchorBarycentric = From->AnchorBarycentric;
        Owner.AnchorPrimitive.Reset();
    }
    else
    {
        Owner.AnchorPrimitive = From->Primitive;
        Owner.AnchorLocalPos = From->LocalPosition;
        Owner.AnchorLocalNormal = From->LocalNormal;
        Owner.AnchorClimbable = nullptr;
        Owner.AnchorTriangleIndex = INDEX_NONE;
    }
    Owner.bAnchorValid = true;

    Owner.RemoteVisualMoveInput = FMath::Lerp(From->MoveInput, To->MoveInput, Alpha);

    const FVector RootDelta = RootLocation - AnchorPoint;
    const FVector HangDirection = RootDelta.SizeSquared() > KINDA_SMALL_NUMBER ? RootDelta.GetSafeNormal() : -FVector::UpVector;

    FVector Forward = -AnchorNormal;
    if (FMath::Abs(FVector::DotProduct(-HangDirection, Forward)) > THRESH_NORMALS_ARE_PARALLEL)
    {
        Forward = Owner.CurrentSurface.SurfaceRight;
    }

    const FQuat RootRotation = FRotationMatrix::MakeFromZX(-HangDirection, Forward).ToQuat().GetNormalized();
    Owner.OwnerCharacter->SetActorLocationAndRotation(RootLocation, RootRotation);

    TryPinHands();
}

void FACFFreeClimbState_Cling::OnExit()
{
    if (bResourcesActive)
    {
        if (UACFWalkingComponent* WalkingComponent = Owner.OwnerCharacter->FindComponentByClass<UACFWalkingComponent>())
        {
            WalkingComponent->PrimeCollisionPatch(false);
        }

        Owner.CapsuleComponent->SetCollisionEnabled(PreviousCapsuleCollision);
        for (const FName& Bone : Owner.LegRootBones)
        {
            Owner.MeshComponent->SetAllBodiesBelowSimulatePhysics(Bone, false, true);
            Owner.MeshComponent->SetAllBodiesBelowPhysicsBlendWeight(Bone, 0.0f);
        }
        Owner.MeshComponent->SetCollisionEnabled(PreviousMeshCollision);
        bResourcesActive = false;
    }

    Cling = FACFClingState();

    if (Owner.ACFFreeClimbAnimInstance)
    {
        Owner.ACFFreeClimbAnimInstance->ReleaseAllPins();
    }
    bHandsPinned = false;

    RemoteSnapshotBuffer.Reset();
    Owner.RemoteVisualMoveInput = FVector2D::ZeroVector;

    if (Owner.GetTransitionTarget() == EACFFreeClimbState::Idle)
    {
        Owner.ExitClimbState();
    }
}

void FACFFreeClimbState_Cling::UpdateMovement(float DeltaTime)
{
    if (DeltaTime <= KINDA_SMALL_NUMBER)
    {
        return;
    }

    FVector AnchorPoint;
    FVector AnchorNormal;
    if (!Owner.ResolveAnchorWorld(AnchorPoint, AnchorNormal))
    {
        Owner.StopClimbing(EACFFreeClimbStopReason::LostSurface);
        return;
    }

    Cling.Pivot = AnchorPoint;
    Owner.CurrentSurface.Point = AnchorPoint;
    Owner.CurrentSurface.RawNormal = AnchorNormal;
    Owner.CurrentSurface.PoseTargetNormal = AnchorNormal.GetSafeNormal();
    Owner.CurrentSurface.SmoothedNormal =
        FMath::VInterpTo(Owner.CurrentSurface.SmoothedNormal, AnchorNormal, DeltaTime, Owner.NormalInterpSpeed).GetSafeNormal();
    Owner.BuildSurfaceBasis(Owner.CurrentSurface);

    TryPinHands();

    const FVector AnchorVelocity = (Cling.Pivot - Cling.PrePivot) / DeltaTime;
    Cling.HangPos += AnchorVelocity * DeltaTime;
    Cling.HangVel -= (AnchorVelocity - Cling.PrevAnchorVel) * Owner.AnchorDragCoef;
    Cling.HangVel += FVector(0.0f, 0.0f, -Owner.ClingGravity) * DeltaTime;

    ResolveCollision(DeltaTime);
    Cling.HangPos += Cling.HangVel * DeltaTime;

    const FVector Delta = Cling.HangPos - Cling.Pivot;
    const float Distance = Delta.Size();
    const FVector HangDirection = Distance > KINDA_SMALL_NUMBER ? Delta / Distance : -FVector::UpVector;
    const bool bTaut = Distance > Cling.ArmLength;
    if (bTaut)
    {
        Cling.HangPos = Cling.Pivot + HangDirection * Cling.ArmLength;
        const float RadialVelocity = FVector::DotProduct(Cling.HangVel, HangDirection);
        if (RadialVelocity > 0.0f)
        {
            Cling.HangVel -= RadialVelocity * HangDirection;
        }
    }

    Cling.HangVel *= FMath::Clamp(1.0f - Owner.ClingDamping, 0.0f, 1.0f);

    const float SwingSpeed = Cling.HangVel.Size();
    const float SwingAngleDegrees = FMath::RadiansToDegrees(FMath::Acos(
        FMath::Clamp(FVector::DotProduct(HangDirection, -FVector::UpVector), -1.0f, 1.0f)));

    FVector Forward = -Owner.CurrentSurface.SmoothedNormal;
    if (FMath::Abs(FVector::DotProduct(-HangDirection, Forward)) > THRESH_NORMALS_ARE_PARALLEL)
    {
        Forward = Owner.CurrentSurface.SurfaceRight;
    }
    Owner.OwnerCharacter->SetActorLocationAndRotation(Cling.HangPos, FRotationMatrix::MakeFromZX(-HangDirection, Forward).ToQuat());

    Cling.PrePivot = Cling.Pivot;
    Cling.PrevAnchorVel = AnchorVelocity;

    if (Owner.bDrawDebug)
    {
        DrawDebugSphere(Owner.GetWorld(), Cling.Pivot, 8.0f, 12, FColor::Cyan, false, 0.0f, 0, 1.5f);
        DrawDebugLine(Owner.GetWorld(), Cling.Pivot, Cling.HangPos, FColor::Cyan, false, 0.0f, 0, 1.5f);
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(
                -1,
                0.0f,
                FColor::Cyan,
                FString::Printf(TEXT("CLING  Taut=%d  Speed=%.0f  Angle=%.0f"), bTaut ? 1 : 0, SwingSpeed, SwingAngleDegrees));
        }
    }
}

void FACFFreeClimbState_Cling::ResolveCollision(float DeltaTime)
{
    if (!Owner.bClingCollision || !Owner.AnchorClimbable)
    {
        return;
    }

    const float CapsuleRadius = Owner.CapsuleComponent->GetScaledCapsuleRadius();
    const float ProbeRadius = Owner.ClingProbeRadius > 0.0f ? FMath::Min(CapsuleRadius, Owner.ClingProbeRadius) : CapsuleRadius;

    const FVector PredictedPosition = Cling.HangPos + Cling.HangVel * DeltaTime;

    FACFSurfaceCapsuleSweepQuery Query;
    Query.StartA = PredictedPosition;
    Query.StartB = PredictedPosition;
    Query.EndA = PredictedPosition;
    Query.EndB = PredictedPosition;
    Query.Radius = ProbeRadius + Owner.ClingSurfacePadding;

    FACFSurfaceCapsuleHitResult CapsuleHit;
    if (Owner.AnchorClimbable->SweepCapsuleSurface(Query, CapsuleHit))
    {
        FVector PushDirection = CapsuleHit.BlendedPushDir.GetSafeNormal();
        if (PushDirection.IsNearlyZero())
        {
            PushDirection = CapsuleHit.PushNormal;
        }

        constexpr float DepenetrationPadding = 0.1f;
        const float PushOut = CapsuleHit.MaxPenetrationDepth - Owner.ClingSurfacePadding + DepenetrationPadding;
        if (PushOut > 0.0f)
        {
            Cling.HangPos += PushDirection * PushOut;
        }

        const float InwardVelocity = FVector::DotProduct(Cling.HangVel, -PushDirection);
        if (InwardVelocity > 0.0f)
        {
            Cling.HangVel += PushDirection * InwardVelocity;
        }
    }
}

void FACFFreeClimbState_Cling::TryPinHands()
{
    if (bHandsPinned || !Owner.ACFFreeClimbAnimInstance)
    {
        return;
    }

    const FVector Pivot = Owner.CurrentSurface.Point;
    const FVector Normal = Owner.CurrentSurface.SmoothedNormal;
    const FVector Right = Owner.CurrentSurface.SurfaceRight;

    auto PinHand = [&](EACFFreeClimbLimb Limb, float Sign) -> bool
    {
        FACFSurfaceHitResult Hit;
        const FVector Target = Pivot + Right * (Sign * Owner.ClingHandOffset);
        if (Owner.QuerySurfaceAt(Target, Normal, Hit) || Owner.QuerySurfaceAt(Pivot, Normal, Hit))
        {
            Owner.ACFFreeClimbAnimInstance->PinLimbToAnchor(Limb, Hit);
            return true;
        }
        return false;
    };

    const bool bLeft = PinHand(EACFFreeClimbLimb::LeftHand, -1.0f);
    const bool bRight = PinHand(EACFFreeClimbLimb::RightHand, 1.0f);
    if (bLeft && bRight)
    {
        bHandsPinned = true;
    }
}

