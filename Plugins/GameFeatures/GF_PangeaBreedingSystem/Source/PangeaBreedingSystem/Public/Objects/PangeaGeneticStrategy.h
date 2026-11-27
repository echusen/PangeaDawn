// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"

#include "Types/BreedingTypes.h"
#include "PangeaGeneticStrategy.generated.h"

/**
 * Overridable rule for combining the genetic traits of two parents into their offspring's traits.
 * The native implementation averages each trait and adds a random mutation of up to 5 percent.
 */
UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced)
class PANGEABREEDINGSYSTEM_API UPangeaGeneticStrategy : public UObject
{
	GENERATED_BODY()
public:
	/** Returns the offspring's traits computed from both parent snapshots. */
	UFUNCTION(BlueprintNativeEvent, Category = "Pangea|BreedingSystem|Genetics")
	FGeneticTraitSet CombineTraits(const FParentSnapshot& ParentA, const FParentSnapshot& ParentB) const;
};
