// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UpgradeRequirement.generated.h"

/**
 * 
 */
UCLASS(Abstract, Blueprintable, EditInlineNew, DefaultToInstanced)
class PANGEABASEUPGRADESYSTEM_API UUpgradeRequirement : public UObject
{
	GENERATED_BODY()
	
public:
	/** Return true if requirement is met for the given context (player or entity). */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Upgrade")
	bool IsRequirementMet(UObject* ContextObject) const;
	virtual bool IsRequirementMet_Implementation(UObject* ContextObject) const;

	/** Optional failure message that can be displayed in UI. */
	UFUNCTION(BlueprintCallable, Category="Upgrade")
	virtual FText GetFailureMessage() const;
	
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Upgrade")
	FText GetRequirementDescription() const;
	virtual FText GetRequirementDescription_Implementation() const;
};
