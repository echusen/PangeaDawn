// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Objects/UpgradeRequirement.h"
#include "Req_FacilityUnlocked.generated.h"

/**
 * Requirement that passes when a facility of the same base is already unlocked.
 * Lets one upgrade depend on a facility that an earlier upgrade enabled.
 */
UCLASS()
class PANGEABASEUPGRADESYSTEM_API UReq_FacilityUnlocked : public UUpgradeRequirement
{
	GENERATED_BODY()

public:
	/** Facility that must already be unlocked */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Requirement")
	FGameplayTag RequiredFacilityTag;

	virtual bool IsRequirementMetForContext(const FBaseUpgradeContext& Context) const override;
	virtual bool IsRequirementMet_Implementation(UObject* ContextObject) const override;
};
