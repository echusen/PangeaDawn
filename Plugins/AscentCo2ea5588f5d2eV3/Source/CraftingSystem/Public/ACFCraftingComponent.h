// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.

#pragma once

#include "ACFVendorComponent.h"
#include "Components/ACFEquipmentComponent.h"
#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "Items/ACFItem.h"

#include "ACFCraftingComponent.generated.h"

class UACFCraftRecipeDataAsset;

UCLASS(Blueprintable, ClassGroup = (ACF), meta = (BlueprintSpawnableComponent))
class CRAFTINGSYSTEM_API UACFCraftingComponent : public UACFVendorComponent {
    GENERATED_BODY()

public:
    // Sets default values for this component's properties
    UACFCraftingComponent();

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

    /*-------------------PLAYER STUFF-----------------------------------*/

    UFUNCTION(BlueprintPure, Category = "ACF | Getters")
    TArray<FInventoryItem> GetAllPawnUpgradableItems(const APawn* pawn) const;

    UFUNCTION(BlueprintPure, Category = "ACF | Getters")
    TArray<FACFCraftingRecipe> GetCraftableRecipes() const
    {
        return CraftableItems;
    }

    UFUNCTION(BlueprintCallable, Category = "ACF | Getters")
    bool TryGetCraftableRecipeForItem(const FBaseItem& recipe, FACFCraftingRecipe& outRecipe) const;

    UFUNCTION(BlueprintCallable, Category = "ACF | Setters")
    void AddNewRecipe(const FACFCraftingRecipe& recipe)
    {
        CraftableItems.Add(recipe);
    }

    UFUNCTION(BlueprintCallable, Category = "ACF | Setters")
    void RemoveRecipe(const FACFCraftingRecipe& recipe)
    {
        CraftableItems.Remove(recipe);
    }

protected:
    // Called when the game starts
    virtual void BeginPlay() override;

    /*Items this vendor can craft*/
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ACF)
    TArray<UACFCraftRecipeDataAsset*> ItemsRecipes;

private:
    UPROPERTY()
    TArray<FACFCraftingRecipe> CraftableItems;
};
