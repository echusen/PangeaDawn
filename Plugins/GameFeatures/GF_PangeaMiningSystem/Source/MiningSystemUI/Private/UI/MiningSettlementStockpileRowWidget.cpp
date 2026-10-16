#include "UI/MiningSettlementStockpileRowWidget.h"

#include "UI/MiningSettlementStockpileWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Blueprint/WidgetTree.h"
#include "UI/MiningUIPresentation.h"
#include "Items/ACFItem.h"

void UMiningSettlementStockpileRowWidget::InitializeRow(UMiningSettlementStockpileWidget* InOwnerWidget, TSubclassOf<UACFItem> InItemClass, int32 InCount, bool bInFromPlayerInventory, const FString& InLabel)
{
	OwnerWidget = InOwnerWidget;
	ItemClass = InItemClass;
	Count = InCount;
	bFromPlayerInventory = bInFromPlayerInventory;
	LabelText = InLabel;

	if (TransferButtonText)
	{
		TransferButtonText->SetText(FText::FromString(LabelText));
	}
}

void UMiningSettlementStockpileRowWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildWidgetTree();
}

void UMiningSettlementStockpileRowWidget::BuildWidgetTree()
{
	if (!WidgetTree || TransferButton)
	{
		return;
	}

	TransferButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("TransferButton"));
    FMiningUIPresentation::Button(TransferButton);
	WidgetTree->RootWidget = TransferButton;

	TransferButtonText = FMiningUIPresentation::Text(WidgetTree, TEXT("TransferButtonText"));
	TransferButtonText->SetText(FText::FromString(LabelText));
	TransferButtonText->SetJustification(ETextJustify::Center);
	TransferButton->AddChild(TransferButtonText);
	TransferButton->OnClicked.AddDynamic(this, &ThisClass::OnTransferClicked);
}

void UMiningSettlementStockpileRowWidget::OnTransferClicked()
{
	if (OwnerWidget && ItemClass && Count > 0)
	{
		OwnerWidget->HandleTransferRequest(ItemClass, Count, bFromPlayerInventory);
	}
}
