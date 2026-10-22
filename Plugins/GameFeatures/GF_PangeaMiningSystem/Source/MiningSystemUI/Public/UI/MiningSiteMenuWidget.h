#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MiningSiteMenuWidget.generated.h"

class AMiningSiteActor;
class APawn;
class UButton;
class UBorder;
class UTextBlock;
class UVerticalBox;

UCLASS()
class MININGSYSTEMUI_API UMiningSiteMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="Mining")
	void InitializeFromSite(AMiningSiteActor* InSiteActor, APawn* InInteractingPawn);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	void BuildWidgetTree();
	void RefreshUI();
	void CloseMenu();
	FText BuildStatusText() const;
	FText BuildCostText() const;
	FText BuildHintText() const;
	void UpdateActionLabels();

	UFUNCTION()
	void HandleRefreshTick();

	UFUNCTION()
	void OnUpgradeClicked();

	UFUNCTION()
	void OnRecruitWorkerClicked();

	UFUNCTION()
	void OnSyncProductionClicked();

	UFUNCTION()
	void OnAdvanceDayClicked();

	UFUNCTION()
	void OnCloseClicked();

	UPROPERTY(Transient)
	TObjectPtr<AMiningSiteActor> SiteActor;

	UPROPERTY(Transient)
	TObjectPtr<APawn> InteractingPawn;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> RootBorder;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> RootBox;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> HintText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StatusText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CostText;

	UPROPERTY(Transient)
	TObjectPtr<UButton> UpgradeButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> UpgradeButtonText;

	UPROPERTY(Transient)
	TObjectPtr<UButton> RecruitWorkerButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> RecruitWorkerButtonText;

	UPROPERTY(Transient)
	TObjectPtr<UButton> SyncProductionButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SyncProductionButtonText;

	UPROPERTY(Transient)
	TObjectPtr<UButton> AdvanceDayButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> AdvanceDayButtonText;

	UPROPERTY(Transient)
	TObjectPtr<UButton> CloseButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CloseButtonText;

	FTimerHandle RefreshTimerHandle;
};
