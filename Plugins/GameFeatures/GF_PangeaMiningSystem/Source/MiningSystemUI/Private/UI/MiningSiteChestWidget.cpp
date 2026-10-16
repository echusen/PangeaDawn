#include "UI/MiningSiteChestWidget.h"

#include "Actors/MiningSiteChestActor.h"
#include "Components/ACFStorageComponent.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Blueprint/WidgetTree.h"
#include "UI/MiningUIPresentation.h"
#include "GameFramework/PlayerController.h"
#include "Items/ACFItem.h"

void UMiningSiteChestWidget::InitializeFromChest(AMiningSiteChestActor* InChestActor, APawn* InInteractingPawn)
{
	ChestActor = InChestActor;
	InteractingPawn = InInteractingPawn;
    if (UANSNavPageWidget* Page = GetTypedOuter<UANSNavPageWidget>())
    {
        Page->OnActivated().RemoveAll(this);
        Page->OnActivated().AddWeakLambda(this, [this]() { if (!IsValid(ChestActor)) { FMiningUIPresentation::Close(this); return; }  RefreshUI(); });

        Page->OnDeactivated().RemoveAll(this);
        Page->OnDeactivated().AddWeakLambda(this, [this]()
        {
        });
    }

	RefreshUI();
}

void UMiningSiteChestWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildWidgetTree();
	RefreshUI();
}

void UMiningSiteChestWidget::BuildWidgetTree()
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
		if (UVerticalBoxSlot* Slot = RootBox->AddChildToVerticalBox(TextBlock))
		{
			Slot->SetPadding(FMargin(8.0f));
		}
		return TextBlock;
	};

	auto AddButton = [this](const TCHAR* ButtonName, const TCHAR* TextName, const FText& Label, TObjectPtr<UButton>& OutButton, TObjectPtr<UTextBlock>& OutText) -> void
	{
		OutButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), ButtonName);
        FMiningUIPresentation::Button(OutButton);
		OutText = FMiningUIPresentation::Text(WidgetTree, TextName);
		OutText->SetText(Label);
		OutText->SetJustification(ETextJustify::Center);
		OutButton->AddChild(OutText);
		if (UVerticalBoxSlot* Slot = RootBox->AddChildToVerticalBox(OutButton))
		{
			Slot->SetPadding(FMargin(8.0f, 4.0f));
		}
	};

	TitleText = AddText(TEXT("TitleText"), 28);
	StatusText = AddText(TEXT("StatusText"), 18);

	AddButton(TEXT("WithdrawAllButton"), TEXT("WithdrawAllButtonText"), FText::FromString(TEXT("Withdraw All")), WithdrawAllButton, WithdrawAllButtonText);
	AddButton(TEXT("CloseButton"), TEXT("CloseButtonText"), FText::FromString(TEXT("Close")), CloseButton, CloseButtonText);

	if (WithdrawAllButton)
	{
		WithdrawAllButton->OnClicked.AddDynamic(this, &ThisClass::OnWithdrawAllClicked);
	}

	if (CloseButton)
	{
		CloseButton->OnClicked.AddDynamic(this, &ThisClass::OnCloseClicked);
	}
}

void UMiningSiteChestWidget::RefreshUI()
{
	if (!TitleText || !StatusText)
	{
		return;
	}

	TitleText->SetText(FText::FromString(TEXT("Mining Chest")));
	StatusText->SetText(BuildStatusText());
}

FText UMiningSiteChestWidget::BuildStatusText() const
{
	if (!IsValid(ChestActor) || !ChestActor->StorageComponent)
	{
		return FText::FromString(TEXT("No chest storage."));
	}

	const TArray<FInventoryItem> Items = ChestActor->StorageComponent->GetInventory();
	if (Items.IsEmpty())
	{
		return FText::FromString(TEXT("Chest is empty."));
	}

	TArray<FString> Parts;
	Parts.Reserve(Items.Num());
	for (const FInventoryItem& Item : Items)
	{
		FString ItemName = Item.ItemClass ? Item.ItemClass->GetName() : TEXT("Unknown");
		if (Item.ItemClass)
		{
			if (const UACFItem* DefaultItem = Item.ItemClass->GetDefaultObject<UACFItem>())
			{
				ItemName = DefaultItem->GetItemName().ToString();
			}
		}

		Parts.Add(FString::Printf(TEXT("%s x%d"), *ItemName, Item.Count));
	}

	return FText::FromString(FString::Printf(TEXT("Stored Items:\n%s"), *FString::Join(Parts, TEXT("\n"))));
}

void UMiningSiteChestWidget::CloseMenu()
{
	if (IsValid(ChestActor))
	{
		ChestActor->ClearActiveChestWidget(this);
	}

	FMiningUIPresentation::Close(this);
}

void UMiningSiteChestWidget::OnWithdrawAllClicked()
{
	if (IsValid(ChestActor))
	{
		ChestActor->ServerWithdrawAllToPawn(InteractingPawn);
	}

	RefreshUI();
}

void UMiningSiteChestWidget::OnCloseClicked()
{
	CloseMenu();
}
