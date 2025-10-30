// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PangeaBreedingSystemTypes.h"
#include "Components/PangeaBreedableComponent.h"
#include "Engine/DataAsset.h"
#include "PangeaSpeciesDataAsset.generated.h"

USTRUCT(BlueprintType)
struct FMaterialGeneticGroup
{
    GENERATED_BODY()

    // Category name (e.g., "Pattern Color", "Skin Color")
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName GroupName;

    // The specific material parameter names to blend
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<FName> ParameterNames;
};

/**
 * 
 */
UCLASS()
class PANGEABREEDINGSYSTEM_API UPangeaSpeciesDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()
	
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName SpeciesID = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TSubclassOf<AActor> CreatureClass;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FIncubationConfig Incubation;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FFertilityConfig Fertility;

    // Whether offspring blend material colors between parents
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Breeding|Appearance")
    bool bInheritParentMaterials = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance")
    int32 InheritanceMaterialSlot = 1;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance")
    float VisualMutationChance = 0.25f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance")
    float ParentBiasPower = 0.8f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance")
    float VisualMutationIntensity = 0.1f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Breeding|Appearance")
    TArray<FMaterialGeneticGroup> MaterialGeneticGroups;
};
