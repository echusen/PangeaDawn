// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved. 

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Items/ACFItem.h"
#include <Engine/DataAsset.h>
#include "ACFCraftRecipeDataAsset.generated.h"

/**
 * 
 */
USTRUCT(BlueprintType)
struct FACFCraftingRecipe : public FTableRowBase {
    GENERATED_BODY()

public:
    FACFCraftingRecipe() {};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ACF)
    TArray<FBaseItem> RequiredItems;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ACF)
    FBaseItem OutputItem;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ACF)
    float CraftingCost = 0.f;

    FORCEINLINE bool operator!=(const FBaseItem& Other) const
    {
        return this->OutputItem.ItemClass != Other.ItemClass;
    }

    FORCEINLINE bool operator==(const FBaseItem& Other) const
    {
        return this->OutputItem.ItemClass == Other.ItemClass;
    }

    FORCEINLINE bool operator!=(const TSubclassOf<class UACFItem>& Other) const
    {
        return this->OutputItem.ItemClass != Other;
    }

    FORCEINLINE bool operator==(const TSubclassOf<class UACFItem>& Other) const
    {
        return this->OutputItem.ItemClass == Other;
    }

    FORCEINLINE bool operator!=(const FACFCraftingRecipe& Other) const
    {
        return this->OutputItem.ItemClass != Other.OutputItem.ItemClass;
    }

    FORCEINLINE bool operator==(const FACFCraftingRecipe& Other) const
    {
        return this->OutputItem.ItemClass == Other.OutputItem.ItemClass;
    }
};

UCLASS()
class CRAFTINGSYSTEM_API UACFCraftRecipeDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public: 

	UFUNCTION(BlueprintCallable, Category = ACF)
	void SetCraftingRecipe(const FACFCraftingRecipe& inRecipe) {
            RecipeConfig = inRecipe;
	}

	UFUNCTION(BlueprintPure, Category = ACF)
	FACFCraftingRecipe GetCraftingRecipe() const {
		return RecipeConfig;
	}

protected: 

	UPROPERTY(EditAnywhere, Category = ACF)
	FACFCraftingRecipe RecipeConfig;
	
};
