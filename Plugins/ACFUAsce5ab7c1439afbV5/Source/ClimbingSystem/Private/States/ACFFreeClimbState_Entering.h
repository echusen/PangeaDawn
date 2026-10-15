// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "States/ACFFreeClimbState.h"

class FACFFreeClimbState_Entering final : public IACFFreeClimbState
{
public:
    using IACFFreeClimbState::IACFFreeClimbState;

    EACFFreeClimbState GetId() const override { return EACFFreeClimbState::Entering; }

    void OnEnter() override;

    void Tick(float DeltaTime) override;

    void ApplyRemoteState(const FACFFreeClimbRepState& State) override;
    
    void TickRemote(float DeltaTime) override;
    
    void OnExit() override;

private:
    void ApplyTransform(float Alpha);
    
private:
    float ElapsedTime = 0.0f;
    double RemoteStateStartServerTimeSeconds = 0.0;
    FTransform StartTransform = FTransform::Identity;
    FTransform TargetTransform = FTransform::Identity;
};
