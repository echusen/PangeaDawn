// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Objects/UpgradeAction.h"
#include "UA_EnableFacility.generated.h"

/** How UUA_EnableFacility changes the state of its facility. */
UENUM(BlueprintType)
enum class EFacilityEnableMode : uint8
{
	/** Set the facility to enabled. */
	EnableOnly,
	/** Set the facility to disabled. */
	DisableOnly,
	/** Legacy relative mode. Rejected by validation because it cannot be replayed safely on load. */
	Toggle
};

/**
 * Upgrade action that sets one facility of the base to an absolute enabled or disabled state.
 * The state is absolute so the action can run again on load to rebuild facility state.
 */
UCLASS()
class PANGEABASEUPGRADESYSTEM_API UUA_EnableFacility : public UUpgradeAction
{
	GENERATED_BODY()

public:
	virtual bool ValidateConfiguration_Implementation(FText& OutFailure) const override;
	virtual bool CanExecuteUpgrade_Implementation(const FBaseUpgradeContext& Context, FText& OutFailure) const override;
	virtual bool TryExecuteUpgrade_Implementation(const FBaseUpgradeContext& Context, FText& OutFailure) override;
	virtual bool CanReconstructOnLoad_Implementation() const override;
	virtual FGameplayTag GetUnlockedFacilityTag_Implementation() const override;

	/** Authored name of the facility. This action does not read it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Facility")
	FText FacilityDisplayName;

	/* Which facility to affect */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Facility")
	FGameplayTag FacilityTag;

	/* Mode */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Facility")
	EFacilityEnableMode Mode = EFacilityEnableMode::EnableOnly;

	virtual void ExecuteForContext(const FBaseUpgradeContext& Context) override;
	virtual void Execute_Implementation(UObject* ContextObject) override;
};
