// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "UpgradeLevelTable.generated.h"

class UUpgradeMilestoneData;

USTRUCT(BlueprintType)
struct FUpgradeLevelEntry
{
	GENERATED_BODY()

	/** Level number (1, 2, 3, ...) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 Level = 1;

	/** Milestones to unlock at this level */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<UUpgradeMilestoneData*> Milestones;
};
/**
 * 
 */
UCLASS()
class PANGEABASEUPGRADESYSTEM_API UUpgradeLevelTable : public UPrimaryDataAsset
{
	GENERATED_BODY()
	
public:

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FUpgradeLevelEntry> LevelEntries;

	void GetMilestonesForLevel(int32 InLevel, TArray<UUpgradeMilestoneData*>& OutMilestones) const;
};
