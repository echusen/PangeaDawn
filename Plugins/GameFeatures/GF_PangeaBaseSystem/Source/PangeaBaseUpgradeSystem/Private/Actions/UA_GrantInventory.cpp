#include "Actions/UA_GrantInventory.h"
#include "ACFItemSystemFunctionLibrary.h"
#include "Components/ACFInventoryComponent.h"
#include "GameFramework/Pawn.h"

namespace
{
bool GetGrantTotals(const TArray<FBaseItem>& Items, TMap<TSubclassOf<UACFItem>, int32>& Totals, FText& Failure)
{
    Totals.Reset();
    for (const FBaseItem& Item : Items)
    {
        FItemDescriptor Data;
        if (!Item.ItemClass || Item.Count <= 0 || Item.ItemClass->HasAnyClassFlags(CLASS_Abstract) ||
            !UACFItemSystemFunctionLibrary::GetItemData(Item.ItemClass, Data) || Data.MaxInventoryStack <= 0 ||
            !FMath::IsFinite(Data.ItemWeight) || Data.ItemWeight < 0.f ||
            static_cast<int64>(Totals.FindRef(Item.ItemClass)) + Item.Count > MAX_int32)
        {
            Failure = FText::FromString(TEXT("Inventory grant has invalid item data/count or count overflow."));
            return false;
        }
        Totals.FindOrAdd(Item.ItemClass) += Item.Count;
    }
    return true;
}
}

bool UUA_GrantInventory::ValidateConfiguration_Implementation(FText& OutFailure) const
{
    TMap<TSubclassOf<UACFItem>, int32> Totals;
    return GetGrantTotals(ItemsToGrant, Totals, OutFailure);
}

bool UUA_GrantInventory::CanExecuteUpgrade_Implementation(const FBaseUpgradeContext& Context, FText& OutFailure) const
{
    TMap<TSubclassOf<UACFItem>, int32> Totals;
    if (!GetGrantTotals(ItemsToGrant, Totals, OutFailure)) return false;
    if (Totals.IsEmpty()) return true;
    UACFInventoryComponent* Inventory = Context.InteractingPawn ? Context.InteractingPawn->FindComponentByClass<UACFInventoryComponent>() : nullptr;
    if (!Inventory)
    {
        OutFailure = FText::FromString(TEXT("Inventory reward requires the interacting pawn's ACF inventory."));
        return false;
    }
    for (const auto& Total : Totals)
        if (Inventory->NumberOfItemCanTake(Total.Key) < Total.Value)
        {
            OutFailure = FText::FromString(TEXT("Insufficient inventory capacity for the reward."));
            return false;
        }
    return true;
}

bool UUA_GrantInventory::TryExecuteUpgrade_Implementation(const FBaseUpgradeContext& Context, FText& OutFailure)
{
    if (!CanExecuteUpgrade(Context, OutFailure)) return false;
    if (ItemsToGrant.IsEmpty()) return true;
    UACFInventoryComponent* Inventory = Context.InteractingPawn ? Context.InteractingPawn->FindComponentByClass<UACFInventoryComponent>() : nullptr;
    if (!Inventory || !Inventory->GetOwner() || !Inventory->GetOwner()->HasAuthority()) return false;
    // ACF's RPC returns void. Verify the actual synchronous authority-side count delta.
    // A failure may be partial; the owning upgrade command records RecoveryRequired, never retries blindly.
    for (const FBaseItem& Item : ItemsToGrant)
    {
        const int64 Before = Inventory->GetTotalCountOfItemsByClass(Item.ItemClass);
        if (Inventory->NumberOfItemCanTake(Item.ItemClass) < Item.Count)
        {
            OutFailure = FText::FromString(TEXT("Reward capacity changed during execution; inspect partial effects."));
            return false;
        }
        Inventory->AddItemToInventory(Item, false);
        if (!IsValid(Inventory) || static_cast<int64>(Inventory->GetTotalCountOfItemsByClass(Item.ItemClass)) - Before != Item.Count)
        {
            OutFailure = FText::FromString(TEXT("ACF did not confirm the full inventory reward; inspect partial effects."));
            return false;
        }
    }
    return true;
}

void UUA_GrantInventory::ExecuteForContext(const FBaseUpgradeContext& Context)
{
    FText Failure;
    TryExecuteUpgrade(Context, Failure);
}

void UUA_GrantInventory::Execute_Implementation(UObject* ContextObject)
{
    FBaseUpgradeContext Context;
    Context.SourceObject = ContextObject;
    Context.InteractingPawn = Cast<APawn>(ContextObject);
    ExecuteForContext(Context);
}
