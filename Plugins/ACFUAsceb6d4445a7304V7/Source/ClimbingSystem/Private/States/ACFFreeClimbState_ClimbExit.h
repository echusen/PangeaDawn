// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once
#include "Engine/EngineTypes.h"
#include "States/ACFFreeClimbState.h"

class UAnimMontage;

class FACFFreeClimbState_ClimbExit final : public IACFFreeClimbState
{
public:
    using IACFFreeClimbState::IACFFreeClimbState;

    EACFFreeClimbState GetId() const override { return EACFFreeClimbState::ClimbExit; }

    void OnEnter() override;

    void Tick(float DeltaTime) override;
    
    void ApplyRemoteState(const FACFFreeClimbRepState& State) override;
    
    void TickRemote(float DeltaTime) override;

    void OnExit() override;

    void HandleMontageEnded(UAnimMontage* Montage, bool bInterrupted);

private:
    bool PlayMontage(float StartPosition);

    void UpdateRotation(float DeltaTime);

    void Finish();

    bool ResolveExitFloor();

    FVector PreviousAnchorWorldPoint = FVector::ZeroVector;
    bool bAnchorFollowActive = false;
    bool bMontagePlaying = false;
    ECollisionEnabled::Type SavedCapsuleCollisionEnabled = ECollisionEnabled::NoCollision;

    FRotator InitialRotation = FRotator::ZeroRotator;
    float InitialUprightDot = 1.0f;
    float RotationBlendElapsed = 0.0f;
    float AppliedRise = 0.0f;
};
