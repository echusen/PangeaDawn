// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "States/ACFFreeClimbState.h"

class FACFFreeClimbState_Idle final : public IACFFreeClimbState
{
public:
    using IACFFreeClimbState::IACFFreeClimbState;

    EACFFreeClimbState GetId() const override { return EACFFreeClimbState::Idle; }

    void FillRepState(FACFFreeClimbRepState& State) const override;

    void Tick(float) override {}
    
    void ApplyRemoteState(const FACFFreeClimbRepState& State) override;
};
