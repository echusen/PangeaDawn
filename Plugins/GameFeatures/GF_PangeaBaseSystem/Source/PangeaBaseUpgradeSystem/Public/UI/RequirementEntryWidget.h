// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RequirementEntryWidget.generated.h"

class UTextBlock;
class UUpgradeRequirement;

/** One row of the upgrade menu's requirement list. Shows a requirement and whether the player meets it. */
UCLASS()
class PANGEABASEUPGRADESYSTEM_API URequirementEntryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Requirement this row stands for. */
	UPROPERTY(BlueprintReadOnly, Category = "Requirement")
	TObjectPtr<UUpgradeRequirement> Requirement;

	/** Optional bound text block that shows the requirement description. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> RequirementText;

	/** Optional bound text block that shows Met or Not Met. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StatusText;

	/** Stores the requirement and fills the bound text blocks. bMet selects the status text. */
	UFUNCTION(BlueprintCallable, Category = "Requirement")
	void InitFromRequirement(UUpgradeRequirement* InRequirement, bool bMet);
};
