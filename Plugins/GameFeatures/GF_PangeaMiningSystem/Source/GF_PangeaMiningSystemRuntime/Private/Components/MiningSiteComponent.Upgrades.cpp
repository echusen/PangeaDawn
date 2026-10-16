#include "Components/MiningSiteComponent.h"

#include "Components/ACFInventoryComponent.h"
#include "DataAssets/MiningSiteDefinition.h"
#include "GameFramework/Actor.h"
#include "Items/ACFItem.h"
#include "Misc/AutomationTest.h"

namespace
{
UACFInventoryComponent* ResolveInventoryComponentFromContext(UObject* UpgradeContext)
{
	if (!UpgradeContext)
	{
		return nullptr;
	}

	if (AActor* Actor = Cast<AActor>(UpgradeContext))
	{
		return Actor->FindComponentByClass<UACFInventoryComponent>();
	}

	if (UActorComponent* ActorComponent = Cast<UActorComponent>(UpgradeContext))
	{
		if (AActor* Owner = ActorComponent->GetOwner())
		{
			return Owner->FindComponentByClass<UACFInventoryComponent>();
		}
	}

	return nullptr;
}

bool BuildRequiredItems(const TArray<FMiningItemQuantity>& Cost, TArray<FBaseItem>& OutItems)
{
	OutItems.Reset();
	OutItems.Reserve(Cost.Num());

	for (const FMiningItemQuantity& CostEntry : Cost)
	{
		UClass* RawItemClass = CostEntry.ItemClass.Get();
		UClass* ACFItemClass = RawItemClass && RawItemClass->IsChildOf(UACFItem::StaticClass()) ? RawItemClass : nullptr;
		if (!ACFItemClass || CostEntry.Quantity <= 0)
		{
			return false;
		}

		// Multiple lines for the same resource are one cost, not independent checks
		// against the same stack (which allowed purchasing with too few items).
		FBaseItem* Existing = OutItems.FindByPredicate([ACFItemClass](const FBaseItem& Item)
		{
			return Item.ItemClass == ACFItemClass;
		});
		if (Existing)
		{
			if (Existing->Count > MAX_int32 - CostEntry.Quantity) return false;
			Existing->Count += CostEntry.Quantity;
		}
		else
		{
			OutItems.Add(FBaseItem(ACFItemClass, CostEntry.Quantity));
		}
	}

	return true;
}
}

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPangeaMiningCostTest, "Pangea.ACF.Mining.AggregatesInventoryCosts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPangeaMiningCostTest::RunTest(const FString& Parameters)
{
	TArray<FBaseItem> Items;
	FMiningItemQuantity Cost;
	Cost.ItemClass = UACFItem::StaticClass();
	Cost.Quantity = 3;
	TestTrue(TEXT("Valid duplicate costs are accepted"), BuildRequiredItems({Cost, Cost}, Items));
	TestEqual(TEXT("Duplicate classes have one combined requirement"), Items.Num(), 1);
	if (Items.Num() == 1) TestEqual(TEXT("Both lines must be paid"), Items[0].Count, 6);
	Cost.Quantity = MAX_int32;
	TestFalse(TEXT("An overflowing combined cost is rejected"), BuildRequiredItems({Cost, Cost}, Items));
	Cost.Quantity = 0;
	TestFalse(TEXT("Zero costs are rejected"), BuildRequiredItems({Cost}, Items));
	Cost.Quantity = 1;
	Cost.ItemClass = UObject::StaticClass();
	TestFalse(TEXT("Non-ACF item classes are rejected"), BuildRequiredItems({Cost}, Items));
	TestTrue(TEXT("An empty cost is valid"), BuildRequiredItems({}, Items));
	TestEqual(TEXT("Output is reset for an empty cost"), Items.Num(), 0);
	return true;
}
#endif

bool UMiningSiteComponent::EstablishSite()
{
	if (!HasAuthority() || bEstablished || !SiteDefinition)
	{
		UE_LOG(LogPangeaMiningSite, Warning, TEXT("EstablishSite blocked. Owner=%s HasAuthority=%s bEstablished=%s SiteDefinition=%s"),
			*GetNameSafe(GetOwner()),
			HasAuthority() ? TEXT("true") : TEXT("false"),
			bEstablished ? TEXT("true") : TEXT("false"),
			*GetNameSafe(SiteDefinition));
		return false;
	}

	FMiningSiteLevelDefinition LevelZero;
	if (!SiteDefinition->GetLevelDefinition(0, LevelZero))
	{
		UE_LOG(LogPangeaMiningSite, Warning, TEXT("EstablishSite failed on %s: site definition %s has no level 0."),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(SiteDefinition));
		return false;
	}

	bEstablished = true;
	CurrentLevel = 0;
	LastProductionWorldSeconds = GetWorldSeconds();
	LastShipmentWorldSeconds = LastProductionWorldSeconds;
	UE_LOG(LogPangeaMiningSite, Log, TEXT("Mining site established. Owner=%s Definition=%s Level=%d StorageCapacity=%d ManualSpeed=%.2f"),
		*GetNameSafe(GetOwner()),
		*GetNameSafe(SiteDefinition),
		CurrentLevel,
		LevelZero.StorageCapacity,
		LevelZero.ManualMiningSpeedMultiplier);
	OnSiteLevelChanged.Broadcast(INDEX_NONE, CurrentLevel);
	return true;
}

bool UMiningSiteComponent::CanUpgradeSite() const
{
	if (!bEstablished || !SiteDefinition)
	{
		return false;
	}

	FMiningSiteLevelDefinition NextLevel;
	return GetNextLevelDefinition(NextLevel);
}

bool UMiningSiteComponent::UpgradeSite()
{
	if (!HasAuthority() || !CanUpgradeSite())
	{
		UE_LOG(LogPangeaMiningSite, Warning, TEXT("UpgradeSite blocked. Owner=%s HasAuthority=%s CurrentLevel=%d Definition=%s"),
			*GetNameSafe(GetOwner()),
			HasAuthority() ? TEXT("true") : TEXT("false"),
			CurrentLevel,
			*GetNameSafe(SiteDefinition));
		return false;
	}

	const int32 OldLevel = CurrentLevel;
	CurrentLevel++;
	LastProductionWorldSeconds = GetWorldSeconds();
	OnSiteLevelChanged.Broadcast(OldLevel, CurrentLevel);
	UE_LOG(LogPangeaMiningSite, Log, TEXT("Mining site upgraded. Owner=%s OldLevel=%d NewLevel=%d StorageCapacity=%d AutoPerDay=%d"),
		*GetNameSafe(GetOwner()),
		OldLevel,
		CurrentLevel,
		GetStorageCapacity(),
		[&]() { FMiningSiteLevelDefinition Definition; return GetCurrentLevelDefinition(Definition) ? Definition.AutomatedMineralsPerDay : 0; }());
	return true;
}

bool UMiningSiteComponent::CanPurchaseUpgrade(UObject* UpgradeContext) const
{
	if (!CanUpgradeSite())
	{
		return false;
	}

	TArray<FMiningItemQuantity> Cost;
	GetNextUpgradeCost(Cost);
	return CanPayUpgradeCost(UpgradeContext, Cost);
}

bool UMiningSiteComponent::PurchaseUpgrade(UObject* UpgradeContext)
{
	if (!HasAuthority() || !CanPurchaseUpgrade(UpgradeContext))
	{
		return false;
	}

	TArray<FMiningItemQuantity> Cost;
	GetNextUpgradeCost(Cost);
	return ConsumeUpgradeCost(UpgradeContext, Cost) && UpgradeSite();
}

bool UMiningSiteComponent::CanPayUpgradeCost_Implementation(UObject* UpgradeContext, const TArray<FMiningItemQuantity>& Cost) const
{
	if (Cost.IsEmpty())
	{
		return true;
	}

	UACFInventoryComponent* InventoryComponent = ResolveInventoryComponentFromContext(UpgradeContext);
	if (!InventoryComponent)
	{
		UE_LOG(LogPangeaMiningSite, Warning, TEXT("CanPayUpgradeCost failed: no inventory component. Owner=%s UpgradeContext=%s"),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(UpgradeContext));
		return false;
	}

	TArray<FBaseItem> RequiredItems;
	if (!BuildRequiredItems(Cost, RequiredItems))
	{
		UE_LOG(LogPangeaMiningSite, Warning, TEXT("CanPayUpgradeCost failed: invalid required items. Owner=%s UpgradeContext=%s"),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(UpgradeContext));
		return false;
	}

	bool bHasEnoughItems = true;
	for (const FBaseItem& RequiredItem : RequiredItems)
	{
		const int32 AvailableCount = InventoryComponent->GetTotalCountOfItemsByClass(RequiredItem.ItemClass);
		UE_LOG(LogPangeaMiningSite, Log, TEXT("CanPayUpgradeCost item check. Owner=%s UpgradeContext=%s ItemClass=%s Required=%d Available=%d"),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(UpgradeContext),
			*GetNameSafe(RequiredItem.ItemClass.Get()),
			RequiredItem.Count,
			AvailableCount);
		if (AvailableCount < RequiredItem.Count)
		{
			bHasEnoughItems = false;
		}
	}

	UE_LOG(LogPangeaMiningSite, Log, TEXT("CanPayUpgradeCost resolved. Owner=%s UpgradeContext=%s Result=%s"),
		*GetNameSafe(GetOwner()),
		*GetNameSafe(UpgradeContext),
		bHasEnoughItems ? TEXT("true") : TEXT("false"));
	return bHasEnoughItems;
}

bool UMiningSiteComponent::ConsumeUpgradeCost_Implementation(UObject* UpgradeContext, const TArray<FMiningItemQuantity>& Cost)
{
	if (Cost.IsEmpty())
	{
		return true;
	}

	UACFInventoryComponent* InventoryComponent = ResolveInventoryComponentFromContext(UpgradeContext);
	if (!InventoryComponent)
	{
		UE_LOG(LogPangeaMiningSite, Warning, TEXT("ConsumeUpgradeCost failed: no inventory component. Owner=%s UpgradeContext=%s"),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(UpgradeContext));
		return false;
	}

	TArray<FBaseItem> RequiredItems;
	if (!BuildRequiredItems(Cost, RequiredItems) || !InventoryComponent->HasEnoughItemsOfType(RequiredItems))
	{
		UE_LOG(LogPangeaMiningSite, Warning, TEXT("ConsumeUpgradeCost failed: insufficient items. Owner=%s UpgradeContext=%s"),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(UpgradeContext));
		return false;
	}

	for (const FBaseItem& RequiredItem : RequiredItems)
	{
		int32 RemainingToConsume = RequiredItem.Count;
		TArray<FInventoryItem> MatchingItems;
		InventoryComponent->GetAllItemsOfClassInInventory(RequiredItem.ItemClass, MatchingItems);

		for (const FInventoryItem& InventoryItem : MatchingItems)
		{
			if (RemainingToConsume <= 0)
			{
				break;
			}

			const int32 AmountToRemove = FMath::Min(RemainingToConsume, InventoryItem.Count);
			if (AmountToRemove > 0)
			{
				InventoryComponent->RemoveItem(InventoryItem, AmountToRemove);
				RemainingToConsume -= AmountToRemove;
			}
		}

		if (RemainingToConsume > 0)
		{
			UE_LOG(LogPangeaMiningSite, Error, TEXT("ConsumeUpgradeCost incomplete removal. Owner=%s UpgradeContext=%s ItemClass=%s Requested=%d Remaining=%d"),
				*GetNameSafe(GetOwner()),
				*GetNameSafe(UpgradeContext),
				*GetNameSafe(RequiredItem.ItemClass.Get()),
				RequiredItem.Count,
				RemainingToConsume);
			return false;
		}
	}

	return true;
}
