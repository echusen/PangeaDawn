// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Interfaces/PDBaseUpgradeInterface.h"
#include "Types/BaseUpgradeContext.h"
#include "Types/BaseUpgradeTypes.h"
#include "UpgradeSystemComponent.generated.h"

class UBaseFacilityCatalogFragment;
class UBaseProgressionFragment;
class UBaseUpgradeDefinition;
class UUpgradeAction;
class UUpgradeRequirement;

/**
 * Component that drives village/base upgrades from a single fragment-based definition.
 * - Reads all level + milestone data from UpgradeDefinition
 * - Evaluates requirements
 * - Executes actions when a level increases
 */
UCLASS(BlueprintType, Blueprintable, ClassGroup = (Pangea), meta = (BlueprintSpawnableComponent))
class PANGEABASEUPGRADESYSTEM_API UPangeaUpgradeSystemComponent : public UActorComponent, public IPDBaseUpgradeInterface
{
	GENERATED_BODY()

public:
	UPangeaUpgradeSystemComponent();
	virtual void Serialize(FArchive& Ar) override;

	/** Checks, without side effects, whether the next level can be upgraded now. Returns the reason when it cannot. */
	UFUNCTION(BlueprintCallable, Category = "Upgrade")
	FPangeaUpgradeResult CheckNextUpgrade(UObject* PlayerContext) const;

	/** Runs the upgrade to the next level: actions first, then payment, then the level commit. Authority only. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Upgrade")
	FPangeaUpgradeResult TryUpgradeChecked(UObject* PlayerContext);

	/** Result of the most recent upgrade or restore command. Not saved. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Upgrade")
	FPangeaUpgradeResult LastUpgradeResult;

	/** Stops retries after an ambiguous partial side effect or a save made inside a command.
	 * Restore a coherent backup / use an audited migration; never clear blindly and retry. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, SaveGame, Category = "Save")
	bool bUpgradeNeedsRecovery = false;

	/** Keys of actions that already completed. An action with a receipt is never executed again by an upgrade command. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, SaveGame, Category = "Save")
	TSet<FString> SuccessfulActionReceipts;

	/** Key of the action or payment that is running. Not empty in a loaded save means the command was interrupted. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, SaveGame, Category = "Save")
	FString PendingOperation;

	/** Call after this component was loaded from a save. Restores completed state on the authority. */
	UFUNCTION(BlueprintCallable, Category = "Save")
	void OnComponentLoaded();

	//Save
	UFUNCTION(BlueprintCallable)
	void LoadCompletedMilestones(UObject* PlayerContext);

	/** Builds the context for one command from PlayerContext (a pawn or a player controller) and the owner of this component. */
	UFUNCTION(BlueprintPure, Category = "Upgrade")
	FBaseUpgradeContext MakeUpgradeContext(UObject* PlayerContext) const;

protected:
	virtual void BeginPlay() override;

public:
	/** Preferred root data asset for base progression. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrade")
	TObjectPtr<UBaseUpgradeDefinition> UpgradeDefinition = nullptr;

	/** Current village/base level (0 = uninitialized / none) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Upgrade", SaveGame)
	int32 CurrentLevel = 0;

	/** Milestones we have successfully executed */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Upgrade", SaveGame)
	FGameplayTagContainer CompletedMilestones;

	/**
	 * Called by your leveling system when the village/base level increases.
	 * PlayerContext is typically the player pawn, controller, or anything
	 * that requirements need to inspect (inventory, quest manager, etc.).
	 */
	UFUNCTION(BlueprintCallable, Category = "Upgrade")
	void OnLevelIncreased(int32 NewLevel, UObject* PlayerContext);

	/** Returns true if all requirements for CurrentLevel+1 are satisfied */
	UFUNCTION(BlueprintCallable, Category = "Upgrade")
	bool CanUpgradeToNextLevel(UObject* PlayerContext) const;

	/** Check if a milestone has already been executed */
	UFUNCTION(BlueprintCallable, Category = "Upgrade")
	bool IsMilestoneCompleted(FGameplayTag MilestoneTag) const;

	/** Mark a milestone as completed (used internally, but exposed for debugging if needed) */
	UFUNCTION(BlueprintCallable, Category = "Upgrade")
	void MarkMilestoneCompleted(FGameplayTag MilestoneTag);

	//UI Helpers
	UFUNCTION(BlueprintCallable, Category = "Upgrade|UI")
	bool GetNextLevelDefinition(FUpgradeLevelDefinition& OutLevel) const;

	/** Fills OutMilestones with the milestones authored for Level. Empty when the level does not exist. */
	UFUNCTION(BlueprintCallable, Category = "Upgrade|UI")
	void GetMilestonesForLevel(int32 Level, TArray<FUpgradeMilestoneDefinition>& OutMilestones) const;

	/** Fills OutRequirements with the requirements of the next level that PlayerContext does not meet. */
	UFUNCTION(BlueprintCallable, Category = "Upgrade|UI")
	void GetUnmetRequirementsForNextLevel(UObject* PlayerContext, TArray<UUpgradeRequirement*>& OutRequirements) const;

	/** Fills OutFacilities with the unique facility tags that the actions of Level unlock. */
	UFUNCTION(BlueprintCallable, Category = "Upgrade|UI")
	void GetFacilitiesUnlockedAtLevel(int32 Level, TArray<FGameplayTag>& OutFacilities) const;

	/** Returns the name from the facility catalog fragment, or the tag as text when no name is authored. */
	UFUNCTION(BlueprintCallable, Category = "Upgrade|UI")
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

	/** Stable receipt key of an action: definition path, milestone tag and action identity. */
	FString GetActionKey(const FUpgradeMilestoneDefinition& Milestone, const UUpgradeAction* Action) const;

	/** Rebuilds absolute state (facilities) from completed milestones after a load. Runs only actions that opt in to reconstruction and never pays costs. */
	bool RestoreCompletedState(UObject* PlayerContext);

	/** True while an upgrade or restore command runs. Rejects nested commands that callbacks could start. */
	bool bCommandInProgress = false;

	/** CheckNextUpgrade without the busy check, so a running command can use it. */
	FPangeaUpgradeResult CheckNextUpgradeInternal(UObject* PlayerContext) const;
};
