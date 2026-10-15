// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "States/ACFFreeClimbState.h"
#include "ACFFreeClimbNetworkRuntime.h"
#include "ACFFreeClimbTypes.h"
#include "Engine/EngineTypes.h"

class UACFFreeClimbableComponent;

class FACFFreeClimbState_Cling final : public IACFFreeClimbState
{
private:
    struct FRemoteSnapshot
    {
        EACFSurfaceSource Source = EACFSurfaceSource::Skeletal;

        TWeakObjectPtr<UACFFreeClimbableComponent> Climbable;
        int32 AnchorTriangleIndex = INDEX_NONE;
        FVector AnchorBarycentric = FVector::ZeroVector;

        TWeakObjectPtr<UPrimitiveComponent> Primitive;
        FVector LocalPosition = FVector::ZeroVector;
        FVector LocalNormal = FVector::ForwardVector;

        FVector AnchorRelativeLocation = FVector::ZeroVector;
        FVector2D MoveInput = FVector2D::ZeroVector;
    };
    
public:
    using IACFFreeClimbState::IACFFreeClimbState;

    EACFFreeClimbState GetId() const override { return EACFFreeClimbState::Cling; }
    
    void FillRepState(FACFFreeClimbRepState& State) const override;

    void OnEnter() override;

    void Tick(float DeltaTime) override;
    
    void ApplyRemoteState(const FACFFreeClimbRepState& State) override;
    
    void TickRemote(float DeltaTime) override;

    void OnExit() override;

private:
    void UpdateMovement(float DeltaTime);
    void ResolveCollision(float DeltaTime);
    void TryPinHands();

    static bool ResolveSnapshot(const FRemoteSnapshot& Snapshot, FVector& OutPoint, FVector& OutNormal);

    FACFClingState Cling;
    ECollisionEnabled::Type PreviousCapsuleCollision = ECollisionEnabled::QueryAndPhysics;
    ECollisionEnabled::Type PreviousMeshCollision = ECollisionEnabled::NoCollision;
    bool bResourcesActive = false;
    bool bHandsPinned = false;

    ACFFreeClimbNetwork::TRemoteSnapshotBuffer<FRemoteSnapshot> RemoteSnapshotBuffer;
};
