#include "UI/MiningSiteMenuWidget.h"

#include "Actors/MiningSiteActor.h"
#include "Components/Button.h"
#include "Components/Border.h"
#include "Components/MiningSiteComponent.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Items/ACFItem.h"
#include "Blueprint/WidgetTree.h"
#include "UI/MiningUIPresentation.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "TimerManager.h"

void UMiningSiteMenuWidget::InitializeFromSite(AMiningSiteActor* InSiteActor, APawn* InInteractingPawn)
{
	SiteActor = InSiteActor;
	InteractingPawn = InInteractingPawn;
    if (UANSNavPageWidget* Page = GetTypedOuter<UANSNavPageWidget>())
    {
        Page->OnActivated().RemoveAll(this);
        Page->OnActivated().AddWeakLambda(this, [this]() { if (!IsValid(SiteActor)) { FMiningUIPresentation::Close(this); return; } if (UWorld* World = GetWorld()) World->GetTimerManager().SetTimer(RefreshTimerHandle, this, &ThisClass::HandleRefreshTick, 0.25f, true); RefreshUI(); });
        if (UWorld* World = GetWorld()) World->GetTimerManager().SetTimer(RefreshTimerHandle, this, &ThisClass::HandleRefreshTick, 0.25f, true);
        Page->OnDeactivated().RemoveAll(this);
        Page->OnDeactivated().AddWeakLambda(this, [this]()
        {
            if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(RefreshTimerHandle);
        });
    }

	RefreshUI();
}

void UMiningSiteMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildWidgetTree();
	RefreshUI();
}

void UMiningSiteMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(RefreshTimerHandle, this, &ThisClass::HandleRefreshTick, 0.25f, true);
	}
}

void UMiningSiteMenuWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RefreshTimerHandle);
	}

	if (IsValid(SiteActor))
	{
		SiteActor->ClearActiveMenuWidget(this);
	}

	Super::NativeDestruct();
}

void UMiningSiteMenuWidget::BuildWidgetTree()
{
	if (!WidgetTree || RootBorder)
	{
		return;
	}

	RootBorder = FMiningUIPresentation::Screen(WidgetTree);

	RootBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("RootBox"));
	FMiningUIPresentation::ScrollContent(WidgetTree, CastChecked<UAUTBorder>(RootBorder), RootBox);

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
	HintText = AddText(TEXT("HintText"), 16);
	StatusText = AddText(TEXT("StatusText"), 18);
	CostText = AddText(TEXT("CostText"), 18);

	AddButton(TEXT("UpgradeButton"), TEXT("UpgradeButtonText"), FText::FromString(TEXT("Upgrade")), UpgradeButton, UpgradeButtonText);
	AddButton(TEXT("SyncProductionButton"), TEXT("SyncProductionButtonText"), FText::FromString(TEXT("Sync Production")), SyncProductionButton, SyncProductionButtonText);
	AddButton(TEXT("AdvanceDayButton"), TEXT("AdvanceDayButtonText"), FText::FromString(TEXT("Advance Day")), AdvanceDayButton, AdvanceDayButtonText);
	AddButton(TEXT("CloseButton"), TEXT("CloseButtonText"), FText::FromString(TEXT("Close")), CloseButton, CloseButtonText);

	if (UpgradeButton)
	{
		UpgradeButton->OnClicked.AddDynamic(this, &ThisClass::OnUpgradeClicked);
	}

	if (SyncProductionButton)
	{
		SyncProductionButton->OnClicked.AddDynamic(this, &ThisClass::OnSyncProductionClicked);
	}

	if (AdvanceDayButton)
	{
		AdvanceDayButton->OnClicked.AddDynamic(this, &ThisClass::OnAdvanceDayClicked);
	}

	if (CloseButton)
	{
		CloseButton->OnClicked.AddDynamic(this, &ThisClass::OnCloseClicked);
	}
}

void UMiningSiteMenuWidget::RefreshUI()
{
	if (!TitleText || !HintText || !StatusText || !CostText)
	{
		return;
	}

	const UMiningSiteComponent* SiteComponent = IsValid(SiteActor) ? SiteActor->MiningSiteComponent : nullptr;
	const int32 CurrentLevel = SiteComponent ? SiteComponent->GetCurrentLevel() : INDEX_NONE;

	TitleText->SetText(FText::Format(
		FText::FromString(TEXT("Mining Site Level {0}")),
		FText::AsNumber(FMath::Max(0, CurrentLevel))));
	HintText->SetText(BuildHintText());
	StatusText->SetText(BuildStatusText());
	CostText->SetText(BuildCostText());
	UpdateActionLabels();

	if (UpgradeButton)
	{
		const bool bCanUpgrade = IsValid(SiteActor) && SiteActor->CanUpgradeFromInteraction(InteractingPawn);
		UpgradeButton->SetIsEnabled(bCanUpgrade);
	}

	if (SyncProductionButton)
	{
		SyncProductionButton->SetIsEnabled(SiteActor != nullptr);
	}

	if (AdvanceDayButton)
	{
		AdvanceDayButton->SetIsEnabled(SiteActor != nullptr);
	}
}

FText UMiningSiteMenuWidget::BuildStatusText() const
{
	const UMiningSiteComponent* SiteComponent = IsValid(SiteActor) ? SiteActor->MiningSiteComponent : nullptr;
	if (!SiteComponent)
	{
		return FText::FromString(TEXT("No mining site data."));
	}

	FMiningSiteLevelDefinition CurrentLevel;
	const bool bHasLevel = SiteComponent->GetCurrentLevelDefinition(CurrentLevel);
	const int32 AutoPerDay = bHasLevel ? CurrentLevel.AutomatedMineralsPerDay : 0;
	const int32 WorkerCount = bHasLevel ? CurrentLevel.WorkerCount : 0;
	const int32 GuardCount = bHasLevel ? CurrentLevel.GuardCount : 0;
	const int32 CourierCount = bHasLevel && CurrentLevel.bShipmentUnlocked && !CurrentLevel.CourierClass.IsNull() ? 1 : 0;
	const int32 ShipmentLossPercent = bHasLevel ? FMath::RoundToInt(CurrentLevel.ShipmentLossChance * 100.0f) : 0;
	const int32 WorkerDeathPercent = bHasLevel ? FMath::RoundToInt(CurrentLevel.WorkerDeathChance * 100.0f) : 0;

	return FText::Format(
		FText::FromString(TEXT("Stored: {0}/{1}\nManual Speed: {2}x\nAuto Production: {3}/day\nWorkers: {4}  Guards: {5}  Couriers: {6}\nShipment Loss: {7}%  Worker Death: {8}%")),
		FText::AsNumber(SiteComponent->GetStoredUnits()),
		FText::AsNumber(SiteComponent->GetStorageCapacity()),
		FText::AsNumber(SiteComponent->GetManualMiningSpeedMultiplier()),
		FText::AsNumber(AutoPerDay),
		FText::AsNumber(WorkerCount),
		FText::AsNumber(GuardCount),
		FText::AsNumber(CourierCount),
		FText::AsNumber(ShipmentLossPercent),
		FText::AsNumber(WorkerDeathPercent));
}

FText UMiningSiteMenuWidget::BuildCostText() const
{
	const UMiningSiteComponent* SiteComponent = IsValid(SiteActor) ? SiteActor->MiningSiteComponent : nullptr;
	if (!SiteComponent)
	{
		return FText::GetEmpty();
	}

	TArray<FMiningItemQuantity> UpgradeCost;
	SiteComponent->GetNextUpgradeCost(UpgradeCost);
	if (UpgradeCost.IsEmpty())
	{
		return FText::FromString(TEXT("No further upgrades."));
	}

	TArray<FString> Parts;
	Parts.Reserve(UpgradeCost.Num());
	for (const FMiningItemQuantity& CostEntry : UpgradeCost)
	{
		FString ItemName = CostEntry.ItemTag.IsValid() ? CostEntry.ItemTag.ToString() : TEXT("Unknown");
		if (const UClass* ItemClass = CostEntry.ItemClass.Get())
		{
			if (const UACFItem* DefaultItem = ItemClass->GetDefaultObject<UACFItem>())
			{
				ItemName = DefaultItem->GetItemName().ToString();
			}
			else
			{
				ItemName = ItemClass->GetName();
			}
		}

		Parts.Add(FString::Printf(TEXT("%s x%d"), *ItemName, CostEntry.Quantity));
	}

	return FText::FromString(FString::Printf(TEXT("Next Upgrade Cost:\n%s"), *FString::Join(Parts, TEXT("\n"))));
}

FText UMiningSiteMenuWidget::BuildHintText() const
{
	const UMiningSiteComponent* SiteComponent = IsValid(SiteActor) ? SiteActor->MiningSiteComponent : nullptr;
	if (!IsValid(SiteActor) || !SiteComponent)
	{
		return FText::FromString(TEXT("No site selected."));
	}

	const FString SettlementName = GetNameSafe(SiteActor->SettlementResourceActor);

	FMiningSiteLevelDefinition NextLevel;
	if (SiteComponent->GetNextLevelDefinition(NextLevel))
	{
		return FText::Format(
			FText::FromString(TEXT("Settlement: {0}\nNext Level: {1}  Storage: {2}  Auto/Day: {3}")),
			FText::FromString(SettlementName.IsEmpty() ? TEXT("None") : SettlementName),
			FText::AsNumber(NextLevel.Level),
			FText::AsNumber(NextLevel.StorageCapacity),
			FText::AsNumber(NextLevel.AutomatedMineralsPerDay));
	}

	return FText::Format(
		FText::FromString(TEXT("Settlement: {0}\nSite is at max level.")),
		FText::FromString(SettlementName.IsEmpty() ? TEXT("None") : SettlementName));
}

void UMiningSiteMenuWidget::UpdateActionLabels()
{
	if (UpgradeButtonText)
	{
		const UMiningSiteComponent* SiteComponent = IsValid(SiteActor) ? SiteActor->MiningSiteComponent : nullptr;
		FMiningSiteLevelDefinition NextLevel;
		if (SiteComponent && SiteComponent->GetNextLevelDefinition(NextLevel))
		{
			UpgradeButtonText->SetText(FText::Format(FText::FromString(TEXT("Upgrade To Level {0}")), FText::AsNumber(NextLevel.Level)));
		}
		else
		{
			UpgradeButtonText->SetText(FText::FromString(TEXT("Max Level Reached")));
		}
	}

	if (SyncProductionButtonText)
	{
		SyncProductionButtonText->SetText(FText::FromString(TEXT("Sync Elapsed Production")));
	}

	if (AdvanceDayButtonText)
	{
		AdvanceDayButtonText->SetText(FText::FromString(TEXT("Advance One Simulated Day")));
	}
}

void UMiningSiteMenuWidget::HandleRefreshTick()
{
	RefreshUI();
}

void UMiningSiteMenuWidget::CloseMenu()
{
	if (IsValid(SiteActor))
	{
		SiteActor->ClearActiveMenuWidget(this);
	}

	FMiningUIPresentation::Close(this);
}

void UMiningSiteMenuWidget::OnUpgradeClicked()
{
	if (IsValid(SiteActor))
	{
		SiteActor->ServerPurchaseNextUpgrade(InteractingPawn);
	}

	RefreshUI();
}

void UMiningSiteMenuWidget::OnSyncProductionClicked()
{
	if (IsValid(SiteActor))
	{
		SiteActor->ServerSyncProduction();
	}

	RefreshUI();
}

void UMiningSiteMenuWidget::OnAdvanceDayClicked()
{
	if (IsValid(SiteActor))
	{
		SiteActor->ServerAdvanceOneSimulatedDay();
	}

	RefreshUI();
}

void UMiningSiteMenuWidget::OnCloseClicked()
{
	CloseMenu();
}
