// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once
#include "States/ACFFreeClimbState.h"
#include "ACFFreeClimbNetworkRuntime.h"
#include "ACFFreeClimbTypes.h"

class FACFFreeClimbState_Climb final : public IACFFreeClimbState
{
private:
    struct FRemoteSnapshot
    {
        EACFSurfaceSource Source = EACFSurfaceSource::Skeletal;

        TWeakObjectPtr<UACFFreeClimbableComponent> Climbable;
        int32 AnchorTriangleIndex = INDEX_NONE;
        FVector AnchorBarycentric = FVector::ZeroVector;

        TWeakObjectPtr<UPrimitiveComponent> Primitive;
        FVector LocalPosition;
        FVector LocalNormal;
        
        FVector2D MoveInput = FVector2D::ZeroVector;
        
        FQuat Rotation = FQuat::Identity;
    };
    
public:
    using IACFFreeClimbState::IACFFreeClimbState;

    EACFFreeClimbState GetId() const override { return EACFFreeClimbState::Climb; }

    void FillRepState(FACFFreeClimbRepState& State) const override;
    
    void OnEnter() override;

    void Tick(float DeltaTime) override;
    
    void ApplyRemoteState(const FACFFreeClimbRepState& State) override;
    
    void TickRemote(float DeltaTime) override;

    void OnExit() override;

private:
    void UpdateMovement(float DeltaTime, FQuat& OutTrackingRotation);
    void ApplyMovementResult(const FQuat& TrackingRotation);
    void StartClimbExit();
    void FinishToWalk();

    bool AdvanceAnchor(const FVector& MoveDir, float StepDistance, FVector& OutMoveDir);
    bool TryDirectAdvance(const FVector& OldPoint, const FVector& OldNormal, const FVector& OldMoveDir,
        float StepDistance, FACFSurfaceHitResult& OutHit, FVector& OutMoveDir);
    bool TrySkeletalCorner(const FVector& OldPoint, const FVector& OldNormal, const FVector& OldMoveDir,
        float StepDistance, FACFSurfaceHitResult& OutHit, FVector& OutTransportedMoveDir);
    bool TryStaticConcaveCorner(const FVector& OldPoint, const FVector& OldNormal, const FVector& OldMoveDir,
        float StepDistance, FACFSurfaceHitResult& OutHit, FVector& OutTransportedMoveDir);
    bool TryStaticConvexCorner(const FVector& OldPoint, const FVector& OldNormal, const FVector& OldMoveDir,
        float StepDistance, FACFSurfaceHitResult& OutHit, FVector& OutTransportedMoveDir);

    static bool ResolveSnapshot(const FRemoteSnapshot& Snapshot, FVector& OutPoint, FVector& OutNormal);

    FVector PrevAnchorPoint = FVector::ZeroVector;
    float SmoothedAnchorSpeed = 0.0f;
    FVector SmoothedNormalVelocity = FVector::ZeroVector;

    bool bHasCompletedInitialMovementUpdate = false;

    bool bMoveBlocked = false;
    bool bAnchorAdvancedThisTick = false;
    
    ACFFreeClimbNetwork::TRemoteSnapshotBuffer<FRemoteSnapshot> RemoteSnapshotBuffer;
};
