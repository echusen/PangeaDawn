// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/DataAsset.h"
#include "UpgradeMilestoneData.generated.h"

class UUpgradeRequirement;
class UUpgradeAction;
/**
 * 
 */
UCLASS(BlueprintType)
class PANGEABASEUPGRADESYSTEM_API UUpgradeMilestoneData : public UPrimaryDataAsset
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade")
	FGameplayTag MilestoneTag;

	/** Designer-assigned display name */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade")
	FText DisplayName;

	/** Requirements that must be met before actions run. Instanced to allow per-milestone config. */
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category="Upgrade")
	TArray<UUpgradeRequirement*> Requirements;

	/** Actions executed when milestone unlocks */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade")
	TArray<UUpgradeAction*> Actions;
};
