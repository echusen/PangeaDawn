// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "Components/ACFQuickSlotComponent.h"
#include "ACFItemSystemFunctionLibrary.h"
#include "ALSLoadAndSaveComponent.h"
#include "ALSSavableInterface.h"
#include "Components/ACFEquipmentComponent.h"
#include "Items/ACFWeapon.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Game/ACFPlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include <GameFramework/Pawn.h>
#include <GameFramework/PlayerController.h>

// Sets default values for this component's properties
UACFQuickSlotComponent::UACFQuickSlotComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
}

void UACFQuickSlotComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    /* The owner is the PlayerController, which is relevant to its own connection only:
     * the assignments never leave the player they belong to */
    DOREPLIFETIME(UACFQuickSlotComponent, QuickSlots);
}

void UACFQuickSlotComponent::OnRegister()
{
    Super::OnRegister();

    const UWorld* world = GetWorld();
    if (world && world->IsGameWorld()) {
        RegisterToSaveComponent();
    }
}

// Called when the game starts
void UACFQuickSlotComponent::BeginPlay()
{
    Super::BeginPlay();

    if (AACFPlayerController* playerCont = Cast<AACFPlayerController>(GetOwner())) {
        if (!playerCont->OnPossessedCharacterChanged.IsAlreadyBound(this, &UACFQuickSlotComponent::HandlePossessedPawnChanged)) {
            playerCont->OnPossessedCharacterChanged.AddDynamic(this, &UACFQuickSlotComponent::HandlePossessedPawnChanged);
        }
    }

    if (GetOwner()->HasAuthority()) {
        AddMissingDefaultSlots();
    }

    BindToInventory();
    NotifyQuickSlotsChanged();
}

void UACFQuickSlotComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    UnbindFromInventory();

    if (AACFPlayerController* playerCont = Cast<AACFPlayerController>(GetOwner())) {
        playerCont->OnPossessedCharacterChanged.RemoveDynamic(this, &UACFQuickSlotComponent::HandlePossessedPawnChanged);
    }

    Super::EndPlay(EndPlayReason);
}

void UACFQuickSlotComponent::AssignItemToQuickSlot_Implementation(FGameplayTag SlotTag, FGuid ItemGuid)
{
    if (!IsQuickSlotTag(SlotTag)) {
        UE_LOG(LogTemp, Warning, TEXT("Invalid quick slot tag %s, expected an item slot tag - UACFQuickSlotComponent::AssignItemToQuickSlot"), *SlotTag.ToString());
        return;
    }

    UACFEquipmentComponent* equipComp = GetPossessedEquipment();
    if (!equipComp) {
        UE_LOG(LogTemp, Warning, TEXT("No equipment on the possessed pawn - UACFQuickSlotComponent::AssignItemToQuickSlot"));
        return;
    }

    FInventoryItem invItem;
    if (!equipComp->GetItemByGuid(ItemGuid, invItem)) {
        UE_LOG(LogTemp, Warning, TEXT("The provided item is not in the inventory - UACFQuickSlotComponent::AssignItemToQuickSlot"));
        return;
    }

    /* The same stack in two quick slots would make the second one unusable as soon as the first consumes it */
    for (FACFQuickSlotData& quickSlot : QuickSlots) {
        if (quickSlot.SlotTag != SlotTag && quickSlot.ItemGuid == ItemGuid) {
            quickSlot.Empty();
        }
    }

    FACFQuickSlotData* slotPtr = QuickSlots.FindByKey(SlotTag);
    if (!slotPtr) {
        slotPtr = &QuickSlots[QuickSlots.Add(FACFQuickSlotData(SlotTag))];
    }

    slotPtr->ItemGuid = ItemGuid;
    slotPtr->ItemClass = invItem.ItemClass;

    NotifyQuickSlotsChanged();
}

void UACFQuickSlotComponent::ClearQuickSlot_Implementation(FGameplayTag SlotTag)
{
    FACFQuickSlotData* slotPtr = QuickSlots.FindByKey(SlotTag);
    if (!slotPtr || slotPtr->IsEmpty()) {
        return;
    }

    slotPtr->Empty();
    NotifyQuickSlotsChanged();
}

void UACFQuickSlotComponent::UseQuickSlot_Implementation(FGameplayTag SlotTag)
{
    FACFQuickSlotData* slotPtr = QuickSlots.FindByKey(SlotTag);
    if (!slotPtr || slotPtr->IsEmpty()) {
        UE_LOG(LogTemp, Log, TEXT("Nothing assigned to quick slot %s - UACFQuickSlotComponent::UseQuickSlot"), *SlotTag.ToString());
        return;
    }

    APawn* possessedPawn = GetPossessedPawn();
    UACFEquipmentComponent* equipComp = GetPossessedEquipment();
    if (!possessedPawn || !equipComp) {
        UE_LOG(LogTemp, Warning, TEXT("No equipment on the possessed pawn - UACFQuickSlotComponent::UseQuickSlot"));
        return;
    }

    /* Resolved again here: the assigned stack may have been spent, sold or dropped since the slot was filled */
    FInventoryItem invItem;
    if (!ResolveSlot(*slotPtr, invItem)) {
        UE_LOG(LogTemp, Log, TEXT("The item of quick slot %s is no longer in the inventory - UACFQuickSlotComponent::UseQuickSlot"), *SlotTag.ToString());
        NotifyQuickSlotsChanged();
        return;
    }

    if (equipComp->CanUseConsumable(invItem, possessedPawn)) {
        equipComp->UseConsumableOnTarget(invItem, possessedPawn);
    } else {
        equipComp->UseInventoryItem(invItem);
    }

    NotifyQuickSlotsChanged();
}

bool UACFQuickSlotComponent::GetItemInQuickSlot(FGameplayTag SlotTag, FACFQuickSlotData& OutSlot) const
{
    const FACFQuickSlotData* slotPtr = QuickSlots.FindByKey(SlotTag);
    if (slotPtr) {
        OutSlot = *slotPtr;
        return true;
    }
    return false;
}

bool UACFQuickSlotComponent::GetFirstFreeQuickSlot(FGameplayTag& OutSlotTag) const
{
    for (const FACFQuickSlotData& quickSlot : QuickSlots) {
        if (quickSlot.IsEmpty()) {
            OutSlotTag = quickSlot.SlotTag;
            return true;
        }
    }
    return false;
}

bool UACFQuickSlotComponent::IsItemInAnyQuickSlot(FGuid ItemGuid) const
{
    return ItemGuid.IsValid() && QuickSlots.Contains(ItemGuid);
}

bool UACFQuickSlotComponent::GetInventoryItemInQuickSlot(FGameplayTag SlotTag, FInventoryItem& OutItem) const
{
    FACFQuickSlotData quickSlot;
    if (!GetItemInQuickSlot(SlotTag, quickSlot)) {
        return false;
    }
    return ResolveSlot(quickSlot, OutItem);
}

TArray<FInventoryItem> UACFQuickSlotComponent::GetInventoryItemsByTag(FGameplayTag ItemTypeTag) const
{
    TArray<FInventoryItem> outItems;

    const UACFEquipmentComponent* equipComp = GetPossessedEquipment();
    if (!equipComp) {
        return outItems;
    }

    for (const FInventoryItem& invItem : equipComp->GetInventory()) {
        /* A spent stack is still in the list until it is removed: offering it would show an item that cannot be used */
        if (invItem.Count <= 0 || invItem.ItemClass.Get() == nullptr) {
            continue;
        }

        if (!ItemTypeTag.IsValid()) {
            outItems.AddUnique(invItem);
            continue;
        }

        FItemDescriptor itemInfo;
        if (!UACFItemSystemFunctionLibrary::GetItemData(invItem.ItemClass, itemInfo)) {
            continue;
        }

        for (const FGameplayTag& itemSlot : itemInfo.ItemSlots) {
            /* MatchesTag and not MatchesTagExact: the item slots are hierarchical, so
             * the tag of a family selects every item belonging to that family */
            if (itemSlot.MatchesTag(ItemTypeTag)) {
                outItems.AddUnique(invItem);
                break;
            }
        }
    }

    return outItems;
}

TArray<FInventoryItem> UACFQuickSlotComponent::GetInventoryItemsByTags(const TArray<FGameplayTag>& ItemSlotTags) const
{
    /* No tag means no filter, exactly like the single tag version: the caller asked for everything */
    if (ItemSlotTags.IsEmpty()) {
        return GetInventoryItemsByTag(FGameplayTag());
    }

    TArray<FInventoryItem> outItems;

    /* The union of the single tag query, so one item listed by two tags is still offered once.
     * An invalid tag is skipped and not forwarded: the single tag version reads it as "no filter"
     * and would hand back the whole inventory, turning one bad entry into a list of everything */
    for (const FGameplayTag& slotTag : ItemSlotTags) {
        if (!slotTag.IsValid()) {
            UE_LOG(LogTemp, Warning, TEXT("Invalid item slot tag in the filter list, ignored - UACFQuickSlotComponent::GetInventoryItemsByTags"));
            continue;
        }

        for (const FInventoryItem& invItem : GetInventoryItemsByTag(slotTag)) {
            outItems.AddUnique(invItem);
        }
    }

    return outItems;
}

TArray<FInventoryItem> UACFQuickSlotComponent::GetInventoryItemsByWeaponTypes(const TArray<FGameplayTag>& WeaponTypes) const
{
    TArray<FInventoryItem> outItems;

    const UACFEquipmentComponent* equipComp = GetPossessedEquipment();
    if (!equipComp) {
        return outItems;
    }

    for (const FInventoryItem& invItem : equipComp->GetInventory()) {
        /* A spent stack is still in the list until it is removed: offering it would show an item that cannot be used */
        if (invItem.Count <= 0 || invItem.ItemClass.Get() == nullptr) {
            continue;
        }

        /* Anything that is not a weapon has no weapon type to match against */
        const UACFWeapon* weapon = Cast<UACFWeapon>(invItem.ItemClass->GetDefaultObject());
        if (!weapon) {
            continue;
        }

        if (WeaponTypes.IsEmpty()) {
            outItems.AddUnique(invItem);
            continue;
        }

        for (const FGameplayTag& weaponType : WeaponTypes) {
            /* MatchesTag and not MatchesTagExact: a family tag selects every weapon type below it */
            if (weapon->GetWeaponType().MatchesTag(weaponType)) {
                outItems.AddUnique(invItem);
                break;
            }
        }
    }

    return outItems;
}

UACFQuickSlotComponent* UACFQuickSlotComponent::GetLocalQuickSlotComponent(const UObject* WorldContextObject)
{
    const APlayerController* playerCont = UGameplayStatics::GetPlayerController(WorldContextObject, 0);
    if (!playerCont) {
        return nullptr;
    }

    if (const AACFPlayerController* acfPlayerCont = Cast<AACFPlayerController>(playerCont)) {
        if (UACFQuickSlotComponent* quickSlotComp = acfPlayerCont->GetQuickSlotComponent()) {
            return quickSlotComp;
        }
    }

    /* Searched on the controller as well: the component can be added in Blueprint, or to a
     * controller that does not derive from the ACF one */
    return playerCont->FindComponentByClass<UACFQuickSlotComponent>();
}

UACFEquipmentComponent* UACFQuickSlotComponent::GetPossessedEquipment() const
{
    const APawn* possessedPawn = GetPossessedPawn();
    if (!possessedPawn) {
        return nullptr;
    }
    return possessedPawn->FindComponentByClass<UACFEquipmentComponent>();
}

void UACFQuickSlotComponent::OnComponentLoaded_Implementation()
{
    /* The saved assignments have just replaced the runtime ones: only the slots the
     * designer added after that save are missing, and they come in empty */
    AddMissingDefaultSlots();
    ValidateQuickSlots();
    NotifyQuickSlotsChanged();
}

void UACFQuickSlotComponent::OnRep_QuickSlots()
{
    OnQuickSlotsChanged.Broadcast(QuickSlots);
}

void UACFQuickSlotComponent::HandlePossessedPawnChanged(const APawn* pawn)
{
    BindToInventory();

    if (GetOwner()->HasAuthority()) {
        ValidateQuickSlots();
    }

    NotifyQuickSlotsChanged();
}

void UACFQuickSlotComponent::HandleItemAdded(const FBaseItem& item)
{
    if (!GetOwner()->HasAuthority()) {
        return;
    }

    /* A slot that lost its stack is waiting for another one of the same class */
    bool bChanged = false;
    for (FACFQuickSlotData& quickSlot : QuickSlots) {
        if (!quickSlot.ItemGuid.IsValid() && quickSlot.ItemClass.Get() != nullptr && quickSlot.ItemClass == item.ItemClass) {
            quickSlot.ItemGuid = item.GetItemGuid();
            bChanged = true;
        }
    }

    if (bChanged) {
        NotifyQuickSlotsChanged();
    }
}

void UACFQuickSlotComponent::HandleItemRemoved(const FBaseItem& item)
{
    if (!GetOwner()->HasAuthority()) {
        return;
    }

    if (!QuickSlots.Contains(item.GetItemGuid())) {
        return;
    }

    ValidateQuickSlots();
    NotifyQuickSlotsChanged();
}

APawn* UACFQuickSlotComponent::GetPossessedPawn() const
{
    if (const APlayerController* playerCont = Cast<APlayerController>(GetOwner())) {
        return playerCont->GetPawn();
    }
    return Cast<APawn>(GetOwner());
}

FGameplayTag UACFQuickSlotComponent::GetMainQuickSlotTag() const
{
    if (MainQuickSlot.IsValid()) {
        return MainQuickSlot;
    }

    /* Nothing configured: the first slot takes the role, so the selection still assigns somewhere */
    for (const FGameplayTag& slotTag : DefaultSlots) {
        if (slotTag.IsValid()) {
            return slotTag;
        }
    }

    for (const FACFQuickSlotData& quickSlot : QuickSlots) {
        if (quickSlot.SlotTag.IsValid()) {
            return quickSlot.SlotTag;
        }
    }

    return FGameplayTag();
}

bool UACFQuickSlotComponent::IsQuickSlotTag(const FGameplayTag& inSlotTag) const
{
    return inSlotTag.MatchesTag(UACFItemSystemFunctionLibrary::GetItemSlotTagRoot());
}

void UACFQuickSlotComponent::AddMissingDefaultSlots()
{
    for (const FGameplayTag& slotTag : DefaultSlots) {
        if (slotTag.IsValid() && !QuickSlots.Contains(slotTag)) {
            QuickSlots.Add(FACFQuickSlotData(slotTag));
        }
    }
}

void UACFQuickSlotComponent::ValidateQuickSlots()
{
    /* Without an inventory to compare against, every assignment is kept: this runs while
     * the pawn is still being possessed or loaded, and clearing here would wipe the save */
    if (!GetPossessedEquipment()) {
        return;
    }

    for (FACFQuickSlotData& quickSlot : QuickSlots) {
        FInventoryItem invItem;
        if (!ResolveSlot(quickSlot, invItem)) {
            UE_LOG(LogTemp, Log, TEXT("Quick slot %s cleared, its item is not in the inventory anymore - UACFQuickSlotComponent::ValidateQuickSlots"), *quickSlot.SlotTag.ToString());
        }
    }
}

bool UACFQuickSlotComponent::ResolveSlot(FACFQuickSlotData& inOutSlot, FInventoryItem& outItem) const
{
    if (inOutSlot.IsEmpty()) {
        return false;
    }

    UACFEquipmentComponent* equipComp = GetPossessedEquipment();
    if (!equipComp) {
        return false;
    }

    /* The exact stack the player picked */
    if (inOutSlot.ItemGuid.IsValid() && equipComp->GetItemByGuid(inOutSlot.ItemGuid, outItem)) {
        return true;
    }

    /* Then any other stack of the same item, so a potion bought again lands back in its slot */
    if (inOutSlot.ItemClass.Get() != nullptr && equipComp->FindFirstItemOfClassInInventory(inOutSlot.ItemClass, outItem)) {
        inOutSlot.ItemGuid = outItem.GetItemGuid();
        return true;
    }

    inOutSlot.Empty();
    return false;
}

void UACFQuickSlotComponent::BindToInventory()
{
    UACFEquipmentComponent* equipComp = GetPossessedEquipment();
    if (boundInventory.Get() == equipComp) {
        return;
    }

    UnbindFromInventory();

    if (!equipComp) {
        return;
    }

    equipComp->OnItemAdded.AddDynamic(this, &UACFQuickSlotComponent::HandleItemAdded);
    equipComp->OnItemRemoved.AddDynamic(this, &UACFQuickSlotComponent::HandleItemRemoved);
    boundInventory = equipComp;
}

void UACFQuickSlotComponent::UnbindFromInventory()
{
    if (UACFInventoryComponent* invComp = boundInventory.Get()) {
        invComp->OnItemAdded.RemoveDynamic(this, &UACFQuickSlotComponent::HandleItemAdded);
        invComp->OnItemRemoved.RemoveDynamic(this, &UACFQuickSlotComponent::HandleItemRemoved);
    }
    boundInventory = nullptr;
}

void UACFQuickSlotComponent::NotifyQuickSlotsChanged()
{
    OnQuickSlotsChanged.Broadcast(QuickSlots);
}

void UACFQuickSlotComponent::RegisterToSaveComponent()
{
    AActor* actorOwner = GetOwner();
    if (!actorOwner) {
        return;
    }

    /* The save system collects the components to serialize from the load and save component
     * and from the savable interface of the actor, merging both: adding this class to the
     * first one works even when the owner implements the interface in Blueprint */
    if (UALSLoadAndSaveComponent* saveComp = actorOwner->FindComponentByClass<UALSLoadAndSaveComponent>()) {
        saveComp->AddComponentClassToSave(GetClass());
    } else if (!actorOwner->GetClass()->ImplementsInterface(UALSSavableInterface::StaticClass())) {
        UE_LOG(LogTemp, Warning, TEXT("%s has neither a Load And Save Component nor the Savable Interface: the quick slots will not be saved - UACFQuickSlotComponent::RegisterToSaveComponent"), *actorOwner->GetName());
    }
}
