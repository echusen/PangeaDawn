// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once
#include "Engine/EngineTypes.h"
#include "Animation/AnimInstance.h"
#include "MotionWarpingComponent.h"
#include "States/ACFFreeClimbState.h"

class FACFFreeClimbState_ClimbUp final : public IACFFreeClimbState
{
public:
    using IACFFreeClimbState::IACFFreeClimbState;

    EACFFreeClimbState GetId() const override { return EACFFreeClimbState::ClimbUp; }

    void OnEnter() override;
    void Tick(float DeltaTime) override;
    void ApplyRemoteState(const FACFFreeClimbRepState& State) override;
    void TickRemote(float DeltaTime) override;
    void OnExit() override;
    void HandleMontageNotify(FName NotifyName);

private:
    bool Update();
    TWeakObjectPtr<UMotionWarpingComponent> MotionWarpingComponent;
    TWeakObjectPtr<URootMotionModifier> RuntimeWarpModifier;
    TArray<FMotionWarpingTarget> SavedWarpTargets;
    TWeakObjectPtr<UAnimInstance> SavedAnimInstance;
    ERootMotionMode::Type SavedRootMotionMode = ERootMotionMode::RootMotionFromMontagesOnly;
    FVector TargetOffset = FVector::ZeroVector;
    FVector PreviousAnchorWorldPoint = FVector::ZeroVector;
    ECollisionEnabled::Type SavedCapsuleCollisionEnabled = ECollisionEnabled::ProbeOnly;
};
