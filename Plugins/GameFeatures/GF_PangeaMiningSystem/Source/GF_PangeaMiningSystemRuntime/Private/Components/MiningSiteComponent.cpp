#include "Components/MiningSiteComponent.h"

#include "Components/ACFStorageComponent.h"
#include "DataAssets/MiningSiteDefinition.h"
#include "Net/UnrealNetwork.h"

DEFINE_LOG_CATEGORY(LogPangeaMiningSite);

UMiningSiteComponent::UMiningSiteComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 1.0f;
	SetIsReplicatedByDefault(true);
}

void UMiningSiteComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (IsTimingReady() && !bTimingOperationInProgress && !bUpgradeInProgress)
	{
		TGuardValue<bool> Guard(bTimingOperationInProgress, true);
		AccountElapsedTime(); // Day/production accounting only; transport owns dispatch policy.
	}
}

void UMiningSiteComponent::BeginPlay()
{
	Super::BeginPlay();

	if (IsTimingReady() && !bClockInitialized)
	{
		LastAccountingWorldSeconds = GetWorldSeconds();
		bClockInitialized = true;
		ShipmentCooldownRemaining = GetConfiguredShipmentInterval();
	}
}

int32 UMiningSiteComponent::GetStorageCapacity() const
{
	FMiningSiteLevelDefinition Definition;
	return GetCurrentLevelDefinition(Definition) ? FMath::Max(0, Definition.StorageCapacity) : 0;
}

int32 UMiningSiteComponent::GetStoredUnits() const
{
	int64 StoredUnits = 0;
	for (const FMiningStoredItem& Item : StoredItems)
	{
		StoredUnits += FMath::Max(0, Item.Quantity);
	}

	return static_cast<int32>(FMath::Min(StoredUnits, int64(MAX_int32)));
}

int32 UMiningSiteComponent::GetAvailableStorage() const
{
	return FMath::Max(0, GetStorageCapacity() - GetStoredUnits());
}

int32 UMiningSiteComponent::GetStoredQuantity(const FMiningItemQuantity& Item) const
{
	int64 Quantity = 0;
	for (const FMiningStoredItem& StoredItem : StoredItems)
	{
		if ((Item.ItemClass && StoredItem.ItemClass == Item.ItemClass) ||
			(!Item.ItemClass && Item.ItemTag.IsValid() && StoredItem.ItemTag == Item.ItemTag))
			Quantity += FMath::Max(0, StoredItem.Quantity);
	}
	return static_cast<int32>(FMath::Min(Quantity, int64(MAX_int32)));
}

float UMiningSiteComponent::GetManualMiningSpeedMultiplier() const
{
	FMiningSiteLevelDefinition Definition;
	return GetCurrentLevelDefinition(Definition) ? Definition.ManualMiningSpeedMultiplier : 1.0f;
}

bool UMiningSiteComponent::GetCurrentLevelDefinition(FMiningSiteLevelDefinition& OutDefinition) const
{
	return SiteDefinition ? SiteDefinition->GetLevelDefinition(CurrentLevel, OutDefinition) : false;
}

bool UMiningSiteComponent::GetNextLevelDefinition(FMiningSiteLevelDefinition& OutDefinition) const
{
	return SiteDefinition && CurrentLevel < MAX_int32 ? SiteDefinition->GetLevelDefinition(CurrentLevel + 1, OutDefinition) : false;
}

void UMiningSiteComponent::GetNextUpgradeCost(TArray<FMiningItemQuantity>& OutCost) const
{
	OutCost.Reset();

	FMiningSiteLevelDefinition NextLevel;
	if (GetNextLevelDefinition(NextLevel))
	{
		OutCost = NextLevel.UpgradeCost;
	}
}

bool UMiningSiteComponent::HasAuthority() const
{
	const AActor* Owner = GetOwner();
	return Owner && Owner->HasAuthority();
}

int32 UMiningSiteComponent::FindStoredItemIndex(const FMiningItemQuantity& Item) const
{
	for (int32 Index = 0; Index < StoredItems.Num(); ++Index)
	{
		const FMiningStoredItem& StoredItem = StoredItems[Index];
		const bool bClassMatches = Item.ItemClass && StoredItem.ItemClass == Item.ItemClass;
		const bool bTagMatches = !Item.ItemClass && Item.ItemTag.IsValid() && StoredItem.ItemTag == Item.ItemTag;
		if (bClassMatches || bTagMatches)
		{
			return Index;
		}
	}

	return INDEX_NONE;
}

bool UMiningSiteComponent::CanStoreItem(const FMiningItemQuantity& Item) const
{
	if (!SiteDefinition || !Item.ItemTag.IsValid() || SiteDefinition->AllowedStorageItemTags.IsEmpty())
	{
		return true;
	}

	for (const FGameplayTag& AllowedTag : SiteDefinition->AllowedStorageItemTags)
	{
		if (Item.ItemTag.MatchesTag(AllowedTag))
		{
			return true;
		}
	}

	return false;
}

double UMiningSiteComponent::GetWorldSeconds() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetTimeSeconds() : 0.0;
}

void UMiningSiteComponent::HandleLinkedStorageChanged()
{
	RebuildStoredItemsFromLinkedStorage();
}

void UMiningSiteComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UMiningSiteComponent, bEstablished);
	DOREPLIFETIME(UMiningSiteComponent, SiteDefinition);
	DOREPLIFETIME(UMiningSiteComponent, CurrentLevel);
	DOREPLIFETIME(UMiningSiteComponent, StoredItems);
	DOREPLIFETIME(UMiningSiteComponent, ProductionRemainder);
	DOREPLIFETIME(UMiningSiteComponent, ShipmentCooldownRemaining);
	DOREPLIFETIME(UMiningSiteComponent, WorkerVacancies);
	DOREPLIFETIME(UMiningSiteComponent, MiningDayElapsedSeconds);
	DOREPLIFETIME(UMiningSiteComponent, CompletedMiningDays);
}
