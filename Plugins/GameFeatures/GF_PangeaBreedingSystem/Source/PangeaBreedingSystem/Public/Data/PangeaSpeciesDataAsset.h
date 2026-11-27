// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

#include "Definitions/PangeaBreedingFragment.h"
#include "Types/BreedingTypes.h"
#include "PangeaSpeciesDataAsset.generated.h"

/**
 * Standalone per-species breeding data asset. The breeding C++ in this module reads its configuration
 * from UPangeaBreedingFragment on the creature definition and does not reference this asset type.
 */
UCLASS()
class PANGEABREEDINGSYSTEM_API UPangeaSpeciesDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Identifier of the species this asset describes. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName SpeciesID = NAME_None;

	/** Actor class of creatures of this species. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSubclassOf<AActor> CreatureClass;

	/** Incubation timing of eggs of this species. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FIncubationConfig Incubation;

	/** Fertility cooldown settings of this species. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FFertilityConfig Fertility;

	// Whether offspring blend material colors between parents
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Breeding|Appearance")
	bool bInheritParentMaterials = true;

	/** Skeletal mesh material slot that carries the inheritable parameters. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance")
	int32 InheritanceMaterialSlot = 1;

	/** Probability that an inherited color receives a random mutation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
	float VisualMutationChance = 0.25f;

	/** Exponent that biases the blend between the two parents' colors. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
	float ParentBiasPower = 0.8f;

	/** Strength of a visual mutation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
	float VisualMutationIntensity = 0.1f;

	/** Color parameter groups that offspring inherit from their parents. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Breeding|Appearance")
	TArray<FMaterialColorGeneticGroup> MaterialGeneticGroups;
};
