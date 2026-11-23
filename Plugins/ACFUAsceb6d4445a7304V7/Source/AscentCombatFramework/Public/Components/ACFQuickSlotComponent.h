// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "Components/ACFInventoryComponent.h"
#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Items/ACFItem.h"

#include "ACFQuickSlotComponent.generated.h"

class UACFEquipmentComponent;

/*A single quick slot. Plain data on purpose: the save archive only serializes
SaveGame properties, so no UObject is allowed to carry quick slot state*/
USTRUCT(BlueprintType)
struct ASCENTCOMBATFRAMEWORK_API FACFQuickSlotData {
    GENERATED_BODY()

public:
    FACFQuickSlotData() {};

    FACFQuickSlotData(const FGameplayTag& inSlotTag)
        : SlotTag(inSlotTag) {};

    /*Slot this quick slot stands for, from the same Itemslot family used by the equipment*/
    UPROPERTY(SaveGame, BlueprintReadOnly, meta = (Categories = "Itemslot"), Category = ACF)
    FGameplayTag SlotTag;

    /*The exact inventory stack assigned to this slot, invalid when the slot is empty*/
    UPROPERTY(SaveGame, BlueprintReadOnly, Category = ACF)
    FGuid ItemGuid;

    /*Class of the assigned item, used to rebind the slot when the saved stack is gone*/
    UPROPERTY(SaveGame, BlueprintReadOnly, Category = ACF)
    TSubclassOf<UACFItem> ItemClass;

    bool IsEmpty() const
    {
        /* Compared through Get(): the TSubclassOf cast operator would need the complete type */
        return !ItemGuid.IsValid() && ItemClass.Get() == nullptr;
    }

    void Empty()
    {
        ItemGuid.Invalidate();
        ItemClass = nullptr;
    }

    FORCEINLINE bool operator==(const FACFQuickSlotData& Other) const
    {
        return this->SlotTag == Other.SlotTag;
    }

    FORCEINLINE bool operator!=(const FACFQuickSlotData& Other) const
    {
        return this->SlotTag != Other.SlotTag;
    }

    FORCEINLINE bool operator==(const FGameplayTag& Other) const
    {
        return this->SlotTag == Other;
    }

    FORCEINLINE bool operator!=(const FGameplayTag& Other) const
    {
        return this->SlotTag != Other;
    }

    FORCEINLINE bool operator==(const FGuid& Other) const
    {
        return this->ItemGuid == Other;
    }

    FORCEINLINE bool operator!=(const FGuid& Other) const
    {
        return this->ItemGuid != Other;
    }
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnQuickSlotsChanged, const TArray<FACFQuickSlotData>&, QuickSlots);

/**
 * Keeps the player's quick slot assignments, meant to live on the ACFPlayerController.
 * Assignments are persistent player state: they are server authoritative, replicated to
 * the owner and stored in the save game, while using an item goes through the inventory
 * and equipment API of the possessed pawn.
 */
UCLASS(ClassGroup = (ACF), Blueprintable, meta = (BlueprintSpawnableComponent))
class ASCENTCOMBATFRAMEWORK_API UACFQuickSlotComponent : public UActorComponent {
    GENERATED_BODY()

public:
    // Sets default values for this component's properties
    UACFQuickSlotComponent();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    /*Assigns the inventory stack with the provided guid to the provided quick slot.
    The stack is removed from any other quick slot holding it*/
    UFUNCTION(Server, Reliable, BlueprintCallable, Category = ACF)
    void AssignItemToQuickSlot(FGameplayTag SlotTag, FGuid ItemGuid);

    /*Empties the provided quick slot, the slot itself stays available*/
    UFUNCTION(Server, Reliable, BlueprintCallable, Category = ACF)
    void ClearQuickSlot(FGameplayTag SlotTag);

    /*Uses the item assigned to the provided quick slot on the possessed pawn.
    Consumables are consumed, anything else is moved to the equipment*/
    UFUNCTION(Server, Reliable, BlueprintCallable, Category = ACF)
    void UseQuickSlot(FGameplayTag SlotTag);

    /*Returns all the quick slots, assigned or not*/
    UFUNCTION(BlueprintPure, Category = ACF)
    TArray<FACFQuickSlotData> GetQuickSlots() const
    {
        return QuickSlots;
    }

    /*Returns the quick slot with the provided tag*/
    UFUNCTION(BlueprintCallable, Category = ACF)
    bool GetItemInQuickSlot(FGameplayTag SlotTag, FACFQuickSlotData& OutSlot) const;

    /*Returns the first quick slot with nothing assigned to it*/
    UFUNCTION(BlueprintCallable, Category = ACF)
    bool GetFirstFreeQuickSlot(FGameplayTag& OutSlotTag) const;

    /*Returns true if the provided inventory stack is already in one of the quick slots*/
    UFUNCTION(BlueprintPure, Category = ACF)
    bool IsItemInAnyQuickSlot(FGuid ItemGuid) const;

    /*Returns the inventory item currently resolved by the provided quick slot*/
    UFUNCTION(BlueprintCallable, Category = ACF)
    bool GetInventoryItemInQuickSlot(FGameplayTag SlotTag, FInventoryItem& OutItem) const;

    /*Returns every stack in the inventory of the possessed pawn whose item slots fall
    under the provided tag, like the consumables when the tag of the consumable slots
    is provided. An invalid tag returns the whole inventory*/
    UFUNCTION(BlueprintPure, Category = ACF)
    TArray<FInventoryItem> GetInventoryItemsByTag(FGameplayTag ItemTypeTag) const;

    /*Returns every stack whose item slots fall under any of the provided tags, so one
    selection can gather families that live on different slots. An empty list returns
    the whole inventory*/
    UFUNCTION(BlueprintPure, Category = ACF)
    TArray<FInventoryItem> GetInventoryItemsByTags(const TArray<FGameplayTag>& ItemSlotTags) const;

    /*Returns every weapon in the inventory whose weapon type falls under any of the
    provided tags, for a selection built on what the weapon is rather than on the slot
    it goes in. An empty list returns every weapon*/
    UFUNCTION(BlueprintPure, Category = ACF)
    TArray<FInventoryItem> GetInventoryItemsByWeaponTypes(const TArray<FGameplayTag>& WeaponTypes) const;

    /*Returns the quick slot component of the local player, so a widget can reach the quick
    slots in a single node instead of casting the player controller by hand*/
    UFUNCTION(BlueprintPure, meta = (WorldContext = "WorldContextObject"), Category = ACF)
    static UACFQuickSlotComponent* GetLocalQuickSlotComponent(const UObject* WorldContextObject);

    /*Returns the equipment of the pawn currently possessed by the owner of this component*/
    UFUNCTION(BlueprintPure, Category = ACF)
    UACFEquipmentComponent* GetPossessedEquipment() const;

    /*The quick slot the selection assigns to. When it is not set, the first of the
    default slots takes the role, so the selection works without any configuration*/
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, meta = (Categories = "Itemslot"), Category = ACF)
    FGameplayTag MainQuickSlot;

    /*Triggered on every assignment, clear, validation and load*/
    UPROPERTY(BlueprintAssignable, Category = ACF)
    FOnQuickSlotsChanged OnQuickSlotsChanged;

protected:
    /*Registration happens before any BeginPlay: the save component of the owner can
    already restore its actor there, and by then this component must be in its list*/
    virtual void OnRegister() override;

    // Called when the game starts
    virtual void BeginPlay() override;

    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    /*Quick slots created when nothing has been assigned to them yet. Only the slots
    missing from the current state are added, so loaded assignments are never touched*/
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (Categories = "Itemslot"), Category = ACF)
    TArray<FGameplayTag> DefaultSlots;

    /*Called by the save system once this component has been deserialized*/
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = ACF)
    void OnComponentLoaded();

    UFUNCTION()
    void OnRep_QuickSlots();

    UFUNCTION()
    void HandlePossessedPawnChanged(const APawn* pawn);

    UFUNCTION()
    void HandleItemAdded(const FBaseItem& item);

    UFUNCTION()
    void HandleItemRemoved(const FBaseItem& item);

private:
    /*Quick slot assignments of this player*/
    UPROPERTY(SaveGame, ReplicatedUsing = OnRep_QuickSlots)
    TArray<FACFQuickSlotData> QuickSlots;

    APawn* GetPossessedPawn() const;

    /*The configured main slot, or the first configured one when it is not set*/
    FGameplayTag GetMainQuickSlotTag() const;

    /*True when the tag belongs to the item slot family the quick slots are built on*/
    bool IsQuickSlotTag(const FGameplayTag& inSlotTag) const;

    /*Adds the configured slots that are still missing, never overwrites an assignment*/
    void AddMissingDefaultSlots();

    /*Rebinds every slot against the real inventory: the exact stack first, then any
    other stack of the same class, and clears the slot when neither exists anymore*/
    void ValidateQuickSlots();

    bool ResolveSlot(FACFQuickSlotData& inOutSlot, FInventoryItem& outItem) const;

    void BindToInventory();

    void UnbindFromInventory();

    void NotifyQuickSlotsChanged();

    /*Makes sure the save system collects this component together with its owner*/
    void RegisterToSaveComponent();

    TWeakObjectPtr<UACFInventoryComponent> boundInventory;
};
