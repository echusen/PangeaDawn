// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Objects/UpgradeRequirement.h"
#include "Req_MilestoneCompleted.generated.h"

/**
 * Requirement that passes when another milestone of the same base is already completed.
 * Lets milestones be chained without a dependency on level numbers.
 */
UCLASS()
class PANGEABASEUPGRADESYSTEM_API UReq_MilestoneCompleted : public UUpgradeRequirement
{
	GENERATED_BODY()

public:
	/** The milestone tag that must be completed before this one */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Requirement")
	FGameplayTag RequiredMilestoneTag;

	virtual bool IsRequirementMetForContext(const FBaseUpgradeContext& Context) const override;
	virtual bool IsRequirementMet_Implementation(UObject* ContextObject) const override;
};
