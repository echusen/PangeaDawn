// ===== ACFCraftingComponent.h =====
#pragma once

#include "ACFVendorComponent.h"
#include "Components/ACFEquipmentComponent.h"
#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "Items/ACFItem.h"
#include "UObject/SoftObjectPath.h"
#include "ACFCraftingComponent.generated.h"

class UACFCraftRecipeDataAsset;

UCLASS(Blueprintable, ClassGroup = (ACF), meta = (BlueprintSpawnableComponent))
class CRAFTINGSYSTEM_API UACFCraftingComponent : public UACFVendorComponent
{
    GENERATED_BODY()

public:
    UACFCraftingComponent();

    /*------------------- RECIPE MANAGEMENT -----------------------------------*/
    
    /** Teaches a recipe to this station. Server only, replicated to clients. */
    UFUNCTION(BlueprintCallable, Category = "ACF | Crafting")
    void AddRecipe(UACFCraftRecipeDataAsset* Recipe);

    /** Removes a recipe from this station, including one authored in ItemsRecipes. Server only. */
    UFUNCTION(BlueprintCallable, Category = "ACF | Crafting")
    bool RemoveRecipe(UACFCraftRecipeDataAsset* Recipe);

    UFUNCTION(BlueprintPure, Category = "ACF | Crafting")
    bool HasRecipe(UACFCraftRecipeDataAsset* Recipe) const;

    UFUNCTION(BlueprintCallable, Category = "ACF | Crafting")
    void AddRecipes(const TArray<UACFCraftRecipeDataAsset*>& RecipesToAdd);

    UFUNCTION(BlueprintCallable, Category = "ACF | Crafting")
    void RemoveRecipes(const TArray<UACFCraftRecipeDataAsset*>& RecipesToRemove);

    /** Empties the whole recipe list of this station. Server only. */
    UFUNCTION(BlueprintCallable, Category = "ACF | Crafting")
    void ClearRuntimeRecipes();

    UFUNCTION(BlueprintPure, Category = "ACF | Crafting")
    TArray<UACFCraftRecipeDataAsset*> GetAllRecipeDataAssets() const;

    /*------------------- CHECKS -----------------------------------*/
    
    UFUNCTION(BlueprintCallable, Category = "ACF | Checks")
    bool CanPawnUpgradeItem(const FInventoryItem& itemToUpgrade, const APawn* pawnOwner) const;

    UFUNCTION(BlueprintCallable, Category = "ACF | Checks")
    bool CanPawnCraftItem(const FACFCraftingRecipe& itemToCraft, const APawn* buyer) const;

    /*------------------- SERVER SIDE -----------------------------------*/
    
    UFUNCTION(BlueprintCallable, Category = "ACF | Crafting")
    void CraftItem(const FACFCraftingRecipe& ItemToCraft, APawn* instigator);

    UFUNCTION(BlueprintCallable, Category = "ACF | Crafting")
    void UpgradeItem(const FInventoryItem& itemToUpgrade, APawn* instigator);

    /*-------------------PLAYER STUFF -----------------------------------*/
    
    UFUNCTION(BlueprintPure, Category = "ACF | Getters")
    TArray<FInventoryItem> GetAllPawnUpgradableItems(const APawn* pawn) const;

    UFUNCTION(BlueprintPure, Category = "ACF | Getters")
    TArray<FACFCraftingRecipe> GetCraftableRecipes() const
    {
        return CraftableItems;
    }

    UFUNCTION(BlueprintCallable, Category = "ACF | Getters")
    bool TryGetCraftableRecipeForItem(const FBaseItem& recipe, FACFCraftingRecipe& outRecipe) const;

    // DEPRECATED - Kept for backwards compatibility
    UFUNCTION(BlueprintCallable, Category = "ACF | Setters", meta = (DeprecatedFunction, DeprecationMessage = "Use AddRecipe(DataAsset) instead"))
    void AddNewRecipe(const FACFCraftingRecipe& recipe)
    {
        CraftableItems.Add(recipe);
    }

    UFUNCTION(BlueprintCallable, Category = "ACF | Setters", meta = (DeprecatedFunction, DeprecationMessage = "Use RemoveRecipe(DataAsset) instead"))
    void RemoveOldRecipe(const FACFCraftingRecipe& recipe)
    {
        CraftableItems.Remove(recipe);
    }

    /*-------------------BLUEPRINT EVENTS -----------------------------------*/
    
    UFUNCTION(BlueprintImplementableEvent, Category = "ACF | Crafting")
    void OnRecipeAdded(UACFCraftRecipeDataAsset* Recipe);

    UFUNCTION(BlueprintImplementableEvent, Category = "ACF | Crafting")
    void OnRecipeRemoved(UACFCraftRecipeDataAsset* Recipe);

    UFUNCTION(BlueprintImplementableEvent, Category = "ACF | Crafting")
    void OnRecipesChanged();

protected:
    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    /** Converts the recipe list into paths, the only form the save archive can restore reliably. */
    virtual void OnComponentSaved_Implementation() override;

    /** Rebuilds the recipe list from the saved paths. */
    virtual void OnComponentLoaded_Implementation() override;

    /**
     * The recipes this station knows: authored in the editor, then owned by the server, which is the
     * only side allowed to add or remove at runtime.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_ItemsRecipes, Category = ACF)
    TArray<TObjectPtr<UACFCraftRecipeDataAsset>> ItemsRecipes;

    /**
     * Serialized form of ItemsRecipes, written on save and consumed on load. Hard asset references
     * cannot be persisted: the save archive resolves them with FindObject only, so a recipe whose
     * asset is not already in memory at load time would come back null.
     */
    UPROPERTY(SaveGame)
    TArray<FSoftObjectPath> SavedRecipePaths;

    /** Tells an empty saved list apart from a save that never contained this component. */
    UPROPERTY(SaveGame)
    bool bRecipesSaved = false;

    UFUNCTION()
    void OnRep_ItemsRecipes();

private:
    /** Craftable form of ItemsRecipes, what the crafting UI reads. */
    UPROPERTY(Transient)
    TArray<FACFCraftingRecipe> CraftableItems;

    void RebuildCraftableItemsCache();

    bool IsAuthorityOrWarn(const TCHAR* CallerName) const;
};
