// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "ACFFreeClimbState_Climb.h"
#include "ClimbingSystem.h"
#include "ACFFreeClimbComponent.h"
#include "ACFFreeClimbAnimInstance.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "DrawDebugHelpers.h"
#include "ACFFreeClimbableComponent.h"
#include "Engine/Engine.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "TimerManager.h"

void FACFFreeClimbState_Climb::FillRepState(FACFFreeClimbRepState& State) const
{
    State.MoveInput = Owner.ClimbInput;
    
    State.ClimbRotation = Owner.OwnerCharacter->GetActorQuat().GetNormalized();
}

void FACFFreeClimbState_Climb::OnEnter()
{
    if (Owner.UsesRemoteState())
    {
        Owner.EnterClimbState();
        
        RemoteSnapshotBuffer.Reset();
        Owner.RemoteVisualMoveInput = FVector2D::ZeroVector;
    }
    
    PrevAnchorPoint = Owner.CurrentSurface.Point;
    SmoothedAnchorSpeed = 0.0f;
    SmoothedNormalVelocity = FVector::ZeroVector;
    bHasCompletedInitialMovementUpdate = false;
    bAnchorAdvancedThisTick = false;
    
    if (!Owner.UsesRemoteState())
    {
        Owner.CaptureAnchorRotationOffset();
    }
}

void FACFFreeClimbState_Climb::Tick(float DeltaTime)
{
    // Jump pressed while pushing down is a drop, not a hop to a lower grip: the character lets go
    // of the surface and falls. Checked before the climb-up probe so pressing down at a ledge top
    // drops instead of mantling.
    if (Owner.bJumpPressed && Owner.ClimbInput.Y < -Owner.DropInputThreshold)
    {
        // Consume the request: a button held down must not retrigger every frame.
        Owner.bJumpPressed = false;
        Owner.DropFromSurface();
        return;
    }

    if (Owner.ClimbInput.Size() > KINDA_SMALL_NUMBER)
    {
        FACFSurfaceHitResult GripHit;
        if (Owner.CanStartClimbUp(GripHit))
        {
            Owner.TransitionTo(EACFFreeClimbState::ClimbUp, &GripHit);
            return;
        }
    }

    if (Owner.bJumpPressed)
    {
        // Consume the request: a button held down must not chain one jump per frame.
        Owner.bJumpPressed = false;
        Owner.TransitionTo(EACFFreeClimbState::Jump);
        return;
    }

    const bool bCanEvaluateClimbExit = bHasCompletedInitialMovementUpdate;

    FQuat TrackingRotation;
    UpdateMovement(DeltaTime, TrackingRotation);
    
    if (Owner.ClimbState != EACFFreeClimbState::Climb) return;

    bHasCompletedInitialMovementUpdate = true;

    const float Tilt = FVector::DotProduct(Owner.CurrentSurface.SmoothedNormal, FVector::UpVector);

    const float RawTilt = FVector::DotProduct(Owner.CurrentSurface.RawNormal, FVector::UpVector);
    if (RawTilt < -FMath::Sin(FMath::DegreesToRadians(Owner.MaxClimbAngle)))
    {
        Owner.StopClimbing(EACFFreeClimbStopReason::LostSurface);
        return;
    }

    if (bCanEvaluateClimbExit &&
        Tilt > FMath::Sin(FMath::DegreesToRadians(Owner.ForceWalkAngle)))
    {
        StartClimbExit();
        return;
    }

    if (bCanEvaluateClimbExit &&
        Tilt > FMath::Sin(FMath::DegreesToRadians(Owner.WalkEnterAngle)) &&
        SmoothedAnchorSpeed <= Owner.WalkEnterMaxAnchorSpeed)
    {
        StartClimbExit();
        return;
    }

    if (Owner.ClimbInput.Y < -KINDA_SMALL_NUMBER)
    {
        FFindFloorResult Floor;
        Owner.MovementComponent->FindFloor(Owner.CapsuleComponent->GetComponentLocation(), Floor, false);
        if (Floor.IsWalkableFloor() &&
            (Floor.GetDistanceToFloor() <= Owner.WalkEnterFloorContactDistance || bMoveBlocked))
        {
            FinishToWalk();
            return;
        }

        if (bMoveBlocked)
        {
            Owner.StopClimbing(EACFFreeClimbStopReason::LostSurface);
            return;
        }
    }

    ApplyMovementResult(TrackingRotation);

    if (Owner.ShouldSendState())
    {
        if (Owner.ConsumeMovementSendTick(DeltaTime))
        {
            Owner.SendRepState(false);
        }

        Owner.PreventCharacterMoveCombining();
    }
}

void FACFFreeClimbState_Climb::ApplyRemoteState(const FACFFreeClimbRepState& State)
{
    FRemoteSnapshot Snapshot;
    Snapshot.Source = State.Source;
    Snapshot.MoveInput = State.MoveInput;
    Snapshot.Rotation = State.ClimbRotation.GetNormalized();

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

bool FACFFreeClimbState_Climb::ResolveSnapshot(const FRemoteSnapshot& Snapshot, FVector& OutPoint, FVector& OutNormal)
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

void FACFFreeClimbState_Climb::TickRemote(float DeltaTime)
{
    const FRemoteSnapshot* From = nullptr;
    const FRemoteSnapshot* To = nullptr;
    float Alpha = 0.0f;
    
    if (!RemoteSnapshotBuffer.Evaluate(DeltaTime, From, To, Alpha)) return;
    
    if (From->Source != To->Source) return;
    
    FVector FromPoint;
    FVector FromNormal;
    if (!ResolveSnapshot(*From, FromPoint, FromNormal)) return;
    
    FVector ToPoint;
    FVector ToNormal;
    if (!ResolveSnapshot(*To, ToPoint, ToNormal)) return;
    
    Owner.CurrentSurface.bValid = true;
    Owner.CurrentSurface.Point = FMath::Lerp(FromPoint, ToPoint, Alpha);
    Owner.CurrentSurface.RawNormal = FMath::Lerp(FromNormal.GetSafeNormal(), ToNormal.GetSafeNormal(), Alpha).GetSafeNormal();
    Owner.CurrentSurface.PoseTargetNormal = Owner.CurrentSurface.RawNormal;
    Owner.CurrentSurface.SmoothedNormal = Owner.CurrentSurface.RawNormal;
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
    Owner.LastDesiredClimbDirection = (Owner.CurrentSurface.SurfaceRight * Owner.RemoteVisualMoveInput.X +
        Owner.CurrentSurface.SurfaceUp * Owner.RemoteVisualMoveInput.Y).GetSafeNormal();
    
    const FQuat RemoteRotation = FQuat::Slerp(From->Rotation, To->Rotation, Alpha).GetNormalized();
    Owner.ApplyClimbTransform(RemoteRotation);
    
    if (Owner.GetWorld() && Owner.GetWorld()->GetNetMode() != NM_DedicatedServer)
    {
        Owner.UpdateProbeContacts();
    }
}

void FACFFreeClimbState_Climb::UpdateMovement(float DeltaTime, FQuat& OutTrackingRotation)
{
    bAnchorAdvancedThisTick = false;
    
    FVector AnchorPoint;
    FVector AnchorNormal;
    if (!Owner.ResolveAnchorWorld(AnchorPoint, AnchorNormal))
    {
        Owner.StopClimbing(EACFFreeClimbStopReason::LostSurface);
        return;
    }

    Owner.CurrentSurface.Point = AnchorPoint;
    Owner.CurrentSurface.RawNormal = AnchorNormal;
    Owner.UpdatePoseTargetNormal(AnchorPoint, AnchorNormal, Owner.LastDesiredClimbDirection, Owner.AnchorClimbable);
    Owner.BuildSurfaceBasis(Owner.CurrentSurface);

    if (!Owner.CalculateClimbRotation(DeltaTime, Owner.ClimbInput, OutTrackingRotation))
    {
        Owner.StopClimbing(EACFFreeClimbStopReason::LostSurface);
        return;
    }
    
    const float RawAnchorSpeed = ((AnchorPoint - PrevAnchorPoint) / DeltaTime).Size();
    SmoothedAnchorSpeed = FMath::FInterpTo(SmoothedAnchorSpeed, RawAnchorSpeed, DeltaTime, Owner.NormalInterpSpeed);
    
    const FVector2D MoveInput = Owner.ClimbInput;

    bMoveBlocked = false;

    if (!MoveInput.IsNearlyZero())
    {
        const FVector2D AxisVelocity =
            MoveInput *
            FVector2D(Owner.BaseHorizontalClimbSpeed, Owner.BaseVerticalClimbSpeed) *
            Owner.ClimbSpeedScale;
        
        Owner.LastDesiredClimbDirection =
            (Owner.CurrentSurface.SurfaceRight * MoveInput.X + Owner.CurrentSurface.SurfaceUp * MoveInput.Y).GetSafeNormal();

        const float MoveDistance = AxisVelocity.Size() * DeltaTime;
        const int32 NumSubsteps = FMath::Clamp(FMath::CeilToInt(MoveDistance / Owner.MaxSubstepDistance), 1, Owner.MaxSubstepsPerFrame);
        const float SubstepDistance = MoveDistance / NumSubsteps;

        FVector StepMoveDir =
            (Owner.CurrentSurface.SurfaceRight * AxisVelocity.X +
                Owner.CurrentSurface.SurfaceUp * AxisVelocity.Y).GetSafeNormal();

        for (int32 Step = 0; Step < NumSubsteps; ++Step)
        {
            if (StepMoveDir.IsNearlyZero())
                break;

            FVector ActualMoveDir;

            if (!AdvanceAnchor(StepMoveDir, SubstepDistance, ActualMoveDir))
            {
                bMoveBlocked = true;
                break;
            }
            bAnchorAdvancedThisTick = true;
            StepMoveDir = ActualMoveDir;
        }
    }
    else
    {
        Owner.LastDesiredClimbDirection = FVector::ZeroVector;
    }

    const FVector FromNormal = Owner.CurrentSurface.SmoothedNormal.GetSafeNormal();
    const FVector ToNormal = Owner.CurrentSurface.PoseTargetNormal.GetSafeNormal();

    const float SmoothTime = FMath::Max(Owner.PoseNormalSmoothTime, KINDA_SMALL_NUMBER);
    const float Omega = 2.0f / SmoothTime;
    const float X = Omega * DeltaTime;
    const float Exp = 1.0f / (1.0f + X + 0.48f * X * X + 0.235f * X * X * X);

    const FVector Change = FromNormal - ToNormal;
    const FVector Temp = (SmoothedNormalVelocity + Omega * Change) * DeltaTime;
    SmoothedNormalVelocity = (SmoothedNormalVelocity - Omega * Temp) * Exp;

    Owner.CurrentSurface.SmoothedNormal = (ToNormal + (Change + Temp) * Exp).GetSafeNormal();

    PrevAnchorPoint = Owner.CurrentSurface.Point;
}

void FACFFreeClimbState_Climb::ApplyMovementResult(const FQuat& TrackingRotation)
{
    FQuat FinalRotation = TrackingRotation;
    
    if (bAnchorAdvancedThisTick)
    {
        FinalRotation = Owner.CalculateMovementSurfaceRotation(TrackingRotation.GetUpVector());
    }
    
    Owner.UpdateProbeContacts();
    
    Owner.ApplyClimbTransform(FinalRotation);
    if (bAnchorAdvancedThisTick)
    {
        Owner.CaptureAnchorRotationOffset();
    }
}

void FACFFreeClimbState_Climb::StartClimbExit()
{
    UE_LOG(LogACFFreeClimb, Log, TEXT("Surface became walkable."));

    // The ClimbExit state is montage driven and aborts to Idle when the montage is missing, which
    // drops the character. FinishToWalk reaches the same end result - upright and walking on the
    // surface - without an animation, so use it as the fallback rather than falling.
    if (!Owner.ACFFreeClimbAnimInstance || !Owner.ACFFreeClimbAnimInstance->GetClimbExitMontage())
    {
        UE_LOG(LogACFFreeClimb, Warning,
            TEXT("No ClimbExitMontage available for %s: walking off the surface without the exit animation."),
            *GetNameSafe(Owner.OwnerCharacter));
        FinishToWalk();
        return;
    }

    Owner.TransitionTo(EACFFreeClimbState::ClimbExit);
}

void FACFFreeClimbState_Climb::FinishToWalk()
{
    Owner.OwnerCharacter->SetActorRotation(
        FRotator(0.0f, Owner.OwnerCharacter->GetActorRotation().Yaw, 0.0f));

    Owner.bPendingExitToWalking = true;
    Owner.TransitionTo(EACFFreeClimbState::Idle);

    if (Owner.WalkEnterInputLockDuration > 0.0f)
    {
        if (AController* Controller = Owner.OwnerCharacter->GetController())
        {
            Controller->SetIgnoreMoveInput(true);

            FTimerHandle UnlockTimer;
            Owner.GetWorld()->GetTimerManager().SetTimer(
                UnlockTimer,
                [WeakController = TWeakObjectPtr<AController>(Controller)]()
                {
                    if (WeakController.IsValid())
                    {
                        WeakController->SetIgnoreMoveInput(false);
                    }
                },
                Owner.WalkEnterInputLockDuration,
                false);
        }
    }
}

bool UACFFreeClimbComponent::CanStartClimbUp(FACFSurfaceHitResult& OutHit) const
{
    const UACFFreeClimbComponent& Owner = *this;
    const UCapsuleComponent* Capsule = Owner.CapsuleComponent;
    if (!Owner.bAnchorValid) return false;

    // Climb-up is montage driven. Without a UACFFreeClimbAnimInstance exposing a ClimbUpMontage the
    // state aborts straight to Idle, which drops the character off the surface - so refuse the
    // transition instead and keep climbing. The character stays on the wall until the free climb
    // animation layer is in place on its AnimBP.
    if (!Owner.ACFFreeClimbAnimInstance || !Owner.ACFFreeClimbAnimInstance->GetClimbUpMontage()) return false;

    const FVector Up = Owner.OwnerCharacter->GetActorUpVector();
    const FVector Forward = Owner.OwnerCharacter->GetActorForwardVector();
    const FRotator CharacterRotation = Owner.OwnerCharacter->GetActorRotation();

    // Reject excessive character pitch or roll independently.
    if (FMath::Abs(FMath::UnwindDegrees(CharacterRotation.Pitch)) > Owner.MaxClimbUpCharacterTiltAngle) return false;
    if (FMath::Abs(FMath::UnwindDegrees(CharacterRotation.Roll)) > Owner.MaxClimbUpCharacterTiltAngle) return false;

    // Find the expected grip surface.
    const float GripProbeLength = Capsule->GetScaledCapsuleRadius() * 2.0f;
    const float GripHeight = Capsule->GetScaledCapsuleHalfHeight() * Owner.ClimbUpGripHeightRatio;
    const FVector GripStart = Capsule->GetComponentLocation() + Up * GripHeight;
    const FVector GripEnd = GripStart + Forward * GripProbeLength;

    if (!Owner.QuerySurfaceSegment(GripStart, GripEnd, OutHit)) return false;

    if (Owner.bDrawDebug)
    {
        DrawDebugBox(Owner.GetWorld(), OutHit.Position, FVector::OneVector * 5.0f, FColor::Yellow, false, -1.0f, SDPG_Foreground);
    }

    // Find a nearby walkable surface.
    const FVector ProbeStart = OutHit.Position + Up * Owner.ClimbUpWalkableProbeDistance;
    const FVector ProbeCorner = ProbeStart + Forward * Owner.ClimbUpWalkableProbeDistance;
    const FVector ProbeEnd = ProbeCorner - Up * Owner.ClimbUpWalkableProbeDistance * 2.0f;

    FACFSurfaceHitResult UpperHit;
    FACFSurfaceHitResult InnerHit;
    const bool bUpperHit = Owner.QuerySurfaceSegment(ProbeStart, ProbeCorner, UpperHit);
    const bool bInnerHit = Owner.QuerySurfaceSegment(ProbeCorner, ProbeEnd, InnerHit);

    const float WalkableFloorZ = Owner.MovementComponent->GetWalkableFloorZ();
    const bool bUpperWalkable = bUpperHit && FVector::DotProduct(UpperHit.Normal.GetSafeNormal(), FVector::UpVector) >= WalkableFloorZ;
    const bool bInnerWalkable = bInnerHit && FVector::DotProduct(InnerHit.Normal.GetSafeNormal(), FVector::UpVector) >= WalkableFloorZ;

    if (Owner.bDrawDebug)
    {
        if (bUpperHit) DrawDebugBox(Owner.GetWorld(), UpperHit.Position, FVector::OneVector * 2.0f, FColor::Red, false, -1.0f, SDPG_Foreground);
        if (bInnerHit) DrawDebugBox(Owner.GetWorld(), InnerHit.Position, FVector::OneVector * 2.0f, FColor::Red, false, -1.0f, SDPG_Foreground);
    }

    if (bUpperHit && !bUpperWalkable) return false;
    if (!bUpperHit && !bInnerWalkable) return false;

    // Check clearance from the grip anchor.
    const float ClearanceHeight = 2.0f * Capsule->GetScaledCapsuleHalfHeight() - Capsule->GetScaledCapsuleHalfHeight_WithoutHemisphere();
    const FVector ClearanceStart = Capsule->GetComponentLocation() + Up * ClearanceHeight;
    const float ClearanceLength = Capsule->GetScaledCapsuleRadius() * Owner.ClimbUpClearanceLengthScale;
    const FVector ClearanceEnd = ClearanceStart + Forward * ClearanceLength;

    if (Owner.bDrawDebug)
    {
        DrawDebugLine(Owner.GetWorld(), ClearanceStart, ClearanceEnd, FColor::Magenta, false, 1.0f);
    }

    FACFSurfaceHitResult ClearanceHit;
    return !Owner.QuerySurfaceSegment(ClearanceStart, ClearanceEnd, ClearanceHit);
}

void FACFFreeClimbState_Climb::OnExit()
{
    RemoteSnapshotBuffer.Reset();
    
    Owner.RemoteVisualMoveInput = FVector2D::ZeroVector;
    
    const EACFFreeClimbState Target = Owner.GetTransitionTarget();
    if (Target == EACFFreeClimbState::Idle ||
        Target == EACFFreeClimbState::ClimbExit)
    {
        Owner.ExitClimbState();
    }
}

bool FACFFreeClimbState_Climb::AdvanceAnchor(const FVector& MoveDir, float StepDistance, FVector& OutMoveDir)
{
    FACFFreeClimbSurface& CurrentSurface = Owner.CurrentSurface;

    const FVector OldPoint = CurrentSurface.Point;
    const FVector OldNormal = CurrentSurface.RawNormal.GetSafeNormal();
    const FVector OldMoveDir = FVector::VectorPlaneProject(MoveDir, OldNormal).GetSafeNormal();

    if (OldMoveDir.IsNearlyZero() || StepDistance <= KINDA_SMALL_NUMBER)
        return false;

    const bool bStatic = Owner.AnchorSource == EACFSurfaceSource::Static;

    FACFSurfaceHitResult Hit;
    bool bResolved = false;

    if (bStatic)
        bResolved = TryStaticConcaveCorner(OldPoint, OldNormal, OldMoveDir, StepDistance, Hit, OutMoveDir);

    if (!bResolved)
        bResolved = TryDirectAdvance(OldPoint, OldNormal, OldMoveDir, StepDistance, Hit, OutMoveDir);

    if (!bResolved)
        bResolved = TrySkeletalCorner(OldPoint, OldNormal, OldMoveDir, StepDistance, Hit, OutMoveDir);

    if (!bResolved && bStatic)
        bResolved = TryStaticConvexCorner(OldPoint, OldNormal, OldMoveDir, StepDistance, Hit, OutMoveDir);

    if (!bResolved)
        return false;

    Owner.SetAnchorFromHit(Hit);

    FVector NewPoint;
    FVector NewNormal;
    if (!Owner.ResolveAnchorWorld(NewPoint, NewNormal)) return false;

    CurrentSurface.Point = NewPoint;
    CurrentSurface.RawNormal = NewNormal.GetSafeNormal();

    Owner.UpdatePoseTargetNormal(
        NewPoint,
        CurrentSurface.RawNormal,
        OutMoveDir,
        Hit.Climbable);

    Owner.BuildSurfaceBasis(CurrentSurface);
    return true;
}

bool FACFFreeClimbState_Climb::TryDirectAdvance(const FVector& OldPoint, const FVector& OldNormal, const FVector& OldMoveDir,
    float StepDistance, FACFSurfaceHitResult& OutHit, FVector& OutMoveDir)
{
    const FVector Predicted = OldPoint + OldMoveDir * StepDistance;

    FACFSurfaceHitResult DirectHit;
    if (!Owner.QuerySurfaceAt(Predicted, OldNormal, DirectHit))
        return false;

    const FVector DirectNormal = DirectHit.Normal.GetSafeNormal();

    const float ForwardProgress = FVector::DotProduct(DirectHit.Position - OldPoint, OldMoveDir);
    const bool bMadeProgress = ForwardProgress >= StepDistance * Owner.MinDirectProgressRatio;

    bool bSurfaceChanged = DirectHit.Source != Owner.AnchorSource;
    if (!bSurfaceChanged && DirectHit.Source == EACFSurfaceSource::Skeletal)
    {
        bSurfaceChanged =
            DirectHit.Climbable != Owner.AnchorClimbable ||
            DirectHit.TriangleIndex != Owner.AnchorTriangleIndex;
    }
    else if (!bSurfaceChanged)
    {
        bSurfaceChanged = DirectHit.HitComponent != Owner.AnchorPrimitive.Get();
    }

    constexpr float MinMeaningfulNormalTurnDegrees = 5.0f;
    const float NormalTurnDegrees = FMath::RadiansToDegrees(
        FMath::Acos(FMath::Clamp(FVector::DotProduct(OldNormal, DirectNormal), -1.0f, 1.0f)));
    const bool bPivotedToNewSurface = bSurfaceChanged && NormalTurnDegrees >= MinMeaningfulNormalTurnDegrees;

    if (!bMadeProgress && !bPivotedToNewSurface)
        return false;

    OutHit = DirectHit;
    OutMoveDir = FVector::VectorPlaneProject(
        FQuat::FindBetweenNormals(OldNormal, DirectNormal).RotateVector(OldMoveDir), DirectNormal).GetSafeNormal();
    return true;
}

bool FACFFreeClimbState_Climb::TrySkeletalCorner(const FVector& OldPoint, const FVector& OldNormal, const FVector& OldMoveDir,
    float StepDistance, FACFSurfaceHitResult& OutHit, FVector& OutTransportedMoveDir)
{
    const FVector Predicted = OldPoint + OldMoveDir * StepDistance;

    TArray<FACFSurfaceHitResult> Candidates;
    if (!Owner.QueryCornerCandidates(OldPoint, Predicted, Candidates))
        return false;

    bool bFound = false;
    float BestScore = -BIG_NUMBER;
    const float MinNormalDot = FMath::Cos(FMath::DegreesToRadians(Owner.MaxCornerTurnDegrees));

    const auto IsSameSurface = [](const FACFSurfaceHitResult& A, const FACFSurfaceHitResult& B)
    {
        if (A.Source != B.Source)
            return false;

        if (A.Source == EACFSurfaceSource::Skeletal)
        {
            return A.Climbable == B.Climbable &&
                A.TriangleIndex == B.TriangleIndex;
        }

        return A.HitComponent != nullptr &&
            A.HitComponent == B.HitComponent;
    };

    for (const FACFSurfaceHitResult& Candidate : Candidates)
    {
        if (!Candidate.IsValidSurface())
            continue;

        const FVector CandidateNormal = Candidate.Normal.GetSafeNormal();
        if (CandidateNormal.IsNearlyZero())
            continue;

        const float NormalDot = FVector::DotProduct(OldNormal, CandidateNormal);

        if (NormalDot < MinNormalDot)
            continue;

        const FVector ToCandidate = Candidate.Position - OldPoint;
        const float ForwardProgress = FVector::DotProduct(ToCandidate, OldMoveDir);

        if (ForwardProgress < -Owner.CornerBacktrackTolerance)
            continue;

        const FVector CandidateMoveDir = FVector::VectorPlaneProject(
            FQuat::FindBetweenNormals(OldNormal, CandidateNormal).RotateVector(OldMoveDir), CandidateNormal).GetSafeNormal();

        if (CandidateMoveDir.IsNearlyZero())
            continue;

        const float DistanceToCorner = FMath::Clamp(FVector::DotProduct(Candidate.Position - OldPoint, OldMoveDir), 0.0f, StepDistance);
        const float RemainingDistance = StepDistance - DistanceToCorner;

        const float ValidationDistance = FMath::Max(RemainingDistance, Owner.CornerProbeReach);
        const FVector CandidatePredicted = Candidate.Position + CandidateMoveDir * ValidationDistance;

        FACFSurfaceHitResult ConfirmedHit;

        if (!Owner.QuerySurfaceAt(CandidatePredicted, CandidateNormal, ConfirmedHit))
            continue;

        if (!ConfirmedHit.IsValidSurface() || !IsSameSurface(ConfirmedHit, Candidate))
            continue;

        const float ConfirmedNormalDot = FVector::DotProduct(CandidateNormal, ConfirmedHit.Normal.GetSafeNormal());

        if (ConfirmedNormalDot < 0.8f)
            continue;

        const float LookAheadProgress = FVector::DotProduct(
            ConfirmedHit.Position - Candidate.Position, CandidateMoveDir);
        const float RequiredLookAheadProgress = ValidationDistance * Owner.MinDirectProgressRatio;

        if (LookAheadProgress < RequiredLookAheadProgress)
        {
            continue;
        }

        FACFSurfaceHitResult FinalHit;
        if (RemainingDistance <= KINDA_SMALL_NUMBER)
        {
            FinalHit = Candidate;
        }
        else if (FMath::IsNearlyEqual(RemainingDistance, ValidationDistance))
        {
            FinalHit = ConfirmedHit;
        }
        else
        {
            const FVector FinalPoint = Candidate.Position + CandidateMoveDir * RemainingDistance;
            if (!Owner.QuerySurfaceAt(FinalPoint, CandidateNormal, FinalHit))
                continue;

            if (!FinalHit.IsValidSurface() || !IsSameSurface(FinalHit, Candidate))
                continue;

            const float FinalNormalDot = FVector::DotProduct(CandidateNormal, FinalHit.Normal.GetSafeNormal());
            if (FinalNormalDot < 0.8f)
                continue;
        }

        const FVector ExpectedFinalPoint = Candidate.Position + CandidateMoveDir * RemainingDistance;
        const float PredictedError = FVector::Distance(FinalHit.Position, ExpectedFinalPoint);

        const float TurnPenalty = 1.0f - NormalDot;

        float Score = 0.0f;

        Score += ForwardProgress * 2.0f;
        Score -= PredictedError * 0.5f;
        Score -= TurnPenalty * 20.0f;

        if (Score <= BestScore)
            continue;

        BestScore = Score;
        OutHit = FinalHit;
        OutTransportedMoveDir = CandidateMoveDir;
        bFound = true;
    }

    return bFound;
}

bool FACFFreeClimbState_Climb::TryStaticConcaveCorner(const FVector& OldPoint, const FVector& OldNormal, const FVector& OldMoveDir,
    float StepDistance, FACFSurfaceHitResult& OutHit, FVector& OutTransportedMoveDir)
{
    if (StepDistance <= KINDA_SMALL_NUMBER)
        return false;

    const FVector ConcaveStart = OldPoint + OldNormal * 2.0f;
    const FVector ConcaveEnd = ConcaveStart + OldMoveDir * (StepDistance + Owner.CornerProbeReach);

    FACFSurfaceHitResult ConcaveHit;
    if (!Owner.QueryStaticSegment(ConcaveStart, ConcaveEnd, ConcaveHit))
        return false;

    const FVector HitNormal = ConcaveHit.Normal.GetSafeNormal();

    if (FVector::DotProduct(HitNormal, OldMoveDir) >= -Owner.StaticConcaveWallFacingDot)
        return false;
    if (FVector::DotProduct(HitNormal, OldNormal) >= 0.98f)
        return false;
    if (FMath::Abs(FVector::DotProduct(HitNormal, FVector::UpVector)) > FMath::Sin(FMath::DegreesToRadians(Owner.MaxClimbAngle)))
        return false;

    OutTransportedMoveDir = FVector::VectorPlaneProject(
        FQuat::FindBetweenNormals(OldNormal, HitNormal).RotateVector(OldMoveDir), HitNormal).GetSafeNormal();
    if (OutTransportedMoveDir.IsNearlyZero())
        return false;

    const float DistanceToSeam = FMath::Clamp(FVector::DotProduct(ConcaveHit.Position - OldPoint, OldMoveDir), 0.0f, StepDistance);

    OutHit = ConcaveHit;
    OutHit.Position = ConcaveHit.Position + OutTransportedMoveDir * (StepDistance - DistanceToSeam);

    return true;
}

bool FACFFreeClimbState_Climb::TryStaticConvexCorner(const FVector& OldPoint, const FVector& OldNormal, const FVector& OldMoveDir,
    float StepDistance, FACFSurfaceHitResult& OutHit, FVector& OutTransportedMoveDir)
{
    const FVector Axis = FVector::CrossProduct(OldNormal, OldMoveDir).GetSafeNormal();
    if (Axis.IsNearlyZero() || StepDistance <= KINDA_SMALL_NUMBER)
        return false;

    const float MaxUpDot = FMath::Sin(FMath::DegreesToRadians(Owner.MaxClimbAngle));

    for (float Angle = Owner.StaticCornerWrapAngleStep; Angle <= Owner.MaxCornerTurnDegrees; Angle += Owner.StaticCornerWrapAngleStep)
    {
        const FQuat Wrap(Axis, FMath::DegreesToRadians(Angle));
        const FVector Normal = Wrap.RotateVector(OldNormal);
        const FVector ProbePoint = OldPoint + Wrap.RotateVector(OldMoveDir) * Owner.CornerProbeReach;

        FACFSurfaceHitResult Hit;
        if (!Owner.QueryStaticSegment(ProbePoint + Normal * Owner.SurfaceProbeStartOffset, ProbePoint - Normal * Owner.SurfaceProbeDistance, Hit))
            continue;

        const FVector HitNormal = Hit.Normal.GetSafeNormal();
        if (FVector::DotProduct(HitNormal, OldNormal) > 0.98f)
            continue;
        if (FMath::Abs(FVector::DotProduct(HitNormal, FVector::UpVector)) > MaxUpDot)
            continue;

        OutTransportedMoveDir = FVector::VectorPlaneProject(
            FQuat::FindBetweenNormals(OldNormal, HitNormal).RotateVector(OldMoveDir), HitNormal).GetSafeNormal();
        if (OutTransportedMoveDir.IsNearlyZero())
            continue;

        OutHit = Hit;
        return true;
    }

    return false;
}
