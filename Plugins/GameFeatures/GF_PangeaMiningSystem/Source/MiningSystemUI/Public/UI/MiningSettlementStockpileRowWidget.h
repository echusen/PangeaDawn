#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MiningSettlementStockpileRowWidget.generated.h"

class UACFItem;
class UButton;
class UMiningSettlementStockpileWidget;
class UTextBlock;

/**
 * One row of the settlement stockpile menu: a button that asks the owning stockpile widget
 * to deposit or withdraw a single inventory stack.
 */
UCLASS()
class MININGSYSTEMUI_API UMiningSettlementStockpileRowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Stores the stack this row transfers and the direction of the transfer, and sets the button label. */
	void InitializeRow(
		UMiningSettlementStockpileWidget* InOwnerWidget, TSubclassOf<UACFItem> InItemClass, int32 InCount, bool bInFromPlayerInventory, const FString& InLabel);

protected:
	virtual void NativeOnInitialized() override;

private:
	void BuildWidgetTree();

	UFUNCTION()
	void OnTransferClicked();

	UPROPERTY(Transient)
	TObjectPtr<UMiningSettlementStockpileWidget> OwnerWidget;

	UPROPERTY(Transient)
	TSubclassOf<UACFItem> ItemClass;

	UPROPERTY(Transient)
	int32 Count = 0;

	UPROPERTY(Transient)
	bool bFromPlayerInventory = false;

	UPROPERTY(Transient)
	FString LabelText;

	UPROPERTY(Transient)
	TObjectPtr<UButton> TransferButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TransferButtonText;
};
