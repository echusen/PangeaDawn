#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Types/BaseUpgradeTypes.h"
#include "UObject/Interface.h"
#include "PDBaseUpgradeInterface.generated.h"

class UUpgradeRequirement;

/** UObject side of IPDBaseUpgradeInterface. Exists only for reflection. */
UINTERFACE(BlueprintType)
class PANGEABASEUPGRADESYSTEM_API UPDBaseUpgradeInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * Contract of a component that provides base upgrades.
 * Lets the village actor and UI drive upgrades without a dependency on the concrete upgrade component.
 */
class PANGEABASEUPGRADESYSTEM_API IPDBaseUpgradeInterface
{
	GENERATED_BODY()

public:
	/** Restores the state of completed milestones after a load. Returns true when the restore succeeded. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Upgrade")
	bool LoadCompletedMilestonesForContext(UObject* PlayerContext);

	/** Returns true when the next level can be upgraded now for PlayerContext. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Upgrade")
	bool CanUpgradeToNextLevelForContext(UObject* PlayerContext) const;

	/** Tries to upgrade to the next level. Returns true only when the level was committed. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Upgrade")
	bool TryUpgradeToNextLevel(UObject* PlayerContext);

	/** Returns the current upgrade level. 0 means no level reached yet. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Upgrade")
	int32 GetCurrentUpgradeLevel() const;

	/** Returns true when the milestone with MilestoneTag is completed. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Upgrade")
	bool IsUpgradeMilestoneCompleted(FGameplayTag MilestoneTag) const;

	/** Copies the definition of the level after the current one to OutLevel. Returns false when there is none. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Upgrade|UI")
	bool GetNextUpgradeLevelDefinition(FUpgradeLevelDefinition& OutLevel) const;

	/** Fills OutRequirements with the requirements of the next level that PlayerContext does not meet. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Upgrade|UI")
	void GetUnmetRequirementsForNextUpgrade(UObject* PlayerContext, TArray<UUpgradeRequirement*>& OutRequirements) const;

	/** Fills OutFacilities with the facility tags that the actions of Level unlock. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Upgrade|UI")
	void GetFacilitiesUnlockedAtUpgradeLevel(int32 Level, TArray<FGameplayTag>& OutFacilities) const;

	/** Returns the display name of a facility, or the tag as text when no name is authored. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Upgrade|UI")
	FText GetUpgradeFacilityDisplayName(FGameplayTag FacilityTag) const;
};
