#include "UI/MiningSettlementStockpileWidget.h"

#include "Actors/MiningSettlementStockpileActor.h"
#include "UI/MiningSettlementStockpileRowWidget.h"
#include "Components/ACFInventoryComponent.h"
#include "Components/ACFStorageComponent.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Blueprint/WidgetTree.h"
#include "UI/MiningUIPresentation.h"
#include "GameFramework/PlayerController.h"
#include "Items/ACFItem.h"

void UMiningSettlementStockpileWidget::InitializeFromStockpile(AMiningSettlementStockpileActor* InStockpileActor, APawn* InInteractingPawn)
{
	StockpileActor = InStockpileActor;
	InteractingPawn = InInteractingPawn;
    if (UANSNavPageWidget* Page = GetTypedOuter<UANSNavPageWidget>())
    {
        Page->OnActivated().RemoveAll(this);
        Page->OnActivated().AddWeakLambda(this, [this]() { if (!IsValid(StockpileActor)) { FMiningUIPresentation::Close(this); return; }  RefreshUI(); });

        Page->OnDeactivated().RemoveAll(this);
        Page->OnDeactivated().AddWeakLambda(this, [this]()
        {
        });
    }

	RefreshUI();
}

void UMiningSettlementStockpileWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildWidgetTree();
	RefreshUI();
}

void UMiningSettlementStockpileWidget::BuildWidgetTree()
{
	if (!WidgetTree || RootBox)
	{
		return;
	}

	RootBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("RootBox"));
	FMiningUIPresentation::ScrollContent(WidgetTree, FMiningUIPresentation::Screen(WidgetTree), RootBox);

	auto AddText = [this](const TCHAR* Name, int32 FontSize) -> UTextBlock*
	{
		UTextBlock* TextBlock = FMiningUIPresentation::Text(WidgetTree, Name, FontSize);
		TextBlock->SetAutoWrapText(true);
		FSlateFontInfo FontInfo = TextBlock->GetFont();
		FontInfo.Size = FontSize;
		TextBlock->SetFont(FontInfo);
		if (UVerticalBoxSlot* VerticalBoxSlot = RootBox->AddChildToVerticalBox(TextBlock))
		{
			VerticalBoxSlot->SetPadding(FMargin(8.0f));
		}
		return TextBlock;
	};

	auto AddBox = [this](const TCHAR* Name) -> UVerticalBox*
	{
		UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), Name);
		if (UVerticalBoxSlot* VerticalBoxSlot = RootBox->AddChildToVerticalBox(Box))
		{
			VerticalBoxSlot->SetPadding(FMargin(8.0f, 4.0f));
		}
		return Box;
	};

	TitleText = AddText(TEXT("TitleText"), 28);
	StatusText = AddText(TEXT("StatusText"), 18);
	PlayerInventoryTitleText = AddText(TEXT("PlayerInventoryTitleText"), 16);
	PlayerItemsBox = AddBox(TEXT("PlayerItemsBox"));
	StockpileInventoryTitleText = AddText(TEXT("StockpileInventoryTitleText"), 16);
	StockpileItemsBox = AddBox(TEXT("StockpileItemsBox"));

	// Keep deposit and withdrawal choices side by side, rather than pushing
	// the stockpile below a potentially long player inventory.
	UHorizontalBox* Inventories = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Inventories"));
	RootBox->AddChildToVerticalBox(Inventories);
	auto AddInventoryColumn = [this, Inventories](const TCHAR* Name, UTextBlock* Heading, UVerticalBox* Items)
	{
		UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), Name);
		UHorizontalBoxSlot* ColumnSlot = Inventories->AddChildToHorizontalBox(Column);
		ColumnSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		ColumnSlot->SetPadding(FMargin(8.f));
		Heading->RemoveFromParent();
		Column->AddChildToVerticalBox(Heading)->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));
		Items->RemoveFromParent();
		USizeBox* ListBounds = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		ListBounds->SetHeightOverride(340.f);
		UScrollBox* ListScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
		ListBounds->SetContent(ListScroll);
		ListScroll->AddChild(Items);
		Column->AddChildToVerticalBox(ListBounds);
	};
	AddInventoryColumn(TEXT("PlayerInventoryColumn"), PlayerInventoryTitleText, PlayerItemsBox);
	AddInventoryColumn(TEXT("StockpileColumn"), StockpileInventoryTitleText, StockpileItemsBox);

	CloseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("CloseButton"));
    FMiningUIPresentation::Button(CloseButton);
	CloseButtonText = FMiningUIPresentation::Text(WidgetTree, TEXT("CloseButtonText"));
	CloseButtonText->SetText(FText::FromString(TEXT("Close")));
	CloseButtonText->SetJustification(ETextJustify::Center);
	CloseButton->AddChild(CloseButtonText);
	if (UVerticalBoxSlot* VerticalBoxSlot = RootBox->AddChildToVerticalBox(CloseButton))
	{
		VerticalBoxSlot->SetPadding(FMargin(8.0f, 8.0f));
	}

	if (CloseButton)
	{
		CloseButton->OnClicked.AddDynamic(this, &ThisClass::OnCloseClicked);
	}
}

void UMiningSettlementStockpileWidget::RefreshUI()
{
	if (!TitleText || !StatusText || !PlayerInventoryTitleText || !PlayerItemsBox || !StockpileInventoryTitleText || !StockpileItemsBox)
	{
		return;
	}

	TitleText->SetText(FText::FromString(TEXT("Settlement Stockpile")));
	StatusText->SetText(BuildStatusText());
	PlayerInventoryTitleText->SetText(FText::FromString(TEXT("Player Inventory")));
	StockpileInventoryTitleText->SetText(FText::FromString(TEXT("Stockpile")));

	PlayerItemsBox->ClearChildren();
	StockpileItemsBox->ClearChildren();

	if (UACFInventoryComponent* InventoryComponent = InteractingPawn ? InteractingPawn->FindComponentByClass<UACFInventoryComponent>() : nullptr)
	{
		RebuildSection(PlayerItemsBox, InventoryComponent->GetInventory(), true);
	}

	if (IsValid(StockpileActor) && StockpileActor->StorageComponent)
	{
		RebuildSection(StockpileItemsBox, StockpileActor->StorageComponent->GetInventory(), false);
	}
}

void UMiningSettlementStockpileWidget::RebuildSection(UVerticalBox* Container, const TArray<FInventoryItem>& Items, bool bFromPlayerInventory)
{
	if (!WidgetTree || !Container)
	{
		return;
	}

	if (Items.IsEmpty())
	{
		UTextBlock* EmptyText = FMiningUIPresentation::Text(WidgetTree, bFromPlayerInventory ? TEXT("EmptyPlayerText") : TEXT("EmptyStockpileText"));
		EmptyText->SetText(FText::FromString(TEXT("Empty")));
		if (UVerticalBoxSlot* VerticalBoxSlot = Container->AddChildToVerticalBox(EmptyText))
		{
			VerticalBoxSlot->SetPadding(FMargin(4.0f));
		}
		return;
	}

	for (const FInventoryItem& Item : Items)
	{
		if (!Item.ItemClass || Item.Count <= 0)
		{
			continue;
		}

		const FString ItemName = Item.ItemClass->GetDefaultObject<UACFItem>()
			? Item.ItemClass->GetDefaultObject<UACFItem>()->GetItemName().ToString()
			: Item.ItemClass->GetName();
		const FString Label = FString::Printf(TEXT("%s %s x%d"), bFromPlayerInventory ? TEXT("Deposit") : TEXT("Withdraw"), *ItemName, Item.Count);

		UMiningSettlementStockpileRowWidget* RowWidget = CreateWidget<UMiningSettlementStockpileRowWidget>(GetOwningPlayer(), UMiningSettlementStockpileRowWidget::StaticClass());
		if (!RowWidget)
		{
			continue;
		}

		RowWidget->InitializeRow(this, Item.ItemClass, Item.Count, bFromPlayerInventory, Label);
		if (UVerticalBoxSlot* VerticalBoxSlot = Container->AddChildToVerticalBox(RowWidget))
		{
			VerticalBoxSlot->SetPadding(FMargin(4.0f, 2.0f));
		}
	}
}

FText UMiningSettlementStockpileWidget::BuildStatusText() const
{
	if (!IsValid(StockpileActor) || !StockpileActor->StorageComponent)
	{
		return FText::FromString(TEXT("No stockpile storage."));
	}

	int32 StoredCount = 0;
	for (const FInventoryItem& Item : StockpileActor->StorageComponent->GetInventory())
	{
		StoredCount += Item.Count;
	}

	return FText::FromString(FString::Printf(TEXT("Stored item count: %d"), StoredCount));
}

void UMiningSettlementStockpileWidget::CloseMenu()
{
	if (IsValid(StockpileActor))
	{
		StockpileActor->ClearActiveStockpileWidget(this);
	}

	FMiningUIPresentation::Close(this);
}

void UMiningSettlementStockpileWidget::HandleTransferRequest(TSubclassOf<UACFItem> ItemClass, int32 Count, bool bFromPlayerInventory)
{
	if (!IsValid(StockpileActor) || !InteractingPawn || !ItemClass || Count <= 0)
	{
		return;
	}

	if (bFromPlayerInventory)
	{
		StockpileActor->ServerTransferItemFromPawn(InteractingPawn, ItemClass, Count);
	}
	else
	{
		StockpileActor->ServerTransferItemToPawn(InteractingPawn, ItemClass, Count);
	}

	RefreshUI();
}

void UMiningSettlementStockpileWidget::OnCloseClicked()
{
	CloseMenu();
}
