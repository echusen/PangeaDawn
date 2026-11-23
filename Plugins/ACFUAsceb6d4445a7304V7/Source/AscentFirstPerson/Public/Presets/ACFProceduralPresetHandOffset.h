// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ACFProceduralPreset.h"
#include "Types/ACFFirstPersonTypes.h"
#include "ACFProceduralPresetHandOffset.generated.h"

/**
 * Offsets for the Procedural Hand Offset node while this preset data is active, e.g. per held item.
 *
 * No motion component reads it: the first person component exposes it as GetHandOffsetData, and the ACF anim
 * instance passes that on to the node's Hand Offset Data pin. Preset data without one gives zero offsets.
 */
UCLASS(meta=(DisplayName="Preset: Hand Offset"))
class ASCENTFIRSTPERSON_API UACFProceduralPresetHandOffset : public UACFProceduralPreset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hand Offset")
	FACFProceduralHandOffsetData HandOffsetData;
};
