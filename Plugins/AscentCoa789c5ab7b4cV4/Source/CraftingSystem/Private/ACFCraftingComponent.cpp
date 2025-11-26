// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.

#include "ACFCraftingComponent.h"
#include "ACFCraftRecipeDataAsset.h"
#include "ACFItemSystemFunctionLibrary.h"
#include "ACFItemsManagerComponent.h"
#include "Components/ACFCurrencyComponent.h"
#include "Components/ACFEquipmentComponent.h"
#include "Game/ACFFunctionLibrary.h"
#include "Game/ACFPlayerController.h"
#include "GameFramework/GameStateBase.h"
#include "Kismet/GameplayStatics.h"

// Sets default values for this component's properties
UACFCraftingComponent::UACFCraftingComponent()
{
    // Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
    // off to improve performance if you don't need them.
    PrimaryComponentTick.bCanEverTick = false;
}

// Called when the game starts
void UACFCraftingComponent::BeginPlay()
{
    Super::BeginPlay();

    for (UACFCraftRecipeDataAsset* receiptKey : ItemsRecipes) {
        if (receiptKey) {
            AddNewRecipe(receiptKey->GetCraftingRecipe());
        }
    }
}

/*------------------- CHECKS -----------------------------------*/

bool UACFCraftingComponent::CanPawnUpgradeItem(const FInventoryItem& itemToUpgrade, const APawn* pawnOwner) const
{
    if (!GetPawnInventory(pawnOwner).Contains(itemToUpgrade)) {
        return false;
    }

    UACFEquipmentComponent* equipComp = GetPawnEquipment(pawnOwner);
    FItemDescriptor itemData;
    UACFItemSystemFunctionLibrary::GetItemData(itemToUpgrade.ItemClass, itemData);
    if (equipComp) {
        if (itemData.bUpgradable && GetPawnCurrency(pawnOwner) >= PriceMultiplierOnSell * itemData.UpgradeCurrencyCost) {
            return equipComp->HasEnoughItemsOfType(itemData.RequiredItemsToUpgrade);
        }
    }
    return false;
}

bool UACFCraftingComponent::CanPawnCraftItem(const FACFCraftingRecipe& itemToCraft, const APawn* buyer) const
{
    UACFEquipmentComponent* equipComp = GetPawnEquipment(buyer);

    if (equipComp->NumberOfItemCanTake(itemToCraft.OutputItem.ItemClass) < itemToCraft.OutputItem.Count) {
        return false;
    }

    return equipComp->HasEnoughItemsOfType(itemToCraft.RequiredItems) && GetPawnCurrency(buyer) >= PriceMultiplierOnSell * itemToCraft.CraftingCost;
}

/* ----------- TO SERVER---------------*/

void UACFCraftingComponent::UpgradeItem(const FInventoryItem& itemToUpgrade, APawn* instigator)
{
    if (GetItemsManager()) {
        GetItemsManager()->UpgradeItem(itemToUpgrade, instigator, this);
    }
}

void UACFCraftingComponent::CraftItem(const FACFCraftingRecipe& ItemToCraft, APawn* instigator)
{
    if (GetItemsManager()) {
        GetItemsManager()->CraftItem(ItemToCraft, instigator, this);
    }
}

/*-------------------PLAYER STUFF-----------------------------------*/

TArray<FInventoryItem> UACFCraftingComponent::GetAllPawnUpgradableItems(const APawn* pawn) const
{
    TArray<FInventoryItem> upgradables;

    const TArray<FInventoryItem> playerInventory = GetPawnInventory(pawn);

    for (const auto& elem : playerInventory) {
        FItemDescriptor itemData;
        UACFItemSystemFunctionLibrary::GetItemData(elem.ItemClass, itemData);
        if (itemData.bUpgradable) {
            upgradables.Add(elem);
        }
    }
    return upgradables;
}

bool UACFCraftingComponent::TryGetCraftableRecipeForItem(const FBaseItem& recipe, FACFCraftingRecipe& outRecipe) const
{
    if (CraftableItems.Contains(recipe)) {
        outRecipe = *CraftableItems.FindByKey(recipe);
        return true;
    }
    return false;
}
