// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "States/ACFFreeClimbState.h"
#include "ACFFreeClimbNetworkTypes.h"

class UACFFreeClimbableComponent;

class FACFFreeClimbState_Jump final : public IACFFreeClimbState
{
public:
    using IACFFreeClimbState::IACFFreeClimbState;

    EACFFreeClimbState GetId() const override { return EACFFreeClimbState::Jump; }

    void OnEnter() override;

    void Tick(float DeltaTime) override;
    
    void TickRemote(float DeltaTime) override;

    void ApplyRemoteJumpState(const FACFJumpRepState& State);

    void StartJumpMovement() override;
    
    void OnExit() override;

private:
    bool BuildLocalPath();
    bool BuildPathPointFromCurrentAnchor(FACFJumpPathPoint& OutPoint) const;
    bool BuildPathPointFromHit(const FACFSurfaceHitResult& Hit, FACFJumpPathPoint& OutPoint) const;
    FACFJumpRepState BuildJumpRepState();
    void UpdateJump(bool bCanInterruptToClimbUp);
    FTransform MakePathTransform(const FVector& AnchorPoint, const FVector& AnchorNormal) const;
    bool ResolvePathPoint(int32 Index, FACFSurfaceHitResult& OutHit) const;
    
    void FinishJump();
    
private:
    TArray<FACFJumpPathPoint> Path;
    
    float StateStartServerTimeSeconds = 0.0f;
    float Duration = 0.0f;

    bool bMovementStarted = false;
    float MovementStartAlpha = 0.0f;
};
