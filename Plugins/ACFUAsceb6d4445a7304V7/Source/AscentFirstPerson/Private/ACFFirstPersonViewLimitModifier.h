// Copyright (C) Dark Tower Interactive SRL. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraModifier.h"
#include "ACFFirstPersonViewLimitModifier.generated.h"

/** Internal camera hook installed and removed by the first-person component. No custom controller required. */
UCLASS(NotBlueprintable)
class UACFFirstPersonViewLimitModifier : public UCameraModifier
{
	GENERATED_BODY()
public:
	UACFFirstPersonViewLimitModifier();
	virtual bool ProcessViewRotation(AActor* ViewTarget, float DeltaTime, FRotator& OutViewRotation, FRotator& OutDeltaRot) override;
};
