#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "BaseUpgradeTypes.generated.h"

class UUpgradeAction;
class UUpgradeRequirement;
class UACFItem;

UENUM(BlueprintType)
enum class EPangeaUpgradeResult : uint8
{
	Succeeded,
	NotAuthority,
	Busy,
	InvalidDefinition,
	RequirementsNotMet,
	ActionNotReady,
	InsufficientItems,
	RecoveryRequired
};

USTRUCT(BlueprintType)
struct PANGEABASEUPGRADESYSTEM_API FPangeaUpgradeResult
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category="Upgrade")
	EPangeaUpgradeResult Result = EPangeaUpgradeResult::InvalidDefinition;
	UPROPERTY(BlueprintReadOnly, Category="Upgrade")
	FText Reason;
	bool IsSuccess() const { return Result == EPangeaUpgradeResult::Succeeded; }
};

/** Explicit payment, independent of check-only UReq_HasItems. Empty costs preserve existing economy. */
USTRUCT(BlueprintType)
struct PANGEABASEUPGRADESYSTEM_API FPangeaUpgradeItemCost
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cost")
	TSubclassOf<UACFItem> ItemClass;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cost", meta=(ClampMin="1"))
	int32 Count = 1;
};

USTRUCT(BlueprintType)
struct PANGEABASEUPGRADESYSTEM_API FFacilityGroupReference
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Facility")
	FText FacilityGroupName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Facility")
	FGameplayTag FacilityTag;
};

USTRUCT(BlueprintType)
struct PANGEABASEUPGRADESYSTEM_API FUpgradeMilestoneDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade")
	FGameplayTag MilestoneTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade")
	FText DisplayName;

	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category="Upgrade")
	TArray<TObjectPtr<UUpgradeRequirement>> Requirements;

	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category="Upgrade")
	TArray<TObjectPtr<UUpgradeAction>> Actions;
};

USTRUCT(BlueprintType)
struct PANGEABASEUPGRADESYSTEM_API FUpgradeLevelDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade")
	int32 Level = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cost")
	TArray<FPangeaUpgradeItemCost> ItemCosts;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade")
	TArray<FUpgradeMilestoneDefinition> Milestones;
};
