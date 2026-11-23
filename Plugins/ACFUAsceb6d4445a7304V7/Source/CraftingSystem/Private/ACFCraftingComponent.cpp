// ===== ACFCraftingComponent.cpp =====

#include "ACFCraftingComponent.h"
#include "ACFCraftRecipeDataAsset.h"
#include "ACFItemSystemFunctionLibrary.h"
#include "ACFItemsManagerComponent.h"
#include "Components/ACFCurrencyComponent.h"
#include "Components/ACFEquipmentComponent.h"
#include "Game/ACFFunctionLibrary.h"
#include "Net/UnrealNetwork.h"

UACFCraftingComponent::UACFCraftingComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
}

void UACFCraftingComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(UACFCraftingComponent, ItemsRecipes);
}

void UACFCraftingComponent::BeginPlay()
{
    Super::BeginPlay();

    RebuildCraftableItemsCache();
}

void UACFCraftingComponent::OnRep_ItemsRecipes()
{
    RebuildCraftableItemsCache();
    OnRecipesChanged();
}

void UACFCraftingComponent::OnComponentSaved_Implementation()
{
    Super::OnComponentSaved_Implementation();

    SavedRecipePaths.Reset(ItemsRecipes.Num());
    for (const TObjectPtr<UACFCraftRecipeDataAsset>& Recipe : ItemsRecipes)
    {
        if (Recipe)
        {
            SavedRecipePaths.Add(FSoftObjectPath(Recipe.Get()));
        }
    }

    bRecipesSaved = true;
}

void UACFCraftingComponent::OnComponentLoaded_Implementation()
{
    Super::OnComponentLoaded_Implementation();

    if (!bRecipesSaved)
    {
        // Save made before this component was persisted: keep the recipes authored in the editor
        return;
    }

    ItemsRecipes.Reset(SavedRecipePaths.Num());
    for (const FSoftObjectPath& RecipePath : SavedRecipePaths)
    {
        if (UACFCraftRecipeDataAsset* Recipe = Cast<UACFCraftRecipeDataAsset>(RecipePath.TryLoad()))
        {
            ItemsRecipes.Add(Recipe);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("ACFCrafting: saved recipe %s could not be loaded and was discarded"),
                *RecipePath.ToString());
        }
    }

    RebuildCraftableItemsCache();
    OnRecipesChanged();
}

bool UACFCraftingComponent::IsAuthorityOrWarn(const TCHAR* CallerName) const
{
    if (GetOwnerRole() == ROLE_Authority)
    {
        return true;
    }

    UE_LOG(LogTemp, Warning, TEXT("ACFCrafting: %s on %s is server only, call it on the authority"),
        CallerName, *GetNameSafe(GetOwner()));
    return false;
}

void UACFCraftingComponent::AddRecipe(UACFCraftRecipeDataAsset* Recipe)
{
    if (!Recipe)
    {
        UE_LOG(LogTemp, Warning, TEXT("ACFCrafting: AddRecipe on %s got a null recipe"), *GetNameSafe(GetOwner()));
        return;
    }

    if (!IsAuthorityOrWarn(TEXT("AddRecipe")) || ItemsRecipes.Contains(Recipe))
    {
        return;
    }

    ItemsRecipes.Add(Recipe);
    RebuildCraftableItemsCache();

    OnRecipeAdded(Recipe);
    OnRecipesChanged();
}

bool UACFCraftingComponent::RemoveRecipe(UACFCraftRecipeDataAsset* Recipe)
{
    if (!Recipe || !IsAuthorityOrWarn(TEXT("RemoveRecipe")))
    {
        return false;
    }

    if (ItemsRecipes.Remove(Recipe) == 0)
    {
        return false;
    }

    RebuildCraftableItemsCache();

    OnRecipeRemoved(Recipe);
    OnRecipesChanged();
    return true;
}

bool UACFCraftingComponent::HasRecipe(UACFCraftRecipeDataAsset* Recipe) const
{
    return Recipe && ItemsRecipes.Contains(Recipe);
}

void UACFCraftingComponent::AddRecipes(const TArray<UACFCraftRecipeDataAsset*>& RecipesToAdd)
{
    if (!IsAuthorityOrWarn(TEXT("AddRecipes")))
    {
        return;
    }

    TArray<UACFCraftRecipeDataAsset*> AddedRecipes;
    for (UACFCraftRecipeDataAsset* Recipe : RecipesToAdd)
    {
        if (!Recipe)
        {
            UE_LOG(LogTemp, Warning, TEXT("ACFCrafting: AddRecipes on %s got a null recipe entry"), *GetNameSafe(GetOwner()));
            continue;
        }

        if (!ItemsRecipes.Contains(Recipe))
        {
            ItemsRecipes.Add(Recipe);
            AddedRecipes.Add(Recipe);
        }
    }

    if (AddedRecipes.IsEmpty())
    {
        return;
    }

    RebuildCraftableItemsCache();

    for (UACFCraftRecipeDataAsset* Recipe : AddedRecipes)
    {
        OnRecipeAdded(Recipe);
    }

    OnRecipesChanged();
}

void UACFCraftingComponent::RemoveRecipes(const TArray<UACFCraftRecipeDataAsset*>& RecipesToRemove)
{
    if (!IsAuthorityOrWarn(TEXT("RemoveRecipes")))
    {
        return;
    }

    TArray<UACFCraftRecipeDataAsset*> RemovedRecipes;
    for (UACFCraftRecipeDataAsset* Recipe : RecipesToRemove)
    {
        if (Recipe && ItemsRecipes.Remove(Recipe) > 0)
        {
            RemovedRecipes.Add(Recipe);
        }
    }

    if (RemovedRecipes.IsEmpty())
    {
        return;
    }

    RebuildCraftableItemsCache();

    for (UACFCraftRecipeDataAsset* Recipe : RemovedRecipes)
    {
        OnRecipeRemoved(Recipe);
    }

    OnRecipesChanged();
}

void UACFCraftingComponent::ClearRuntimeRecipes()
{
    if (!IsAuthorityOrWarn(TEXT("ClearRuntimeRecipes")) || ItemsRecipes.IsEmpty())
    {
        return;
    }

    const TArray<TObjectPtr<UACFCraftRecipeDataAsset>> RemovedRecipes = ItemsRecipes;
    ItemsRecipes.Empty();
    RebuildCraftableItemsCache();

    for (const TObjectPtr<UACFCraftRecipeDataAsset>& Recipe : RemovedRecipes)
    {
        OnRecipeRemoved(Recipe.Get());
    }

    OnRecipesChanged();
}

TArray<UACFCraftRecipeDataAsset*> UACFCraftingComponent::GetAllRecipeDataAssets() const
{
    TArray<UACFCraftRecipeDataAsset*> Result;
    Result.Reserve(ItemsRecipes.Num());

    for (const TObjectPtr<UACFCraftRecipeDataAsset>& Recipe : ItemsRecipes)
    {
        if (Recipe)
        {
            Result.Add(Recipe);
        }
    }

    return Result;
}

void UACFCraftingComponent::RebuildCraftableItemsCache()
{
    CraftableItems.Empty(ItemsRecipes.Num());

    for (const TObjectPtr<UACFCraftRecipeDataAsset>& Recipe : ItemsRecipes)
    {
        if (Recipe)
        {
            CraftableItems.Add(Recipe->GetCraftingRecipe());
        }
    }
}

/*------------------- CHECKS AND CRAFTING -----------------------------------*/

bool UACFCraftingComponent::CanPawnUpgradeItem(const FInventoryItem& itemToUpgrade, const APawn* pawnOwner) const
{
    if (!GetPawnInventory(pawnOwner).Contains(itemToUpgrade))
    {
        return false;
    }
    
    UACFEquipmentComponent* equipComp = GetPawnEquipment(pawnOwner);
    FItemDescriptor itemData;
    UACFItemSystemFunctionLibrary::GetItemData(itemToUpgrade.ItemClass, itemData);
    
    if (equipComp)
    {
        if (itemData.bUpgradable && GetPawnCurrency(pawnOwner) >= PriceMultiplierOnSell * itemData.UpgradeCurrencyCost)
        {
            return equipComp->HasEnoughItemsOfType(itemData.RequiredItemsToUpgrade);
        }
    }
    
    return false;
}

bool UACFCraftingComponent::CanPawnCraftItem(const FACFCraftingRecipe& itemToCraft, const APawn* buyer) const
{
    UACFEquipmentComponent* equipComp = GetPawnEquipment(buyer);
    
    if (equipComp->NumberOfItemCanTake(itemToCraft.OutputItem.ItemClass) < itemToCraft.OutputItem.Count)
    {
        return false;
    }
    
    return equipComp->HasEnoughItemsOfType(itemToCraft.RequiredItems) && 
           GetPawnCurrency(buyer) >= PriceMultiplierOnSell * itemToCraft.CraftingCost;
}

void UACFCraftingComponent::UpgradeItem(const FInventoryItem& itemToUpgrade, APawn* instigator)
{
    if (GetItemsManager())
    {
        GetItemsManager()->UpgradeItem(itemToUpgrade, instigator, this);
    }
}

void UACFCraftingComponent::CraftItem(const FACFCraftingRecipe& ItemToCraft, APawn* instigator)
{
    if (GetItemsManager())
    {
        GetItemsManager()->CraftItem(ItemToCraft, instigator, this);
    }
}

TArray<FInventoryItem> UACFCraftingComponent::GetAllPawnUpgradableItems(const APawn* pawn) const
{
    TArray<FInventoryItem> upgradables;
    const TArray<FInventoryItem> playerInventory = GetPawnInventory(pawn);
    
    for (const auto& elem : playerInventory)
    {
        FItemDescriptor itemData;
        UACFItemSystemFunctionLibrary::GetItemData(elem.ItemClass, itemData);
        
        if (itemData.bUpgradable)
        {
            upgradables.Add(elem);
        }
    }
    
    return upgradables;
}

bool UACFCraftingComponent::TryGetCraftableRecipeForItem(const FBaseItem& recipe, FACFCraftingRecipe& outRecipe) const
{
    if (CraftableItems.Contains(recipe))
    {
        outRecipe = *CraftableItems.FindByKey(recipe);
        return true;
    }
    
    return false;
}
