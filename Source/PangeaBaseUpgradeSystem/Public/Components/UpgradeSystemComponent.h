// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Components/ActorComponent.h"
#include "UpgradeSystemComponent.generated.h"


class UUpgradeLevelTable;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PANGEABASEUPGRADESYSTEM_API UUpgradeSystemComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UUpgradeSystemComponent();

	/** Level table data asset */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade")
	UUpgradeLevelTable* LevelTable;

	/** Current level (local cached) - in practice this may be driven by GAS attributes */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Upgrade")
	int32 CurrentLevel = 1;

	/** Call to trigger evaluation for a new level (e.g., called when GAS attribute changes) */
	UFUNCTION(BlueprintCallable, Category="Upgrade")
	void OnLevelIncreased(int32 NewLevel, UObject* PlayerContext);
	
	/** All milestone tags that this base has completed */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Upgrade")
	FGameplayTagContainer CompletedMilestones;
	
	bool IsMilestoneCompleted(FGameplayTag MilestoneTag) const;

	void MarkMilestoneCompleted(FGameplayTag MilestoneTag);
	
	UFUNCTION(BlueprintCallable, Category="Upgrade")
	bool CanUpgradeToNextLevel(UObject* PlayerContext) const;

protected:
	virtual void BeginPlay() override;

private:
	void ExecuteMilestonesForLevel(int32 Level, UObject* PlayerContext);
};
