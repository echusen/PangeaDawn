// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ACFProceduralPreset.h"
#include "Engine/CollisionProfile.h"
#include "Types/ACFFirstPersonItemTypes.h"
#include "ACFProceduralPresetBlockingMotion.generated.h"

UCLASS(meta=(DisplayName="Preset: Blocking Motion"))
class ASCENTFIRSTPERSON_API UACFProceduralPresetBlockingMotion : public UACFProceduralPreset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Blocking")
	TEnumAsByte<enum ECollisionChannel> TraceChannel = ECC_Visibility;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Blocking")
	FVector BlockingOffsetLocation = FVector(0.0f, -5.0f, -5.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Blocking")
	FRotator BlockingOffsetRotation = FRotator(-25.0f, -75.0f, 0.0f);

	/**
	 * Where the held item ends, usually its muzzle or tip, so the sweep reaches past it instead of past the hand.
	 * Resolved by the first person component. Unset uses the item's own default, such as a ranged weapon's muzzle.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Blocking")
	FACFFirstPersonItemPoint CollisionPoint;

};
