// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2025. All Rights Reserved.

#include "Components/ACFInventoryComponent.h"
#include "ACFItemSystemFunctionLibrary.h"
#include "Components/ACFStorageComponent.h"
#include "Items/ACFConsumable.h"
#include "Items/ACFItem.h"
#include "Logging.h"
#include <GameFramework/Character.h>
#include <GameplayTagContainer.h>
#include <GenericPlatform/GenericPlatformMath.h>
#include <Kismet/KismetMathLibrary.h>
#include <Net/Serialization/FastArraySerializer.h>
#include <Net/UnrealNetwork.h>

FInventoryItem::FInventoryItem(const FBaseItem& inItem)
{
    Count = inItem.Count;
    ItemGuid = inItem.GetItemGuid();
    ItemClass = inItem.ItemClass.Get();
}

FInventoryItem::FInventoryItem(const FStartingItem& inItem)
{
    Count = inItem.Count;
    ItemGuid = FGuid::NewGuid();
    ItemClass = inItem.ItemClass.Get();
    DropChancePercentage = inItem.DropChancePercentage;
};

TArray<FInventoryItem> FACFInventoryList::GetAllItems() const
{
    return Inventory;
}

bool FACFInventoryList::GetItemByIndex(const int32 index, FInventoryItem& outItem) const
{
    if (Inventory.IsValidIndex(index)) {
        outItem = Inventory[index];
        return true;
    }
    return false;
}

bool FACFInventoryList::ContainsItem(const FInventoryItem& Item) const
{
    return Inventory.Contains(Item);
}

bool FACFInventoryList::Contains(const FGuid& ItemGUID) const
{
    return Inventory.Contains(ItemGUID);
}



// Sets default values for this component's properties
UACFInventoryComponent::UACFInventoryComponent()
{
    // Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
    // off to improve performance if you don't need them.
    PrimaryComponentTick.bCanEverTick = false;
    InventoryList.Init(GetOwner());

    // ...
}

// Called when the game starts
void UACFInventoryComponent::BeginPlay()
{
    Super::BeginPlay();
    actorOwner = GetOwner();
    InventoryList.Init(actorOwner);
    // ...
}

void UACFInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(UACFInventoryComponent, InventoryList);
    DOREPLIFETIME(UACFInventoryComponent, currentInventoryWeight);
}

int32 UACFInventoryComponent::NumberOfItemCanTake(const TSubclassOf<UACFItem>& itemToCheck)
{
    int32 addeditemstotal = 0;
    TArray<FInventoryItem> outItems;
    GetAllItemsOfClassInInventory(itemToCheck, outItems);
    FItemDescriptor itemInfo;
    UACFItemSystemFunctionLibrary::GetItemData(itemToCheck, itemInfo);
    float MaxByWeight = 999.f;
    if (itemInfo.ItemWeight > 0) {
        MaxByWeight = (MaxInventoryWeight - currentInventoryWeight) / itemInfo.ItemWeight;
    }
    const int32 maxAddableByWeight = FMath::TruncToInt(MaxByWeight);
    const int32 FreeSpaceInInventory = MaxInventorySlots - GetInventoryList().Num();
    int32 maxAddableByStack = FreeSpaceInInventory * itemInfo.MaxInventoryStack;
    // IF WE ALREADY HAVE SOME ITEMS LIKE THAT, INCREMENT ACTUAL VALUE
    if (outItems.Num() > 0) {
        for (const auto& outItem : outItems) {
            maxAddableByStack += itemInfo.MaxInventoryStack - outItem.Count;
        }
    }
    addeditemstotal = FGenericPlatformMath::Min(maxAddableByStack, maxAddableByWeight);
    return addeditemstotal;
}

void UACFInventoryComponent::RefreshTotalWeight()
{
    currentInventoryWeight = 0.f;
    const auto& allItems = GetInventoryList().GetAllItems();

    for (const auto& item : allItems) {
        FItemDescriptor itemInfo;
        if (!UACFItemSystemFunctionLibrary::GetItemData(item.ItemClass, itemInfo)) {
            return;
        }
        currentInventoryWeight += itemInfo.ItemWeight * item.Count;
    }
}

void UACFInventoryComponent::AddItemToInventory_Implementation(const FBaseItem& ItemToAdd, bool bAutoEquip)
{
    Internal_AddItem(ItemToAdd, bAutoEquip);
}

void UACFInventoryComponent::AddInventoryItem_Implementation(const FInventoryItem& ItemToAdd)
{
    Internal_AddInventoryItem(ItemToAdd);
}

void UACFInventoryComponent::AddItemToInventoryByClass_Implementation(TSubclassOf<UACFItem> inItem, int32 count /*= 1*/, bool bAutoEquip)
{
    AddItemToInventory(FBaseItem(inItem, count), bAutoEquip);
}

void UACFInventoryComponent::RemoveItemByIndex_Implementation(const int32 index, int32 count /*= 1*/)
{
    FInventoryItem outItem;
    if (GetInventoryList().GetItemByIndex(index, outItem)) {
        RemoveItem(outItem, count);
    }
}

void UACFInventoryComponent::RemoveItem_Implementation(const FInventoryItem& item, int32 count /*= 1*/)
{
    FInventoryItem outItem;

    if (GetItemByGuid(item.GetItemGuid(), outItem)) {
        const int32 finalCount = FMath::Min(count, outItem.Count);
        FItemDescriptor itemInfo;
        if (!UACFItemSystemFunctionLibrary::GetItemData(outItem.ItemClass, itemInfo)) {
            return;
        }
        const float weightRemoved = finalCount * itemInfo.ItemWeight;
        outItem.Count -= finalCount;

        HandleItemRemoved(outItem, count);
        if (outItem.Count <= 0) {
            GetInventoryList().RemoveEntry(outItem);
        } else {
            GetInventoryList().ChangeEntry(outItem);
        }

        currentInventoryWeight -= weightRemoved;
        OnItemRemoved.Broadcast(FBaseItem(item.ItemClass, finalCount));

        OnInventoryChanged.Broadcast();
    }
}

void UACFInventoryComponent::InitializeStartingItems()
{
	if (GetOwner()->HasAuthority()) {
		GetInventoryList().Empty();
		currentInventoryWeight = 0.f;
		for (const FStartingItem& item : GetStartingItems()) {
			Internal_AddItem(item, item.bAutoEquip, item.DropChancePercentage);
			if (GetInventoryList().Num() > MaxInventorySlots) {
				UE_LOG(ACFInventoryLog, Log, TEXT("Invalid Inventory setup, too many slots on character!!! - "
					"ACFInventoryComp"));
			}
		}
	}
}

bool UACFInventoryComponent::HasEnoughItemsOfType(const TArray<FBaseItem>& ItemsToCheck) const
{
    for (const FBaseItem& item : ItemsToCheck) {
        int32 numberToCheck = item.Count;
        TArray<FInventoryItem> invItems;

        GetAllItemsOfClassInInventory(item.ItemClass, invItems);
        int32 TotItems = 0;
        for (const auto& invItem : invItems) {
            TotItems += invItem.Count;
        }

        if (TotItems < numberToCheck) {
            return false;
        }
    }
    return true;
}

bool UACFInventoryComponent::HasAnyItemOfType(const TSubclassOf<UACFItem>& itemToCheck) const
{
    const auto& allItems = GetInventoryListConst().GetAllItems();

    for (const auto& item : allItems) {
        if (item.ItemClass == itemToCheck) {
            return true;
        }
    }
    return false;
}

void UACFInventoryComponent::ConsumeItems_Implementation(const TArray<FBaseItem>& ItemsToCheck)
{
    for (const auto& item : ItemsToCheck) {
        TArray<FInventoryItem> invItems;
        GetAllItemsOfClassInInventory(item.ItemClass, invItems);
        if (invItems.IsValidIndex(0)) {
            RemoveItem((invItems[0]), item.Count);
        }
    }
}

void UACFInventoryComponent::MoveItemsFromInventory_Implementation(const TArray<FInventoryItem>& inItems, UACFInventoryComponent* sourceInventory)
{
    if (!sourceInventory) {
        UE_LOG(ACFInventoryLog, Error,
            TEXT("Invalid StsourceInventoryorage, verify that the owner of this component is "
                 "replicated! - ACFEquipmentComp"));
        return;
    }
    for (const auto& item : inItems) {
        const int32 numItems = Internal_AddInventoryItem(item);
        if (numItems > 0) {
            sourceInventory->RemoveItem(item, numItems);
        }
    }
}

void UACFInventoryComponent::HandleItemRemoved(const FInventoryItem& item, int32 count)
{
}

void UACFInventoryComponent::HandleItemAdded(const FInventoryItem& item, int32 count /*= 1*/, bool bTryToEquip /*= true*/, FGameplayTag equipSlot)
{
}

void UACFInventoryComponent::OnRep_Inventory()
{
    OnInventoryChanged.Broadcast();
}

int32 UACFInventoryComponent::Internal_AddInventoryItem(const FInventoryItem& ItemToAdd, bool bTryToEquip)
{
    if (ItemToAdd.ItemClass) {
        FItemDescriptor itemData;
        UACFItemSystemFunctionLibrary::GetItemData(ItemToAdd.ItemClass, itemData);
        // we don't check the stack size if the max is 1, we just add the item to the inventory
        if (itemData.MaxInventoryStack == 1 && ItemToAdd.Count == 1 && NumberOfItemCanTake(ItemToAdd.ItemClass) > 0) {
            GetInventoryList().AddEntry(ItemToAdd);
            currentInventoryWeight += itemData.ItemWeight;
            OnInventoryChanged.Broadcast();
            OnItemAdded.Broadcast(FBaseItem(ItemToAdd.ItemClass, ItemToAdd.Count));
            HandleItemAdded(ItemToAdd, ItemToAdd.Count, bTryToEquip, FGameplayTag()); // HandleItemAdded with default equip slot
            return ItemToAdd.Count;
        } else {
            const int32 addedItems = Internal_AddItem(FBaseItem(ItemToAdd.ItemClass, ItemToAdd.Count), bTryToEquip, ItemToAdd.DropChancePercentage);
            if (addedItems > 0) {
                OnInventoryChanged.Broadcast();
                //OnItemAdded.Broadcast(FBaseItem(ItemToAdd.ItemClass, addedItems));
            }
            return addedItems;
        }
    } else {
        UE_LOG(ACFInventoryLog, Error, TEXT("Invalid ItemInstance in AddInventoryItem_Implementation! - UACFInventoryComponent"));
    }
    return -1;
}

int32 UACFInventoryComponent::Internal_AddItem(const FBaseItem& itemToAdd, bool bTryToEquip /*= false*/, float dropChancePercentage /*= 0.f*/, FGameplayTag equipSlot /*= FGameplayTag()*/)
{

    if (!itemToAdd.ItemClass) {
        UE_LOG(ACFInventoryLog, Warning,
            TEXT("Invalid ItemClass! - UACFEquipmentComponent::Internal_AddItem"));
        return -1;
    }
    int32 addeditemstotal = 0;
    int32 addeditemstmp = 0;
    bool bSuccessful = false;

    FItemDescriptor itemData;

    UACFItemSystemFunctionLibrary::GetItemData(itemToAdd.ItemClass, itemData);

    if (itemData.MaxInventoryStack == 0) {
        UE_LOG(ACFInventoryLog, Warning,
            TEXT("Max Inventory Stack cannot be 0!!!! - UACFEquipmentComponent::Internal_AddItem"));
        return -1;
    }

    if (currentInventoryWeight >= MaxInventoryWeight) {
        return -1;
    }
    const float itemweight = itemData.ItemWeight;
    int32 maxAddableByWeightTotal = itemToAdd.Count;
    if (itemweight > 0) {
        maxAddableByWeightTotal = FMath::TruncToInt(
            (MaxInventoryWeight - currentInventoryWeight) / itemweight);
    }
    int32 count = itemToAdd.Count;
    if (maxAddableByWeightTotal < itemToAdd.Count) {
        count = maxAddableByWeightTotal;
    }

    if (count <= 0) {
        return -1;
    }

    TArray<FInventoryItem> outItems;
    GetAllItemsOfClassInInventory(itemToAdd.ItemClass, outItems);
    // IF WE ALREADY HAVE SOME ITEMS LIKE THAT, INCREMENT ACTUAL VALUE

    if (outItems.Num() > 0) {
        for (auto& outItem : outItems) {
            if (outItem.Count < itemData.MaxInventoryStack) {
                // `count` is already constrained by the max inventory weight.
                if (outItem.Count + count <= itemData.MaxInventoryStack) {
                    addeditemstmp = count;
                } else {
                    int32 maxAddableByStack = itemData.MaxInventoryStack - outItem.Count;
                    addeditemstmp = maxAddableByStack;
                }

                outItem.Count += addeditemstmp;
                addeditemstotal += addeditemstmp;
                count -= addeditemstmp;
                outItem.DropChancePercentage = dropChancePercentage;
                GetInventoryList().ChangeEntry(outItem);
                HandleItemAdded(outItem, count, bTryToEquip, equipSlot);

                bSuccessful = true;
            }
        }
    }

    // If an existing item hasn't had its count incremeneted, then we add a new item
    const int32 NumberOfItemNeed = FMath::CeilToInt((float)count / (float)itemData.MaxInventoryStack);
    const int32 FreeSpaceInInventory = MaxInventorySlots - GetInventoryList().Num();
    const int32 NumberOfStackToCreate = FGenericPlatformMath::Min(NumberOfItemNeed, FreeSpaceInInventory);
    for (int32 i = 0; i < NumberOfStackToCreate; i++) {
        if (GetInventoryList().Num() < MaxInventorySlots) {
            FInventoryItem newItem(itemToAdd);
            if (count > itemData.MaxInventoryStack) {
                newItem.Count = itemData.MaxInventoryStack;
            } else {
                newItem.Count = count;
            }
            newItem.DropChancePercentage = dropChancePercentage;
            addeditemstotal += newItem.Count;
            count -= newItem.Count;
            GetInventoryList().AddEntry(newItem);

            HandleItemAdded(newItem, count, bTryToEquip, equipSlot);

            bSuccessful = true;
        }
    }
    if (bSuccessful) {
        currentInventoryWeight += itemData.ItemWeight * addeditemstotal;
        OnInventoryChanged.Broadcast();
        if (addeditemstotal > 0) {
            OnItemAdded.Broadcast(FBaseItem(itemToAdd.ItemClass, addeditemstotal));
        }
        return addeditemstotal;
    }

    return addeditemstotal;
}

bool UACFInventoryComponent::GetItemByGuid(const FGuid& itemGuid, FInventoryItem& outItem) const
{
    return GetInventoryListConst().GetItem(itemGuid, outItem);
}

int32 UACFInventoryComponent::GetTotalCountOfItemsByClass(const TSubclassOf<UACFItem>& ItemClass) const
{
    int32 totalItems = 0;
    TArray<FInventoryItem> outItems;
    GetAllItemsOfClassInInventory(ItemClass, outItems);

    for (const auto& item : outItems) {
        totalItems += item.Count;
    }
    return totalItems;
}

void UACFInventoryComponent::GetAllItemsOfClassInInventory(const TSubclassOf<UACFItem>& ItemClass, TArray<FInventoryItem>& outItems) const
{
    outItems.Empty();
    const auto& allItems = GetInventoryListConst().GetAllItems();

    for (const auto& item : allItems) {
        if (item.ItemClass == ItemClass) {
            outItems.Add(item);
        }
    }
}

void UACFInventoryComponent::GetAllSellableItemsInInventory(TArray<FInventoryItem>& outItems) const
{
    outItems.Empty();
    const auto& allItems = GetInventoryListConst().GetAllItems();
    for (const auto& item : allItems) {
        FItemDescriptor itemData;
        UACFItemSystemFunctionLibrary::GetItemData(item.ItemClass, itemData);
        if (itemData.bSellable) {
            outItems.Add(item);
        }
    }
}

bool UACFInventoryComponent::FindFirstItemOfClassInInventory(const TSubclassOf<UACFItem>& ItemClass, FInventoryItem& outItem) const
{
    TArray<FInventoryItem> outItems;
    GetAllItemsOfClassInInventory(ItemClass, outItems);
    if (outItems.Num() > 0) {
        outItem = outItems[0];
        return true;
    }

    return false;
}

void UACFInventoryComponent::UseConsumableOnTarget(const FInventoryItem& Inventoryitem, APawn* target)
{
    UACFConsumable* consumable = NewObject<UACFConsumable>(actorOwner, Inventoryitem.ItemClass);

    if (consumable) {
        consumable->SetItemOwner(actorOwner);
        if (consumable->CanBeUsed(target)) {
            Internal_UseItem(consumable, target, Inventoryitem);
        }
    }
}

bool UACFInventoryComponent::CanUseConsumable(const FInventoryItem& Inventoryitem, APawn* target) const
{
    UACFConsumable* consumable = NewObject<UACFConsumable>(actorOwner, Inventoryitem.ItemClass);
    return consumable && consumable->CanBeUsed(target);
}

void UACFInventoryComponent::Internal_UseItem(UACFConsumable* consumable, APawn* target, const FInventoryItem& Inventoryitem)
{
    if (consumable && consumable->CanBeUsed(target)) {

        consumable->Internal_UseItem(target);
        if (consumable->GetConsumeOnUse()) {
            RemoveItem(Inventoryitem, 1);
        }
    } else {
        UE_LOG(ACFInventoryLog, Error, TEXT("Invalid Consumable!!! - UACFEquipmentComponent::UseConsumableOnTarget"));
    }
}
