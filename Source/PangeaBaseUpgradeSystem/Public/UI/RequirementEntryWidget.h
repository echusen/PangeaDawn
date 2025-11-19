// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RequirementEntryWidget.generated.h"

class UUpgradeRequirement;
class UTextBlock;

UCLASS()
class PANGEABASEUPGRADESYSTEM_API URequirementEntryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category="Requirement")
	TObjectPtr<UUpgradeRequirement> Requirement;

	UPROPERTY(meta=(BindWidgetOptional))
	UTextBlock* RequirementText;

	UPROPERTY(meta=(BindWidgetOptional))
	UTextBlock* StatusText;

	UFUNCTION(BlueprintCallable, Category="Requirement")
	void InitFromRequirement(UUpgradeRequirement* InRequirement, bool bMet);
};
