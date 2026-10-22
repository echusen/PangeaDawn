#include "Components/MiningSiteComponent.h"
#include "Components/PangeaMiningTransportComponent.h"

#include "Components/ACFStorageComponent.h"
#include "DataAssets/MiningSiteDefinition.h"
#include "Items/ACFItem.h"

namespace
{
TSubclassOf<UACFItem> ResolveACFItemClass(const FMiningItemQuantity& Item)
{
	UClass* RawItemClass = Item.ItemClass.Get();
	return RawItemClass && RawItemClass->IsChildOf(UACFItem::StaticClass()) ? RawItemClass : nullptr;
}
}

int32 UMiningSiteComponent::AddStoredItem(const FMiningItemQuantity& Item)
{
	if (!HasAuthority() || Item.Quantity <= 0 || !CanStoreItem(Item))
	{
		UE_LOG(LogPangeaMiningSite, Verbose, TEXT("AddStoredItem blocked. Owner=%s Quantity=%d Available=%d"),
			*GetNameSafe(GetOwner()),
			Item.Quantity,
			GetAvailableStorage());
		return 0;
	}

	const int32 AcceptedQuantity = FMath::Min(Item.Quantity, GetAvailableStorage());
	if (AcceptedQuantity <= 0)
	{
		return 0;
	}

	if (LinkedStorageComponent)
	{
		FMiningItemQuantity AcceptedItem = Item;
		AcceptedItem.Quantity = AcceptedQuantity;
		if (TSubclassOf<UACFItem> ItemClass = ResolveACFItemClass(AcceptedItem))
		{
			const int32 Before = GetStoredQuantity(AcceptedItem);
			LinkedStorageComponent->AddItem(FBaseItem(ItemClass, AcceptedQuantity));
			RebuildStoredItemsFromLinkedStorage();
			const int32 ActuallyAccepted = FMath::Clamp(GetStoredQuantity(AcceptedItem) - Before, 0, AcceptedQuantity);
			UE_LOG(LogPangeaMiningSite, Log, TEXT("Mining storage added %d/%d units via linked chest. Owner=%s Stored=%d Capacity=%d ItemClass=%s Tag=%s"),
				AcceptedQuantity,
				Item.Quantity,
				*GetNameSafe(GetOwner()),
				GetStoredUnits(),
				GetStorageCapacity(),
				*GetNameSafe(Item.ItemClass.Get()),
				*Item.ItemTag.ToString());
			return ActuallyAccepted;
		}
	}

	const int32 ExistingIndex = FindStoredItemIndex(Item);
	if (StoredItems.IsValidIndex(ExistingIndex))
	{
		StoredItems[ExistingIndex].Quantity += AcceptedQuantity;
	}
	else
	{
		FMiningStoredItem StoredItem;
		StoredItem.ItemClass = Item.ItemClass;
		StoredItem.ItemTag = Item.ItemTag;
		StoredItem.Quantity = AcceptedQuantity;
		StoredItems.Add(StoredItem);
	}

	OnStorageChanged.Broadcast();
	UE_LOG(LogPangeaMiningSite, Log, TEXT("Mining storage added %d/%d units. Owner=%s Stored=%d Capacity=%d ItemClass=%s Tag=%s"),
		AcceptedQuantity,
		Item.Quantity,
		*GetNameSafe(GetOwner()),
		GetStoredUnits(),
		GetStorageCapacity(),
		*GetNameSafe(Item.ItemClass.Get()),
		*Item.ItemTag.ToString());
	return AcceptedQuantity;
}

int32 UMiningSiteComponent::RemoveStoredItem(const FMiningItemQuantity& Item)
{
	if (!HasAuthority() || Item.Quantity <= 0)
	{
		return 0;
	}

	if (LinkedStorageComponent)
	{
		if (TSubclassOf<UACFItem> ItemClass = ResolveACFItemClass(Item))
		{
			const int32 Before = GetStoredQuantity(Item);
			int32 RemainingToRemove = Item.Quantity;
			TArray<FInventoryItem> MatchingItems;
			LinkedStorageComponent->GetAllItemsOfClassInInventory(ItemClass, MatchingItems);

			for (const FInventoryItem& InventoryItem : MatchingItems)
			{
				if (RemainingToRemove <= 0)
				{
					break;
				}

				const int32 AmountToRemove = FMath::Min(RemainingToRemove, InventoryItem.Count);
				if (AmountToRemove > 0)
				{
					LinkedStorageComponent->RemoveItem(InventoryItem, AmountToRemove);
					RemainingToRemove -= AmountToRemove;
				}
			}

			RebuildStoredItemsFromLinkedStorage();
			return FMath::Clamp(Before - GetStoredQuantity(Item), 0, Item.Quantity);
		}
	}

	int32 RemovedQuantity = 0;
	while (RemovedQuantity < Item.Quantity)
	{
		const int32 ExistingIndex = FindStoredItemIndex(Item);
		if (!StoredItems.IsValidIndex(ExistingIndex)) break;
		const int32 Amount = FMath::Min(Item.Quantity - RemovedQuantity, FMath::Max(0, StoredItems[ExistingIndex].Quantity));
		StoredItems[ExistingIndex].Quantity -= Amount;
		RemovedQuantity += Amount;
		if (StoredItems[ExistingIndex].Quantity <= 0) StoredItems.RemoveAt(ExistingIndex);
	}

	OnStorageChanged.Broadcast();
	return RemovedQuantity;
}

int32 UMiningSiteComponent::ProduceForElapsedSeconds(const float ElapsedSeconds)
{
	if (!IsTimingReady() || bTimingOperationInProgress || bUpgradeInProgress) return 0;
	TGuardValue<bool> Guard(bTimingOperationInProgress, true);
	return AdvanceProduction(double(ElapsedSeconds));
}

int32 UMiningSiteComponent::ProduceSegment(const double ElapsedSeconds)
{
	if (!FMath::IsFinite(ElapsedSeconds) || ElapsedSeconds <= 0.0)
	{
		UE_LOG(LogPangeaMiningSite, Verbose, TEXT("ProduceForElapsedSeconds blocked. Owner=%s Established=%s Elapsed=%.2f"),
			*GetNameSafe(GetOwner()),
			bEstablished ? TEXT("true") : TEXT("false"),
			ElapsedSeconds);
		return 0;
	}

	FMiningSiteLevelDefinition Definition;
	if (!GetCurrentLevelDefinition(Definition) || Definition.AutomatedMineralsPerDay <= 0 || !SiteDefinition ||
		!FMath::IsFinite(SiteDefinition->SimulatedDaySeconds) || SiteDefinition->SimulatedDaySeconds <= 0.0f ||
		(!Definition.PrimaryOutput.ItemClass && !Definition.PrimaryOutput.ItemTag.IsValid()))
	{
		UE_LOG(LogPangeaMiningSite, Verbose, TEXT("No mining automation. Owner=%s Level=%d AutoPerDay=%d"),
			*GetNameSafe(GetOwner()),
			CurrentLevel,
			Definition.AutomatedMineralsPerDay);
		return 0;
	}

	const int32 Available = GetAvailableStorage();
	if (Available <= 0)
	{
		ProductionRemainder = 0.0; // No hidden backlog while full.
		return 0;
	}
	const double DaySeconds = FMath::Max(1.0, double(SiteDefinition->SimulatedDaySeconds));
	// Levels with no authored workforce retain legacy passive-production semantics.
	const double StaffingRatio = Definition.WorkerCount > 0
		? double(GetLivingWorkerCount()) / Definition.WorkerCount : 1.0;
	const double Total = ProductionRemainder + (ElapsedSeconds / DaySeconds) * Definition.AutomatedMineralsPerDay * StaffingRatio;
	if (!FMath::IsFinite(Total)) return 0;
	// Small roundoff at exact unit boundaries must not depend on sync cadence.
	const double WholeUnits = FMath::FloorToDouble(Total + 1.e-9);
	const int32 ProducedQuantity = static_cast<int32>(FMath::Min(WholeUnits, double(Available)));
	ProductionRemainder = WholeUnits >= Available ? 0.0 : FMath::Max(0.0, Total - WholeUnits);
	if (ProducedQuantity <= 0)
	{
		return 0;
	}

	FMiningItemQuantity ProducedItem = Definition.PrimaryOutput;
	ProducedItem.Quantity = ProducedQuantity;
	const int32 AcceptedQuantity = AddStoredItem(ProducedItem);
	UE_LOG(LogPangeaMiningSite, Log, TEXT("Mining production tick. Owner=%s Elapsed=%.2f Requested=%d Accepted=%d"),
		*GetNameSafe(GetOwner()),
		ElapsedSeconds,
		ProducedQuantity,
		AcceptedQuantity);
	return AcceptedQuantity;
}

bool UMiningSiteComponent::TryAutoShipment()
{
	if (SiteDefinition && SiteDefinition->bUseHybridTransport)
	{
		UPangeaMiningTransportComponent* Transport = GetOwner() ? GetOwner()->FindComponentByClass<UPangeaMiningTransportComponent>() : nullptr;
		return Transport && Transport->TryDispatch();
	}
	if (!IsTimingReady() || bTimingOperationInProgress || bUpgradeInProgress) return false;
	TGuardValue<bool> Guard(bTimingOperationInProgress, true);
	AccountElapsedTime();
	return DispatchAutoShipment();
}

bool UMiningSiteComponent::DispatchAutoShipment()
{
	if (SiteDefinition && SiteDefinition->bUseHybridTransport) return false; // Hybrid ledger owns delivery; never also instant-grant.
	if (!HasAuthority() || !bEstablished)
	{
		UE_LOG(LogPangeaMiningSite, Verbose, TEXT("TryAutoShipment blocked. Owner=%s Established=%s"),
			*GetNameSafe(GetOwner()),
			bEstablished ? TEXT("true") : TEXT("false"));
		return false;
	}

	FMiningSiteLevelDefinition Definition;
	if (!GetCurrentLevelDefinition(Definition) || !Definition.bShipmentUnlocked || Definition.MaxShipmentUnits <= 0 ||
		!FMath::IsFinite(Definition.ShipmentIntervalSeconds) || Definition.ShipmentIntervalSeconds < 0.0f ||
		!FMath::IsFinite(Definition.ShipmentLossChance) || Definition.ShipmentLossChance < 0.0f || Definition.ShipmentLossChance > 1.0f)
	{
		UE_LOG(LogPangeaMiningSite, Verbose, TEXT("Shipment unavailable. Owner=%s Level=%d Unlocked=%s MaxShipment=%d"),
			*GetNameSafe(GetOwner()),
			CurrentLevel,
			Definition.bShipmentUnlocked ? TEXT("true") : TEXT("false"),
			Definition.MaxShipmentUnits);
		return false;
	}

	if (ShipmentCooldownRemaining > 0.0)
	{
		return false;
	}

	FMiningItemQuantity ShipmentItem = Definition.PrimaryOutput;
	ShipmentItem.Quantity = FMath::Min(Definition.MaxShipmentUnits, GetStoredQuantity(ShipmentItem));
	// Preserve existing empty-attempt cadence; commit before any inventory delegates.
	ShipmentCooldownRemaining = GetConfiguredShipmentInterval();
	if (ShipmentItem.Quantity <= 0)
	{
		return false;
	}

	const int32 RemovedQuantity = RemoveStoredItem(ShipmentItem);
	if (RemovedQuantity <= 0) return false;
	ShipmentItem.Quantity = RemovedQuantity;
	const bool bLost = FMath::FRand() < Definition.ShipmentLossChance;
	const int32 DeliveredQuantity = bLost ? 0 : RemovedQuantity;
	OnShipmentResolved.Broadcast(ShipmentItem, DeliveredQuantity, bLost);
	UE_LOG(LogPangeaMiningSite, Log, TEXT("Mining shipment resolved. Owner=%s Removed=%d Delivered=%d Lost=%s LossChance=%.2f"),
		*GetNameSafe(GetOwner()),
		RemovedQuantity,
		DeliveredQuantity,
		bLost ? TEXT("true") : TEXT("false"),
		Definition.ShipmentLossChance);
	return RemovedQuantity > 0;
}

bool UMiningSiteComponent::ReserveTransportCargo(FMiningItemQuantity& OutCargo)
{
	OutCargo = FMiningItemQuantity();
	if (!IsTimingReady() || bTimingOperationInProgress || bUpgradeInProgress || !SiteDefinition || !SiteDefinition->bUseHybridTransport) return false;
	const UPangeaMiningTransportComponent* Transport = GetOwner()->FindComponentByClass<UPangeaMiningTransportComponent>();
	if (!Transport || Transport->HasActiveShipment()) return false;
	TGuardValue<bool> Guard(bTimingOperationInProgress, true);
	AccountElapsedTime();
	FMiningSiteLevelDefinition Level;
	if (!GetCurrentLevelDefinition(Level) || !Level.bShipmentUnlocked || Level.MaxShipmentUnits <= 0 || ShipmentCooldownRemaining > 0.0 ||
		!FMath::IsFinite(Level.ShipmentIntervalSeconds) || Level.ShipmentIntervalSeconds < 0.f ||
		!FMath::IsFinite(Level.ShipmentLossChance) || Level.ShipmentLossChance < 0.f || Level.ShipmentLossChance > 1.f ||
		!Level.PrimaryOutput.ItemClass || !Level.PrimaryOutput.ItemClass->IsChildOf(UACFItem::StaticClass())) return false;
	OutCargo = Level.PrimaryOutput;
	OutCargo.Quantity = FMath::Min(Level.MaxShipmentUnits, GetStoredQuantity(OutCargo));
	if (OutCargo.Quantity <= 0) return false;
	ShipmentCooldownRemaining = GetConfiguredShipmentInterval();
	OutCargo.Quantity = RemoveStoredItem(OutCargo); // Exactly one withdrawal into the ledger, never per visual actor.
	GetOwner()->ForceNetUpdate();
	return OutCargo.Quantity > 0;
}

void UMiningSiteComponent::SyncProductionFromWorldTime()
{
	if (!IsTimingReady() || bTimingOperationInProgress || bUpgradeInProgress)
	{
		return;
	}

	TGuardValue<bool> Guard(bTimingOperationInProgress, true);
	AccountElapsedTime();
	DispatchAutoShipment();
	UE_LOG(LogPangeaMiningSite, Log, TEXT("Mining site synced from world time. Owner=%s Stored=%d Capacity=%d"),
		*GetNameSafe(GetOwner()),
		GetStoredUnits(),
		GetStorageCapacity());
}

void UMiningSiteComponent::SetLinkedStorageComponent(UACFStorageComponent* InStorageComponent)
{
	if (!HasAuthority()) return;
	if (LinkedStorageComponent == InStorageComponent)
	{
		return;
	}

	if (LinkedStorageComponent && LinkedStorageComponent->OnInventoryChanged.IsAlreadyBound(this, &ThisClass::HandleLinkedStorageChanged))
	{
		LinkedStorageComponent->OnInventoryChanged.RemoveDynamic(this, &ThisClass::HandleLinkedStorageChanged);
	}

	LinkedStorageComponent = InStorageComponent;

	if (!LinkedStorageComponent)
	{
		return;
	}

	if (!LinkedStorageComponent->OnInventoryChanged.IsAlreadyBound(this, &ThisClass::HandleLinkedStorageChanged))
	{
		LinkedStorageComponent->OnInventoryChanged.AddDynamic(this, &ThisClass::HandleLinkedStorageChanged);
	}

	if (LinkedStorageComponent->GetInventory().IsEmpty())
	{
		// AddItem broadcasts synchronously and rebuilds StoredItems. Iterate a snapshot.
		const TArray<FMiningStoredItem> SavedItems = StoredItems;
		for (const FMiningStoredItem& StoredItem : SavedItems)
		{
			FMiningItemQuantity MiningItem;
			MiningItem.ItemClass = StoredItem.ItemClass;
			MiningItem.ItemTag = StoredItem.ItemTag;
			MiningItem.Quantity = StoredItem.Quantity;

			if (TSubclassOf<UACFItem> ItemClass = ResolveACFItemClass(MiningItem))
			{
				LinkedStorageComponent->AddItem(FBaseItem(ItemClass, StoredItem.Quantity));
			}
		}
	}

	RebuildStoredItemsFromLinkedStorage();
}

void UMiningSiteComponent::RebuildStoredItemsFromLinkedStorage()
{
	if (!LinkedStorageComponent)
	{
		return;
	}

	StoredItems.Reset();

	for (const FInventoryItem& InventoryItem : LinkedStorageComponent->GetInventory())
	{
		if (!InventoryItem.ItemClass || InventoryItem.Count <= 0)
		{
			continue;
		}

		FMiningStoredItem StoredItem;
		StoredItem.ItemClass = InventoryItem.ItemClass;
		StoredItem.Quantity = InventoryItem.Count;
		StoredItems.Add(StoredItem);
	}

	OnStorageChanged.Broadcast();
}
