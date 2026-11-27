#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MiningSettlementStockpileWidget.generated.h"

class AMiningSettlementStockpileActor;
class APawn;
class UACFInventoryComponent;
class UACFItem;
class UButton;
class UTextBlock;
class UVerticalBox;
struct FInventoryItem;

/**
 * Settlement stockpile menu. Lists the interacting pawn's inventory next to the stockpile's storage
 * and forwards deposit and withdraw requests to the stockpile actor.
 */
UCLASS()
class MININGSYSTEMUI_API UMiningSettlementStockpileWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Binds the menu to a stockpile and the pawn using it, then refreshes the lists. */
	UFUNCTION(BlueprintCallable, Category = "Mining")
	void InitializeFromStockpile(AMiningSettlementStockpileActor* InStockpileActor, APawn* InInteractingPawn);

	/** Called by a row: asks the stockpile actor to move Count of ItemClass to or from the interacting pawn, then refreshes. */
	void HandleTransferRequest(TSubclassOf<UACFItem> ItemClass, int32 Count, bool bFromPlayerInventory);

protected:
	virtual void NativeOnInitialized() override;

private:
	void RebuildSection(UVerticalBox* Container, const TArray<FInventoryItem>& Items, bool bFromPlayerInventory);
	void BuildWidgetTree();
	void RefreshUI();
	void CloseMenu();
	FText BuildStatusText() const;

	UFUNCTION()
	void OnCloseClicked();

	UPROPERTY(Transient)
	TObjectPtr<AMiningSettlementStockpileActor> StockpileActor;

	UPROPERTY(Transient)
	TObjectPtr<APawn> InteractingPawn;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> RootBox;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StatusText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PlayerInventoryTitleText;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> PlayerItemsBox;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StockpileInventoryTitleText;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> StockpileItemsBox;

	UPROPERTY(Transient)
	TObjectPtr<UButton> CloseButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CloseButtonText;
};
