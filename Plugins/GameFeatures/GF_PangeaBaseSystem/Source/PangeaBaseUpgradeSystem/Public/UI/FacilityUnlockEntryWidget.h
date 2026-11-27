// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "FacilityUnlockEntryWidget.generated.h"

class UTextBlock;

/** One row of the upgrade menu's unlock list. Shows the name of a facility that the next level unlocks. */
UCLASS()
class PANGEABASEUPGRADESYSTEM_API UFacilityUnlockEntryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Facility this row stands for. */
	UPROPERTY(BlueprintReadOnly, Category = "Facility")
	FGameplayTag FacilityTag;

	/** Optional bound text block that shows the facility name. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> FacilityNameText;

	/** Stores the tag and shows the tag itself as the name. */
	UFUNCTION(BlueprintCallable, Category = "Facility")
	void InitFromFacilityTag(FGameplayTag InTag);

	/** Stores the tag and shows DisplayName as the name. */
	UFUNCTION(BlueprintCallable, Category = "Facility")
	void InitFromFacilityDisplayName(FGameplayTag InTag, const FText& DisplayName);
};
