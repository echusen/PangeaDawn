// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2025. All Rights Reserved.

#pragma once

#include "ACFCurrencyComponent.h"
#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "Items/ACFItem.h"
#include <GameFramework/Character.h>
#include <GameplayTagContainer.h>

#include "ACFInventoryComponent.generated.h"

class UACFStorageComponent;
class UACFConsumable;
struct FBaseItem;

USTRUCT(BlueprintType)
struct FStartingItem : public FBaseItem {
	GENERATED_BODY()

public:
	FStartingItem() {};

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = ACF)
	bool bAutoEquip = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.0", ClampMax = "100.0"), Category = ACF)
	float DropChancePercentage = 0.f;

	FORCEINLINE bool operator==(const FStartingItem& Other) const
	{
		return this->ItemClass == Other.ItemClass;
	}

	FORCEINLINE bool operator!=(const FStartingItem& Other) const
	{
		return this->ItemClass != Other.ItemClass;
	}
};

USTRUCT(BlueprintType)
struct FInventoryItem : public FBaseItem {
	GENERATED_BODY()

public:
	FInventoryItem() {};

	FInventoryItem(const FBaseItem& inItem);

	FInventoryItem(const FStartingItem& inItem);

	/*Identifies if this item is equipped*/
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = ACF)
	bool bIsEquipped = false;

	/*If this item is equipped, this is the slot in which is equipped.
	Not set if the item is not equipped*/
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = ACF)
	FGameplayTag EquipmentSlot;

	/*Chance of this item of being dropped on death*/
	UPROPERTY(SaveGame, BlueprintReadOnly, meta = (ClampMin = "0.0", ClampMax = "100.0"), Category = ACF)
	float DropChancePercentage = 0.f;

	FORCEINLINE bool operator==(const FInventoryItem& Other) const
	{
		return this->GetItemGuid() == Other.GetItemGuid();
	}

	FORCEINLINE bool operator!=(const FInventoryItem& Other) const
	{
		return this->GetItemGuid() != Other.GetItemGuid();
	}

	FORCEINLINE bool operator==(const FGuid& Other) const
	{
		return this->GetItemGuid() == Other;
	}

	FORCEINLINE bool operator!=(const FGuid& Other) const
	{
		return this->GetItemGuid() != Other;
	}
};

/** List of inventory items */
USTRUCT(BlueprintType)
struct FACFInventoryList : public FFastArraySerializer {
	GENERATED_BODY()

	FACFInventoryList()
		: actorOwner(nullptr)
	{
	}

	TArray<FInventoryItem> GetAllItems() const;
	bool GetItemByIndex(const int32 index, FInventoryItem& outItem) const;
	bool ContainsItem(const FInventoryItem& Item) const;
	bool Contains(const FGuid& ItemGUID) const;

	void Init(AActor* owner)
	{
		actorOwner = owner;
	}
	void Empty()
	{
		Inventory.Empty();
		MarkArrayDirty();
	}

	bool IsEmpty() const
	{
		return Inventory.Num() == 0;
	}
	int32 Num() const { return Inventory.Num(); }

	bool GetItem(const FGuid& itemToSearch, FInventoryItem& outItem) const
	{
		if (Inventory.Contains(itemToSearch)) {
			outItem = *Inventory.FindByKey(itemToSearch);
			return true;
		}
		return false;
	}

	bool IsValidIndex(int32 index)
	{
		return Inventory.IsValidIndex(index);
	}

	void MarkItemAsEquipped(const FGuid& item, bool bIsEquipped, const FGameplayTag& itemSlot)
	{
		FInventoryItem* itemPtr = Inventory.FindByKey(item);
		if (itemPtr) {
			itemPtr->bIsEquipped = bIsEquipped;
			itemPtr->EquipmentSlot = itemSlot;
			MarkItemDirty(*itemPtr);
		}
	}

public:
	//~FFastArraySerializer contract
	void PreReplicatedRemove(const TArrayView<int32> RemovedIndices, int32 FinalSize) {};
	void PostReplicatedAdd(const TArrayView<int32> AddedIndices, int32 FinalSize) {
		// ValidateChanges(AddedIndices);
	};

	void ValidateChanges(const TArrayView<int32> AddedIndices);

	void PostReplicatedChange(const TArrayView<int32> ChangedIndices, int32 FinalSize) {
		// ValidateChanges(ChangedIndices);
	};
	//~End of FFastArraySerializer contract

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FInventoryItem, FACFInventoryList>(Inventory, DeltaParms, *this);
	}

	void AddEntry(FInventoryItem Instance)
	{
		if (!Inventory.Contains(Instance)) {
			//  Instance.Init(actorOwner);
			Inventory.Add(Instance);
			MarkItemDirty(Instance);
		}
	}

	void RemoveEntry(FInventoryItem Instance)
	{
		Inventory.Remove(Instance);
		MarkItemDirty(Instance);
	}

	void ChangeEntry(FInventoryItem Instance)
	{
		FInventoryItem* itemPtr = Inventory.FindByKey(Instance);
		if (itemPtr) {
			*itemPtr = Instance;
			MarkItemDirty(*itemPtr);
		}
	}

private:
	// Replicated list of items
	UPROPERTY(SaveGame)
	TArray<FInventoryItem> Inventory;

	UPROPERTY(NotReplicated)
	TObjectPtr<AActor> actorOwner;
};

template <>
struct TStructOpsTypeTraits<FACFInventoryList> : public TStructOpsTypeTraitsBase2<FACFInventoryList> {
	enum { WithNetDeltaSerializer = true };
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInventoryChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnItemAdded, const FBaseItem&, item);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnItemRemoved, const FBaseItem&, item);

/**
 * UACFInventoryComponent
 *
 * Component that manages the character's inventory, including item addition, removal, storage handling, and weight management.
 * Handles replication, usage of consumables, and interaction with currency and equipment systems.
 */
UCLASS(ClassGroup = (ACF), meta = (BlueprintSpawnableComponent))
class INVENTORYSYSTEM_API UACFInventoryComponent : public UACFCurrencyComponent {
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UACFInventoryComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:
	/**
		* Adds an item to the inventory from its class type.
		*
		* @param inItem The class of the item to add.
		* @param count Number of items to add.
		* @param bAutoEquip Whether to automatically equip the item if possible.
		*/
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "ACF|Inventory")
	void AddItemToInventoryByClass(TSubclassOf<UACFItem> inItem, int32 count = 1, bool bAutoEquip = true);

	/**
	 * Adds a base item instance to the inventory.
	 *
	 * @param ItemToAdd The base item data to add.
	 * @param bAutoEquip Whether to automatically equip the item if possible.
	 */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "ACF|Inventory")
	void AddItemToInventory(const FBaseItem& ItemToAdd, bool bAutoEquip = true);

	/**
	 * Adds an inventory item struct to the inventory.
	 *
	 * @param ItemToAdd The inventory item to add.
	 */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "ACF|Inventory")
	void AddInventoryItem(const FInventoryItem& ItemToAdd);

	/**
	* Removes the specified amount of an item from the inventory.
	* Automatically unequips it if it is equipped.
	*
	* @param item The item to remove.
	* @param count The number of items to remove.
	*/
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "ACF|Inventory")
	void RemoveItem(const FInventoryItem& item, int32 count = 1);

	/**
	 * Removes an item from the inventory by its index.
	 *
	 * @param index The index of the item in the inventory list.
	 * @param count The number of items to remove.
	 */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "ACF|Inventory")
	void RemoveItemByIndex(const int32 index, int32 count = 1);

	/* Use this only on Server!!!
	*
	*Initialize a character's inventory and equipment, receives in input the mesh to be
	* used for equipment and attachments
	*/
	UFUNCTION(BlueprintCallable, Category = ACF)
	void InitializeStartingItems();

	/*------------------------ STORAGE -----------------------------------------*/
   /**
	 * Moves a collection of items from another inventory to this one.
	 *
	 * @param inItems The items to move.
	 * @param sourceInventory The inventory component from which the items are moved.
	 */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = ACF)
	void MoveItemsFromInventory(const TArray<FInventoryItem>& inItems, UACFInventoryComponent* sourceInventory);

	/**
	 * Retrieves all items currently in the inventory.
	 *
	 * @return Array of all inventory items.
	 */
	UFUNCTION(BlueprintPure, Category = "ACF|Getters")
	FORCEINLINE TArray<FInventoryItem> GetInventory() const
	{
		return GetInventoryListConst().GetAllItems();
	}


	/**
	 * Returns the number of available slots in the inventory.
	 *
	 * @return number of available slots in the inventory.
	 */
	UFUNCTION(BlueprintPure, Category = "ACF|Getters")
	FORCEINLINE int32 GetAvailableSlots() const
	{
		return MaxInventorySlots - GetInventoryListConst().Num();
	}

	/**
	 * Checks whether an item exists in the inventory.
	 *
	 * @param item The item to check.
	 * @return True if the item exists in the inventory, false otherwise.
	 */
	UFUNCTION(BlueprintPure, Category = "ACF|Getters")
	FORCEINLINE bool IsInInventory(const FInventoryItem& item) const
	{
		return GetInventoryListConst().ContainsItem(item);
	}


	/**
	* Finds an item in the inventory by its unique GUID.
	*
	* @param itemGuid The GUID of the item to search for.
	* @param outItem The resulting item, if found.
	* @return True if the item is found, false otherwise.
	*/
	UFUNCTION(BlueprintPure, Category = "ACF|Getters")
	bool GetItemByGuid(const FGuid& itemGuid, FInventoryItem& outItem) const;

	/**
	 * Finds an item in the inventory by its index.
	 *
	 * @param index The index of the item.
	 * @param outItem The resulting item, if found.
	 * @return True if the item exists at the given index.
	 */
	UFUNCTION(BlueprintPure, Category = "ACF|Getters")
	bool GetItemByIndex(const int32 index, FInventoryItem& outItem) const
	{
		return GetInventoryListConst().GetItemByIndex(index, outItem);
	}

	/**
	 * Returns the total number of items of a specific class in the inventory.
	 *
	 * @param itemClass The class of the item.
	 * @return The total count of items matching that class.
	 */
	UFUNCTION(BlueprintCallable, Category = "ACF|Getters")
	int32 GetTotalCountOfItemsByClass(const TSubclassOf<UACFItem>& itemClass) const;

	/**
	 * Retrieves all items of a given class from the inventory.
	 *
	 * @param itemClass The class of the items to find.
	 * @param outItems The array of matching items.
	 */
	UFUNCTION(BlueprintCallable, Category = "ACF|Getters")
	void GetAllItemsOfClassInInventory(const TSubclassOf<UACFItem>& itemClass, TArray<FInventoryItem>& outItems) const;


	/**
	 * Retrieves all items marked as sellable from the inventory.
	 *
	 * @param outItems The array of sellable items.
	 */
	UFUNCTION(BlueprintPure, Category = "ACF|Getters")
	void GetAllSellableItemsInInventory(TArray<FInventoryItem>& outItems) const;

	/** Returns the list of starting items for this character. */
	UFUNCTION(BlueprintPure, Category = "ACF|Getters")
	TArray<FStartingItem> GetStartingItems() const { return StartingItems; }

	/**
	 * Finds the first item of a specified class in the inventory.
	 *
	 * @param itemClass The class of the item to search for.
	 * @param outItem The resulting item, if found.
	 * @return True if a matching item is found.
	 */
	UFUNCTION(BlueprintCallable, Category = "ACF|Getters")
	bool FindFirstItemOfClassInInventory(const TSubclassOf<UACFItem>& itemClass, FInventoryItem& outItem) const;

	/**
	 * Consumes a collection of base items from the inventory.
	 *
	 * @param ItemsToCheck The items to consume.
	 */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "ACF|Inventory")
	void ConsumeItems(const TArray<FBaseItem>& ItemsToCheck);

	/**
	 * Uses a consumable item on a target pawn.
	 *
	 * @param Inventoryitem The consumable item to use.
	 * @param target The pawn receiving the effect.
	 */
	UFUNCTION(BlueprintCallable, Category = "ACF|Inventory")
	void UseConsumableOnTarget(const FInventoryItem& Inventoryitem, APawn* target);

	/**
	 * Checks whether a consumable can be used on the given target.
	 *
	 * @param Inventoryitem The consumable item.
	 * @param target The pawn to check against.
	 * @return True if the consumable can be used.
	 */
	UFUNCTION(BlueprintCallable, Category = "ACF|Inventory")
	bool CanUseConsumable(const FInventoryItem& Inventoryitem, APawn* target) const;

	/**
	 * Checks whether the inventory has enough of each item in the given list.
	 *
	 * @param ItemsToCheck The list of items and amounts to verify.
	 * @return True if all required items are present.
	 */
	UFUNCTION(BlueprintCallable, Category = "ACF|Checks")
	bool HasEnoughItemsOfType(const TArray<FBaseItem>& ItemsToCheck) const;

	/**
		* Checks whether the inventory contains at least one instance of a given item type.
		*
		* @param itemToCheck The class of the item to check for.
		* @return True if at least one item of that class is present.
		*/
	UFUNCTION(BlueprintCallable, Category = "ACF|Checks")
	bool HasAnyItemOfType(const TSubclassOf<UACFItem>& itemToCheck) const;

	/**
	 * Gets the total current weight of all items in the inventory.
	 *
	 * @return The total weight of the inventory.
	 */
	UFUNCTION(BlueprintPure, Category = "ACF|Getters")
	FORCEINLINE float GetCurrentInventoryTotalWeight() const
	{
		return currentInventoryWeight;
	}

	/**
	 * Sets a new maximum allowed inventory weight.
	 *
	 * @param newMax The new maximum weight value.
	 */
	UFUNCTION(BlueprintCallable, Category = "ACF|Setters")
	void SetMaxInventoryWeight(int32 newMax)
	{
		MaxInventoryWeight = newMax;
	}

	/**
	 * Sets a new maximum number of inventory slots.
	 *
	 * @param newMax The new maximum slot count.
	 */
	UFUNCTION(BlueprintCallable, Category = "ACF|Setters")
	void SetMaxInventorySlots(int32 newMax)
	{
		MaxInventorySlots = newMax;
	}

	/**
	 * Returns how many more items of the specified type the inventory can hold.
	 *
	 * @param itemToCheck The item class to evaluate.
	 * @return The number of items that can still be added before reaching the limit.
	 */
	UFUNCTION(BlueprintCallable, Category = "ACF|Checks")
	int32 NumberOfItemCanTake(const TSubclassOf<UACFItem>& itemToCheck);

	/**
	 * Recalculates the total inventory weight.
	 * Should be called whenever an item is added, removed, or modified.
	 */
	UFUNCTION(BlueprintCallable, Category = ACF)
	void RefreshTotalWeight();

	/** Event triggered whenever the inventory content changes. */
	UPROPERTY(BlueprintAssignable, Category = ACF)
	FOnInventoryChanged OnInventoryChanged;

	/** Event triggered when an item is added to the inventory. */
	UPROPERTY(BlueprintAssignable, Category = ACF)
	FOnItemAdded OnItemAdded;

	/** Event triggered when an item is removed from the inventory. */
	UPROPERTY(BlueprintAssignable, Category = ACF)
	FOnItemRemoved OnItemRemoved;

	const FACFInventoryList& GetInventoryListConst() const { return InventoryList; }

protected:
	FACFInventoryList& GetInventoryList() { return InventoryList; }

	/*Maximum number of Slot items in Inventory*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Savegame, Category = "ACF|Inventory")
	int32 MaxInventorySlots = 40;

	/*Max cumulative weight on*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Savegame, Category = "ACF|Inventory")
	float MaxInventoryWeight = 180.f;

	
	/* The character's starting items*/
	UPROPERTY(EditAnywhere, meta = (TitleProperty = "ItemClass"), BlueprintReadWrite, Category = ACF)
	TArray<FStartingItem> StartingItems;

	virtual void HandleItemRemoved(const FInventoryItem& item, int32 count = 1);
	virtual void HandleItemAdded(const FInventoryItem& item, int32 count = 1, bool bTryToEquip = true, FGameplayTag equipSlot = FGameplayTag());
	virtual int32 Internal_AddItem(const FBaseItem& item, bool bTryToEquip = false, float dropChancePercentage = 0.f, FGameplayTag equipSlot = FGameplayTag());
	void Internal_UseItem(UACFConsumable* consumable, APawn* target, const FInventoryItem& Inventoryitem);
	int32 Internal_AddInventoryItem(const FInventoryItem& ItemToAdd, bool bTryToEquip = true);

	UPROPERTY(Replicated)
	float currentInventoryWeight = 0.f;

private:
	/*Inventory of this character*/
	UPROPERTY(SaveGame, Replicated, ReplicatedUsing = OnRep_Inventory)
	FACFInventoryList InventoryList;

	UFUNCTION()
	void OnRep_Inventory();

	TObjectPtr<AActor> actorOwner;
};
