// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Types/BaseUpgradeContext.h"
#include "UObject/Object.h"
#include "UpgradeRequirement.generated.h"

/**
 * Base class for one condition that gates a milestone of a base upgrade.
 * Requirements are authored inline in the upgrade definition and only check state. They never consume anything.
 */
UCLASS(Abstract, Blueprintable, EditInlineNew, DefaultToInstanced)
class PANGEABASEUPGRADESYSTEM_API UUpgradeRequirement : public UObject
{
	GENERATED_BODY()

public:
	/** Checks the requirement against a full upgrade context. The default forwards to IsRequirementMet with the preferred requirement object. */
	virtual bool IsRequirementMetForContext(const FBaseUpgradeContext& Context) const;

	/** Return true if requirement is met for the given context (player or entity). */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Upgrade")
	bool IsRequirementMet(UObject* ContextObject) const;
	virtual bool IsRequirementMet_Implementation(UObject* ContextObject) const;

	/** Optional failure message that can be displayed in UI. */
	UFUNCTION(BlueprintCallable, Category = "Upgrade")
	virtual FText GetFailureMessage() const;

	/** Text that describes the requirement in the upgrade menu. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Upgrade")
	FText GetRequirementDescription() const;
	virtual FText GetRequirementDescription_Implementation() const;
};
