// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "VillageUpgradeMenuWidget.generated.h"

struct FGameplayTag;
class AVillageBase;
class APawn;
class UUpgradeSystemComponent;
class URequirementEntryWidget;
class UFacilityUnlockEntryWidget;

class UTextBlock;
class UScrollBox;
class UButton;

/**
 * Simple Upgrade UI:
 * - Shows current level and next level
 * - Lists requirements for next level
 * - Lists facilities unlocked at next level
 * - Upgrade button if possible
 */
UCLASS()
class PANGEABASEUPGRADESYSTEM_API UVillageUpgradeMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	/** Village this menu is showing data for */
	UPROPERTY(BlueprintReadWrite, Category="Upgrade")
	TObjectPtr<AVillageBase> VillageActor;

	/** Pawn interacting with the village */
	UPROPERTY(BlueprintReadWrite, Category="Upgrade")
	TObjectPtr<APawn> InteractingPawn;

	/** Bound via UMG (Style A) */

	UPROPERTY(meta=(BindWidgetOptional))
	UTextBlock* CurrentLevelText;

	UPROPERTY(meta=(BindWidgetOptional))
	UTextBlock* NextLevelText;

	UPROPERTY(meta=(BindWidgetOptional))
	UScrollBox* RequirementsScroll;

	UPROPERTY(meta=(BindWidgetOptional))
	UScrollBox* UnlocksScroll;

	UPROPERTY(meta=(BindWidgetOptional))
	UButton* UpgradeButton;

	/** Widget classes for entries (set in BP) */

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade|UI")
	TSubclassOf<URequirementEntryWidget> RequirementEntryClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade|UI")
	TSubclassOf<UFacilityUnlockEntryWidget> FacilityUnlockEntryClass;

	/** Initialize with village + pawn */
	UFUNCTION(BlueprintCallable, Category="Upgrade")
	void InitializeFromVillage(AVillageBase* InVillage, APawn* InInteractingPawn);

protected:
	virtual void NativeOnInitialized() override;
	
	UFUNCTION()
	void OnUpgradeButtonClicked();

private:
	UUpgradeSystemComponent* GetUpgradeSystem() const;
	FText GetFacilityDisplayName(const FGameplayTag& FacilityTag) const;
	void RefreshUI();
};
