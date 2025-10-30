// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Data/PangeaBreedingSystemTypes.h"
#include "UObject/NoExportTypes.h"
#include "PangeaGeneticStrategy.generated.h"

/**
 * 
 */
UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced)
class PANGEABREEDINGSYSTEM_API UPangeaGeneticStrategy : public UObject
{
	GENERATED_BODY()
public:
    UFUNCTION(BlueprintNativeEvent, Category="Pangea|BreedingSystem|Genetics")
    FGeneticTraitSet CombineTraits(const FParentSnapshot& ParentA, const FParentSnapshot& ParentB) const;
};
