// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "ACFFreeClimbStateTypes.h"

struct FACFFreeClimbRepState;
class UACFFreeClimbComponent;

class IACFFreeClimbState
{
public:
    explicit IACFFreeClimbState(UACFFreeClimbComponent& InOwner)
        : Owner(InOwner)
    {
    }

    virtual ~IACFFreeClimbState() = default;

    virtual EACFFreeClimbState GetId() const = 0;

    virtual void FillRepState(FACFFreeClimbRepState& State) const {}

    virtual void OnEnter() {}

    virtual void Tick(float DeltaTime) = 0;

    virtual void TickRemote(float DeltaTime) {}

    virtual void ApplyRemoteState(const FACFFreeClimbRepState& State) {}

    virtual void StartJumpMovement() {}

    virtual void OnExit() {}

protected:
    UACFFreeClimbComponent& Owner;
};
