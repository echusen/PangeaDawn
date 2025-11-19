// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Components/ActorComponent.h"
#include "UpgradeSystemComponent.generated.h"


class UVillageDefinitionData;
struct FUpgradeLevelDefinition;
struct FUpgradeMilestoneDefinition;
class UUpgradeRequirement;
class UUpgradeAction;

/**
 * Component that drives village/base upgrades using UVillageDefinitionData.
 * - Reads all level + milestone data from VillageDefinition
 * - Evaluates requirements
 * - Executes actions when a level increases
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PANGEABASEUPGRADESYSTEM_API UUpgradeSystemComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UUpgradeSystemComponent();

protected:
	virtual void BeginPlay() override;

public:

	/** Master data asset that defines all levels, milestones, and facilities for this village */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade")
	UVillageDefinitionData* VillageDefinition = nullptr;

	/** Current village/base level (0 = uninitialized / none) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Upgrade")
	int32 CurrentLevel = 0;

	/** Milestones we have successfully executed */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Upgrade")
	FGameplayTagContainer CompletedMilestones;

	/**
	 * Called by your leveling system when the village/base level increases.
	 * PlayerContext is typically the player pawn, controller, or anything
	 * that requirements need to inspect (inventory, quest manager, etc.).
	 */
	UFUNCTION(BlueprintCallable, Category="Upgrade")
	void OnLevelIncreased(int32 NewLevel, UObject* PlayerContext);

	/** Returns true if all requirements for CurrentLevel+1 are satisfied */
	UFUNCTION(BlueprintCallable, Category="Upgrade")
	bool CanUpgradeToNextLevel(UObject* PlayerContext) const;

	/** Check if a milestone has already been executed */
	UFUNCTION(BlueprintCallable, Category="Upgrade")
	bool IsMilestoneCompleted(FGameplayTag MilestoneTag) const;

	/** Mark a milestone as completed (used internally, but exposed for debugging if needed) */
	UFUNCTION(BlueprintCallable, Category="Upgrade")
	void MarkMilestoneCompleted(FGameplayTag MilestoneTag);
	
	//UI Helpers
	UFUNCTION(BlueprintCallable, Category="Upgrade|UI")
    bool GetNextLevelDefinition(FUpgradeLevelDefinition& OutLevel) const;
    
    UFUNCTION(BlueprintCallable, Category="Upgrade|UI")
    void GetMilestonesForLevel(int32 Level, TArray<FUpgradeMilestoneDefinition>& OutMilestones) const;
    
    UFUNCTION(BlueprintCallable, Category="Upgrade|UI")
    void GetUnmetRequirementsForNextLevel(UObject* PlayerContext, TArray<UUpgradeRequirement*>& OutRequirements) const;
    
    UFUNCTION(BlueprintCallable, Category="Upgrade|UI")
    void GetFacilitiesUnlockedAtLevel(int32 Level, TArray<FGameplayTag>& OutFacilities) const;


private:

	/** Helper: find level definition for an absolute level number */
	const FUpgradeLevelDefinition* FindLevelDefinition(int32 Level) const;

	/** Helper: execute all milestones belonging to a level (if requirements are met) */
	void ExecuteMilestonesForLevel(int32 Level, UObject* PlayerContext);
};
