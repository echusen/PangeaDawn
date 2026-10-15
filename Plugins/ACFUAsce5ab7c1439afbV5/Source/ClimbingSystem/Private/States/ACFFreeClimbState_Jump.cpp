// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "ACFFreeClimbState_Jump.h"

#include "ACFFreeClimbableComponent.h"
#include "ACFFreeClimbComponent.h"
#include "ACFFreeClimbNetworkRuntime.h"

#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Character.h"

void FACFFreeClimbState_Jump::OnEnter()
{
    Path.Reset();
    
    StateStartServerTimeSeconds = 0.0f;
    Duration = Owner.JumpDuration;
    bMovementStarted = !Owner.bJumpMovementWaitsForNotify;
    MovementStartAlpha = 0.0f;

    Owner.JumpProgress = 0.0f;
    
    if (Owner.UsesRemoteState())
    {
        Owner.EnterClimbState();
        Owner.RemoteVisualMoveInput = FVector2D::ZeroVector;
        return;
    }
    
    if (!BuildLocalPath()) { return; }

    if (Owner.ShouldSendState())
    {
        StateStartServerTimeSeconds = Owner.LocalStateStartServerTimeSeconds;
        Owner.SendJumpRepState(BuildJumpRepState());
    }
    else
    {
        StateStartServerTimeSeconds = static_cast<float>(ACFFreeClimbNetwork::GetServerTimeSeconds(Owner.GetWorld()));
    }
}

void FACFFreeClimbState_Jump::Tick(float DeltaTime)
{
    if (Path.Num() < 2)
    {
        Owner.TransitionTo(EACFFreeClimbState::Climb);
        return;
    }
    
    UpdateJump(true);
}

void FACFFreeClimbState_Jump::TickRemote(float DeltaTime)
{
    if (Path.Num() < 2) return;
    
    UpdateJump(false);
}

void FACFFreeClimbState_Jump::ApplyRemoteJumpState(const FACFJumpRepState& State)
{
    Path = State.Path;
    Duration = State.Duration;

    if (StateStartServerTimeSeconds <= 0.0f)
    {
        StateStartServerTimeSeconds = static_cast<float>(
            ACFFreeClimbNetwork::GetServerTimeSeconds(Owner.GetWorld()));
    }
}

void FACFFreeClimbState_Jump::StartJumpMovement()
{
    if (bMovementStarted) return;

    bMovementStarted = true;
    MovementStartAlpha = Owner.JumpProgress;
}

void FACFFreeClimbState_Jump::OnExit()
{
    Path.Reset();
    
    StateStartServerTimeSeconds = 0.0f;
    Duration = 0.0f;
    bMovementStarted = false;
    MovementStartAlpha = 0.0f;

    if (Owner.GetTransitionTarget() == EACFFreeClimbState::Idle)
    {
        Owner.ExitClimbState();
    }
}

bool FACFFreeClimbState_Jump::BuildLocalPath()
{
    if (Owner.JumpSegment <= 0) { return false; }

    // No stick input: default jump direction is up on the climb surface.
    const FVector2D JumpInput = Owner.ClimbInput.IsNearlyZero()
        ? FVector2D(0.0f, 1.0f)
        : Owner.ClimbInput.GetClampedToMaxSize(1.0f);
    
    Path.Reserve(Owner.JumpSegment + 1);
    FACFJumpPathPoint StartPoint;
    if (!BuildPathPointFromCurrentAnchor(StartPoint)) return false;
    Path.Add(MoveTemp(StartPoint));

    const float CapsuleRadius = Owner.CapsuleComponent->GetScaledCapsuleRadius();
    
    const float SegmentDistance = Owner.JumpDistance / Owner.JumpSegment;

    FVector CurrentPoint = Owner.CurrentSurface.Point;
    FVector Normal = Owner.CurrentSurface.SmoothedNormal;

    FVector Direction = Owner.OwnerCharacter->GetTransform().TransformVector(
        FVector(0.0f, JumpInput.X, JumpInput.Y).GetSafeNormal());
    Direction = FVector::VectorPlaneProject(Direction, Normal).GetSafeNormal();
    
    for (int32 Segment = 0; Segment < Owner.JumpSegment; ++Segment)
    {
        const FVector PredictedPoint = CurrentPoint + Direction * SegmentDistance;
        
        FACFSurfaceHitResult Hit;
        if (!Owner.QuerySurfaceAt(PredictedPoint, Normal, Hit))
            break;
        
        FACFJumpPathPoint Point;
        if (!BuildPathPointFromHit(Hit, Point))
            break;

        Path.Add(MoveTemp(Point));

        CurrentPoint = Hit.Position;
        Normal = Hit.Normal.GetSafeNormal();
        Direction = FVector::VectorPlaneProject(Direction, Hit.Normal).GetSafeNormal();
    }
    
    return Path.Num() >= 2;
}

bool FACFFreeClimbState_Jump::BuildPathPointFromCurrentAnchor(FACFJumpPathPoint& OutPoint) const
{
    if (!Owner.bAnchorValid) return false;

    OutPoint = FACFJumpPathPoint{};
    OutPoint.Source = Owner.AnchorSource;

    if (Owner.AnchorSource == EACFSurfaceSource::Skeletal)
    {
        if (!IsValid(Owner.AnchorClimbable.Get()) || Owner.AnchorTriangleIndex == INDEX_NONE) return false;

        OutPoint.ClimbableActor = Owner.AnchorClimbable->GetOwner();
        OutPoint.TriangleIndex = Owner.AnchorTriangleIndex;
        OutPoint.Barycentric = Owner.AnchorBarycentric;
        return true;
    }

    UPrimitiveComponent* Primitive = Owner.AnchorPrimitive.Get();
    if (!IsValid(Primitive)) return false;

    OutPoint.StaticPrimitive = Primitive;
    OutPoint.StaticLocalPosition = Owner.AnchorLocalPos;
    OutPoint.StaticLocalNormal = Owner.AnchorLocalNormal;
    return true;
}

bool FACFFreeClimbState_Jump::BuildPathPointFromHit(const FACFSurfaceHitResult& Hit, FACFJumpPathPoint& OutPoint) const
{
    if (!Hit.IsValidSurface()) return false;

    OutPoint = FACFJumpPathPoint{};
    OutPoint.Source = Hit.Source;

    if (Hit.Source == EACFSurfaceSource::Skeletal)
    {
        if (!IsValid(Hit.Climbable) || Hit.TriangleIndex == INDEX_NONE) return false;

        OutPoint.ClimbableActor = Hit.Climbable->GetOwner();
        OutPoint.TriangleIndex = Hit.TriangleIndex;
        OutPoint.Barycentric = Hit.Barycentric;
        return true;
    }

    UPrimitiveComponent* Primitive = Hit.HitComponent;
    if (!IsValid(Primitive)) return false;

    const FTransform ComponentTransform = Primitive->GetComponentTransform();
    OutPoint.StaticPrimitive = Primitive;
    OutPoint.StaticLocalPosition = ComponentTransform.InverseTransformPosition(Hit.Position);
    OutPoint.StaticLocalNormal = ComponentTransform.InverseTransformVectorNoScale(Hit.Normal.GetSafeNormal());
    return true;
}

FACFJumpRepState FACFFreeClimbState_Jump::BuildJumpRepState()
{
    FACFJumpRepState State;
    State.CommonState = Owner.BuildRepState();
    State.Path = Path;
    State.Duration = Duration;
    
    return State;
}

void FACFFreeClimbState_Jump::UpdateJump(bool bCanInterruptToClimbUp)
{
    const double ServerNow = ACFFreeClimbNetwork::GetServerTimeSeconds(Owner.GetWorld());
    const double Elapsed = FMath::Max(0.0, ServerNow - static_cast<double>(StateStartServerTimeSeconds));
    const float TimeAlpha = FMath::Clamp(static_cast<float>(Elapsed / static_cast<double>(Duration)), 0.0f, 1.0f);
    const float MovementAlpha = bMovementStarted
        ? FMath::Clamp((TimeAlpha - MovementStartAlpha) / FMath::Max(1.0f - MovementStartAlpha, KINDA_SMALL_NUMBER), 0.0f, 1.0f)
        : 0.0f;
    const float PathAlpha = FMath::InterpEaseOut(0.0f, 1.0f, MovementAlpha, 3.0f);
    const float Index = PathAlpha * static_cast<float>(Path.Num() - 1);
    const int32 FromIndex = FMath::FloorToInt(Index);
    const int32 ToIndex = FMath::Min(FromIndex + 1, Path.Num() - 1);
    const float Alpha = Index - static_cast<float>(FromIndex);

    Owner.JumpProgress = TimeAlpha;
    
    FACFSurfaceHitResult FromHit;
    if (!ResolvePathPoint(FromIndex, FromHit)) return;
    
    FACFSurfaceHitResult ToHit;
    if (!ResolvePathPoint(ToIndex, ToHit)) return;
    
    const FTransform FromTransform = MakePathTransform(FromHit.Position, FromHit.Normal);
    const FTransform ToTransform = MakePathTransform(ToHit.Position, ToHit.Normal);
    const FVector Location = FMath::Lerp(FromTransform.GetLocation(), ToTransform.GetLocation(), Alpha);
    const FQuat Rotation = FQuat::Slerp(FromTransform.GetRotation(), ToTransform.GetRotation(), Alpha).GetNormalized();
    const FVector SurfacePoint = FMath::Lerp(FromHit.Position, ToHit.Position, Alpha);
    const FVector SurfaceNormal = FMath::Lerp(FromHit.Normal.GetSafeNormal(), ToHit.Normal.GetSafeNormal(), Alpha).GetSafeNormal();
    
    Owner.CurrentSurface.bValid = true;
    Owner.CurrentSurface.Point = SurfacePoint;
    
    Owner.CurrentSurface.RawNormal = SurfaceNormal;
    Owner.CurrentSurface.PoseTargetNormal = SurfaceNormal;
    Owner.CurrentSurface.SmoothedNormal = SurfaceNormal;
    Owner.BuildSurfaceBasis(Owner.CurrentSurface);
    Owner.OwnerCharacter->SetActorLocationAndRotation(Location, Rotation);

    // Remote instances follow the replicated state transition rather than
    // independently interrupting their copy of the jump path.
    if (bCanInterruptToClimbUp && Owner.ClimbInput.Y > KINDA_SMALL_NUMBER)
    {
        FACFSurfaceHitResult GripHit;
        if (Owner.CanStartClimbUp(GripHit))
        {
            Owner.TransitionTo(EACFFreeClimbState::ClimbUp, &GripHit);
            return;
        }
    }
    
    if (TimeAlpha < 1.0f) return;
    
    FinishJump();
}

FTransform FACFFreeClimbState_Jump::MakePathTransform(const FVector& AnchorPoint, const FVector& AnchorNormal) const
{
    const FVector Normal = AnchorNormal.GetSafeNormal();
    const FQuat Rotation = FRotationMatrix::MakeFromXZ(-Normal, Owner.ProjectSurfaceUp(Normal)).ToQuat().GetNormalized();
    const FVector Location = Owner.GetAnchorAdjustedActorLocation(AnchorPoint, Normal, Rotation);
    
    return FTransform(Rotation, Location, Owner.OwnerCharacter->GetActorScale3D());
}

bool FACFFreeClimbState_Jump::ResolvePathPoint(int32 Index, FACFSurfaceHitResult& OutHit) const
{
    if (!Path.IsValidIndex(Index)) return false;

    const FACFJumpPathPoint& Point = Path[Index];
    OutHit = FACFSurfaceHitResult{};
    OutHit.Source = Point.Source;

    if (Point.Source == EACFSurfaceSource::Skeletal)
    {
        UACFFreeClimbableComponent* Climbable = Point.ClimbableActor
            ? Point.ClimbableActor->FindComponentByClass<UACFFreeClimbableComponent>()
            : nullptr;
        if (!IsValid(Climbable)) return false;

        if (!Climbable->ResolveAnchor(Point.TriangleIndex, Point.Barycentric, OutHit.Position, OutHit.Normal)) return false;

        OutHit.Climbable = Climbable;
        OutHit.TriangleIndex = Point.TriangleIndex;
        OutHit.Barycentric = Point.Barycentric;
        return true;
    }

    UPrimitiveComponent* Primitive = Point.StaticPrimitive.Get();
    if (!IsValid(Primitive)) return false;

    const FTransform ComponentTransform = Primitive->GetComponentTransform();
    OutHit.HitComponent = Primitive;
    OutHit.Position = ComponentTransform.TransformPosition(Point.StaticLocalPosition);
    OutHit.Normal = ComponentTransform.TransformVectorNoScale(Point.StaticLocalNormal).GetSafeNormal();
    return true;
}

void FACFFreeClimbState_Jump::FinishJump()
{
    const int32 LandingIndex = Path.Num() - 1;
    
    FACFSurfaceHitResult LandingHit;
    if (!ResolvePathPoint(LandingIndex, LandingHit))
    {
        if (!Owner.UsesRemoteState())
        {
            Owner.StopClimbing(EACFFreeClimbStopReason::LostSurface);
        }
        return;
    }
    
    Owner.SetAnchorFromHit(LandingHit);
    Owner.UpdateSurfaceFromHit(LandingHit);
    Owner.CurrentSurface.bValid = true;

    Owner.BuildSurfaceBasis(Owner.CurrentSurface);
    Owner.ApplyClimbTransform();
    Owner.TransitionTo(EACFFreeClimbState::Climb);
}
