// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Components/ActorComponent.h"
#include "Interfaces/PDBaseUpgradeInterface.h"
#include "Types/BaseUpgradeContext.h"
#include "Types/BaseUpgradeTypes.h"
#include "UpgradeSystemComponent.generated.h"


class UBaseFacilityCatalogFragment;
class UBaseProgressionFragment;
class UBaseUpgradeDefinition;
class UUpgradeRequirement;
class UUpgradeAction;

/**
 * Component that drives village/base upgrades from a single fragment-based definition.
 * - Reads all level + milestone data from UpgradeDefinition
 * - Evaluates requirements
 * - Executes actions when a level increases
 */
UCLASS(BlueprintType, Blueprintable, ClassGroup=(Pangea), meta=(BlueprintSpawnableComponent))
class PANGEABASEUPGRADESYSTEM_API UPangeaUpgradeSystemComponent : public UActorComponent, public IPDBaseUpgradeInterface
{
	GENERATED_BODY()

public:
	UPangeaUpgradeSystemComponent();
	virtual void Serialize(FArchive& Ar) override;

	UFUNCTION(BlueprintCallable, Category="Upgrade")
	FPangeaUpgradeResult CheckNextUpgrade(UObject* PlayerContext) const;
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Upgrade")
	FPangeaUpgradeResult TryUpgradeChecked(UObject* PlayerContext);
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Upgrade")
	FPangeaUpgradeResult LastUpgradeResult;

	/** Stops retries after an ambiguous partial side effect or a save made inside a command.
	 * Restore a coherent backup / use an audited migration; never clear blindly and retry. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, SaveGame, Category="Save")
	bool bUpgradeNeedsRecovery = false;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, SaveGame, Category="Save")
	TSet<FString> SuccessfulActionReceipts;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, SaveGame, Category="Save")
	FString PendingOperation;
	UFUNCTION(BlueprintCallable, Category="Save")
	void OnComponentLoaded();
	
	//Save
	UFUNCTION(BlueprintCallable)
	void LoadCompletedMilestones(UObject* PlayerContext);

	UFUNCTION(BlueprintPure, Category="Upgrade")
	FBaseUpgradeContext MakeUpgradeContext(UObject* PlayerContext) const;

protected:
	virtual void BeginPlay() override;

public:
	/** Preferred root data asset for base progression. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade")
	TObjectPtr<UBaseUpgradeDefinition> UpgradeDefinition = nullptr;

	/** Current village/base level (0 = uninitialized / none) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Upgrade", SaveGame)
	int32 CurrentLevel = 0;

	/** Milestones we have successfully executed */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Upgrade", SaveGame)
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

	UFUNCTION(BlueprintCallable, Category="Upgrade|UI")
	FText GetFacilityDisplayName(const FGameplayTag& FacilityTag) const;

	// IPDBaseUpgradeInterface
	virtual bool LoadCompletedMilestonesForContext_Implementation(UObject* PlayerContext) override;
	virtual bool CanUpgradeToNextLevelForContext_Implementation(UObject* PlayerContext) const override;
	virtual bool TryUpgradeToNextLevel_Implementation(UObject* PlayerContext) override;
	virtual int32 GetCurrentUpgradeLevel_Implementation() const override;
	virtual bool IsUpgradeMilestoneCompleted_Implementation(FGameplayTag MilestoneTag) const override;
	virtual bool GetNextUpgradeLevelDefinition_Implementation(FUpgradeLevelDefinition& OutLevel) const override;
	virtual void GetUnmetRequirementsForNextUpgrade_Implementation(UObject* PlayerContext, TArray<UUpgradeRequirement*>& OutRequirements) const override;
	virtual void GetFacilitiesUnlockedAtUpgradeLevel_Implementation(int32 Level, TArray<FGameplayTag>& OutFacilities) const override;
	virtual FText GetUpgradeFacilityDisplayName_Implementation(FGameplayTag FacilityTag) const override;


private:
	bool HasDefinitionData() const;

	const UBaseProgressionFragment* GetProgressionFragment() const;
	const UBaseFacilityCatalogFragment* GetFacilityCatalogFragment() const;

	bool ResolveLevelDefinition(int32 Level, FUpgradeLevelDefinition& OutLevel) const;

	/** Helper: find level definition for an absolute level number */
	bool FindLevelDefinition(int32 Level, FUpgradeLevelDefinition& OutLevel) const;

	FString GetActionKey(const FUpgradeMilestoneDefinition& Milestone, const UUpgradeAction* Action) const;
	bool RestoreCompletedState(UObject* PlayerContext);
	bool bCommandInProgress = false;
	FPangeaUpgradeResult CheckNextUpgradeInternal(UObject* PlayerContext) const;
};
