#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "BaseUpgradeTypes.generated.h"

class UACFItem;
class UUpgradeAction;
class UUpgradeRequirement;

/** Outcome of checking or running an upgrade command. */
UENUM(BlueprintType)
enum class EPangeaUpgradeResult : uint8
{
	/** The check passed, or the upgrade was committed. */
	Succeeded,
	/** The command was called without network authority. */
	NotAuthority,
	/** Another upgrade command is already running on the component. */
	Busy,
	/** The upgrade definition is missing or invalid, or it has no next level. */
	InvalidDefinition,
	/** At least one requirement of the next level is not met. */
	RequirementsNotMet,
	/** An action failed its preflight check, or failed while state was rebuilt on load. */
	ActionNotReady,
	/** The interacting pawn cannot pay the explicit item cost of the level. */
	InsufficientItems,
	/** An interrupted or partial upgrade blocks further commands until the save is recovered. */
	RecoveryRequired
};

/** Result of an upgrade command with a reason the UI can show. Lets callers tell failure causes apart. */
USTRUCT(BlueprintType)
struct PANGEABASEUPGRADESYSTEM_API FPangeaUpgradeResult
{
	GENERATED_BODY()
	/** Outcome code of the command. */
	UPROPERTY(BlueprintReadOnly, Category = "Upgrade")
	EPangeaUpgradeResult Result = EPangeaUpgradeResult::InvalidDefinition;

	/** Optional text that explains a failure. Can be empty. */
	UPROPERTY(BlueprintReadOnly, Category = "Upgrade")
	FText Reason;
	bool IsSuccess() const
	{
		return Result == EPangeaUpgradeResult::Succeeded;
	}
};

/** Explicit payment, independent of check-only UReq_HasItems. Empty costs preserve existing economy. */
USTRUCT(BlueprintType)
struct PANGEABASEUPGRADESYSTEM_API FPangeaUpgradeItemCost
{
	GENERATED_BODY()
	/** Item class the interacting pawn pays with. Must be a concrete ACF item. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cost")
	TSubclassOf<UACFItem> ItemClass;

	/** Number of unequipped items removed. Entries with the same class are added together. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cost", meta = (ClampMin = "1"))
	int32 Count = 1;
};

/** Display name for one facility tag. Used by the facility catalog fragment so UI does not show raw tags. */
USTRUCT(BlueprintType)
struct PANGEABASEUPGRADESYSTEM_API FFacilityGroupReference
{
	GENERATED_BODY()

	/** Name shown in UI for the facility. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Facility")
	FText FacilityGroupName;

	/** Tag that identifies the facility. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Facility")
	FGameplayTag FacilityTag;
};

/** One step of an upgrade level: the requirements that gate it and the actions it runs when the level is reached. */
USTRUCT(BlueprintType)
struct PANGEABASEUPGRADESYSTEM_API FUpgradeMilestoneDefinition
{
	GENERATED_BODY()

	/** Identity of the milestone. Must be valid and unique in the whole definition. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrade")
	FGameplayTag MilestoneTag;

	/** Name of the milestone for UI. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrade")
	FText DisplayName;

	/** Requirements that must all be met before the level can be upgraded. They are checks only, never payment. */
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category = "Upgrade")
	TArray<TObjectPtr<UUpgradeRequirement>> Requirements;

	/** Actions executed in order when the level is upgraded. */
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category = "Upgrade")
	TArray<TObjectPtr<UUpgradeAction>> Actions;
};

/** One upgrade level of a base: its number, its explicit item cost and its milestones. */
USTRUCT(BlueprintType)
struct PANGEABASEUPGRADESYSTEM_API FUpgradeLevelDefinition
{
	GENERATED_BODY()

	/** Absolute level number. Levels must be consecutive and start at 1. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrade")
	int32 Level = 0;

	/** Items removed from the interacting pawn after all actions of the level succeeded. Empty means free. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cost")
	TArray<FPangeaUpgradeItemCost> ItemCosts;

	/** Milestones completed when this level is reached. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrade")
	TArray<FUpgradeMilestoneDefinition> Milestones;
};
