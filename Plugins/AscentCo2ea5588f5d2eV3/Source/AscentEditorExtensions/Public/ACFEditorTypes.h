// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"

#include "ACFEditorTypes.generated.h"

/**
 *
 */
 USTRUCT(BlueprintType)
struct FAssetIconData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, config, Category = "Custom Asset Icons")
    TSoftObjectPtr<UTexture2D> Icon;

    UPROPERTY(EditAnywhere, config, Category = "Custom Asset Icons")
    FLinearColor IconTint = FLinearColor(0.85f, 0.85f, 0.85f, .85f);


    UPROPERTY(EditAnywhere, config, Category = "Custom Asset Icons")
    FVector2D IconScale = FVector2D(.7f, .7f);
};

USTRUCT(BlueprintType)
struct FAssetActionConfig {
    GENERATED_BODY()

    // The class you want this Asset Action to be responsible for.
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Class Settings")
    TSoftClassPtr<UObject> AssetClass;

    /*Replaces the default class name if not empty*/
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Class Settings")
    FText ClassNameOverride;

    // The color of the asset in the editor menus only. Once the asset is created it will default back to its original color.
    // By default it's the default Blueprint color.
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Cosmetic")
    FColor AssetColor = FColor(63, 126, 255);
};

USTRUCT(BlueprintType)
struct FAssetActionSettings {
    GENERATED_BODY()

    FAssetActionSettings()
    {
        CategoryName = "ACF Gameplay";
        ClassNameOverride = FText::FromString("");
    }

    FAssetActionSettings(const FAssetActionConfig& actionConfig, const FName& inCategoryName, 
        const bool bInSubMenu, const FText& inSubCategoryName)
    {
        AssetClass = actionConfig.AssetClass.Get();
        ClassNameOverride = actionConfig.ClassNameOverride;
        bEnabled = true;
        CategoryName = inCategoryName;
        UseSubMenu = bInSubMenu;
        SubCategoryName = inSubCategoryName;
        AssetColor = actionConfig.AssetColor;
    }

    // If this Asset Action is enabled and show up in the set category.
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Asset Action")
    bool bEnabled = true;

    // The class you want this Asset Action to be responsible for.
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Class Settings")
    TSubclassOf<UObject> AssetClass;

    /*Replaces the default class name if not empty*/
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Class Settings")
    FText ClassNameOverride;

    // The name of the category this Asset Action should be in.
    // Example: Weapons
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Asset Category")
    FName CategoryName;

    // Bool to check if sub menu should be considered at all in the drop down menu.
    UPROPERTY()
    bool UseSubMenu = false;

    // The name of the Sub Category this Asset Action will exists in within the category. If left empty, it will count as no SubCategory.
    // NOTE: A sub category can only be created if there are more than one item in the main category.
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Asset Category", meta = (EditCondition = UseSubMenu))
    FText SubCategoryName;

    // The color of the asset in the editor menus only. Once the asset is created it will default back to its original color.
    // By default it's the default Blueprint color.
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Cosmetic")
    FColor AssetColor = FColor(63, 126, 255);

    bool IsValid()
    {
        return bEnabled && !CategoryName.IsNone() && AssetClass;
    }
};

USTRUCT(BlueprintType)
struct FCategoryConfig {
    GENERATED_BODY()

    // The name of the category this Asset Action should be in.
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Asset Category")
    FName CategoryName = "Enemies";

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Asset Category")
    TArray<FAssetActionConfig> Entries;
};

USTRUCT(BlueprintType)
struct FPlaceCategoryConfig : public FCategoryConfig {
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Asset Category")
    FName IconName = "ACFEditor.SmallIcon";
};

USTRUCT(BlueprintType)
struct FMainCategoryConfig {
    GENERATED_BODY()

    // The name of the category this Asset Action should be in.
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Asset Category")
    FName MainCategoryName = "Ascent Combat Framework";

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Asset Category")
    TArray<FAssetActionConfig> Entries;

    UPROPERTY(BlueprintReadWrite, meta = (TitleProperty = "CategoryName"), EditAnywhere, Category = "Asset Category")
    TArray<FCategoryConfig> SubCategories;
};

UCLASS()
class ASCENTEDITOREXTENSIONS_API UACFEditorTypes : public UObject {
    GENERATED_BODY()
};
