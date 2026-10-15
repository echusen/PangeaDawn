// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ACFFreeClimbStateTypes.generated.h"

UENUM(BlueprintType)
enum class EACFFreeClimbState : uint8
{
    Idle,
    Entering,
    Climb,
    Jump,
    Cling,
    ClimbUp,
    ClimbExit,
};
