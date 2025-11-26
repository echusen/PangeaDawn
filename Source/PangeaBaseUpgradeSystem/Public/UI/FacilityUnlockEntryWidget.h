// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Blueprint/UserWidget.h"
#include "FacilityUnlockEntryWidget.generated.h"

class UTextBlock;

UCLASS()
class PANGEABASEUPGRADESYSTEM_API UFacilityUnlockEntryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category="Facility")
	FGameplayTag FacilityTag;

	UPROPERTY(meta=(BindWidgetOptional))
	UTextBlock* FacilityNameText;

	UFUNCTION(BlueprintCallable, Category="Facility")
	void InitFromFacilityTag(FGameplayTag InTag);
	
	UFUNCTION(BlueprintCallable, Category="Facility")
	void InitFromFacilityDisplayName(FGameplayTag InTag, const FText& DisplayName);
};
