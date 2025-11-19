// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/DataAsset.h"
#include "VillageDefinitionData.generated.h"


class UUpgradeAction;
class UUpgradeRequirement;

USTRUCT(BlueprintType)
struct FFacilityGroupReference
{
	GENERATED_BODY();

	UPROPERTY(EditAnywhere)
	FGameplayTag FacilityTag;
};

USTRUCT(BlueprintType)
struct FUpgradeMilestoneDefinition
{
	GENERATED_BODY();

	UPROPERTY(EditAnywhere)
	FGameplayTag MilestoneTag;

	UPROPERTY(EditAnywhere)
	TArray<TObjectPtr<UUpgradeRequirement>> Requirements;

	UPROPERTY(EditAnywhere)
	TArray<TObjectPtr<UUpgradeAction>> Actions;
};

USTRUCT(BlueprintType)
struct FUpgradeLevelDefinition
{
	GENERATED_BODY();

	UPROPERTY(EditAnywhere)
	int32 Level;

	UPROPERTY(EditAnywhere)
	TArray<FUpgradeMilestoneDefinition> Milestones;
};

UCLASS(BlueprintType)
class PANGEABASEUPGRADESYSTEM_API UVillageDefinitionData : public UPrimaryDataAsset
{
	GENERATED_BODY()
	
public:

	UPROPERTY(EditAnywhere, Category="Village")
	FGameplayTag VillageTag;

	UPROPERTY(EditAnywhere, Category="Village")
	TArray<FFacilityGroupReference> FacilityGroups;

	UPROPERTY(EditAnywhere, Category="Upgrade System")
	TArray<FUpgradeLevelDefinition> Levels;
};
