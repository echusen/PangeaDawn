// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PangeaBreedingSystemTypes.generated.h"

USTRUCT(BlueprintType)
struct FGeneticTrait
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName Name = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Value = 0.f;
};

USTRUCT(BlueprintType)
struct FGeneticTraitSet
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FGeneticTrait> Traits;

    float GetValue(FName InName, float Default = 0.f) const
    {
        if (const FGeneticTrait* Found = Traits.FindByPredicate([&](const FGeneticTrait& T){ return T.Name == InName; }))
        {
            return Found->Value;
        }
        return Default;
    }

    void SetValue(FName InName, float InValue)
    {
        if (FGeneticTrait* Found = Traits.FindByPredicate([&](const FGeneticTrait& T){ return T.Name == InName; }))
        {
            Found->Value = InValue;
        }
        else
        {
            Traits.Add({InName, InValue});
        }
    }
};

USTRUCT(BlueprintType)
struct FParentSnapshot
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName SpeciesID = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FGuid CreatureId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FGeneticTraitSet Traits;

    UPROPERTY()
    TSoftObjectPtr<AActor> ParentActor;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TMap<FName, FLinearColor> VisualData;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TMap<FName, FLinearColor> MaterialParams;
};

USTRUCT(BlueprintType)
struct FIncubationConfig
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float IncubationSeconds = 120.f;
};

USTRUCT(BlueprintType)
struct FFertilityConfig
{
    GENERATED_BODY()

    // Minimum time before a creature can breed again
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Breeding")
    float FertilityCooldownSeconds = 120.0f;

    // Whether both parents go on cooldown or only female
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Breeding")
    bool bAffectsBothParents = true;
};

