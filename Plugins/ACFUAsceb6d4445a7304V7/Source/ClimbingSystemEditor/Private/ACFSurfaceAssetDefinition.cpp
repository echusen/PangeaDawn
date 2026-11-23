// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "ACFSurfaceAssetDefinition.h"

#include "ACFSurfaceAsset.h"
#include "ACFSurfaceAssetEditor.h"
#include "ACFSurfaceEditorStyle.h"

EAssetCommandResult UACFSurfaceAssetDefinition::OpenAssets(const FAssetOpenArgs& OpenArgs) const
{
    for (UACFSurfaceAsset* Asset : OpenArgs.LoadObjects<UACFSurfaceAsset>())
    {
        TSharedRef<FACFSurfaceAssetEditor> Editor = MakeShared<FACFSurfaceAssetEditor>();

        Editor->InitEditor(
            OpenArgs.GetToolkitMode(),
            OpenArgs.ToolkitHost,
            Asset);
    }

    return EAssetCommandResult::Handled;
}

FText UACFSurfaceAssetDefinition::GetAssetDisplayName() const
{
    return NSLOCTEXT("ACF", "ClimbSurfaceAssetName", "ACF Climb Surface");
}

TSoftClassPtr<UObject> UACFSurfaceAssetDefinition::GetAssetClass() const
{
    return UACFSurfaceAsset::StaticClass();
}

TConstArrayView<FAssetCategoryPath> UACFSurfaceAssetDefinition::GetAssetCategories() const
{
    // Sits in the same "ACF Pawn" menu the Asset Actions register, under a "Climbing" submenu.
    // Categories are keyed by their source string, so declaring the same texts here merges this
    // asset into that menu instead of creating a separate one.
    static const FAssetCategoryPath Categories[] = {
        FAssetCategoryPath(NSLOCTEXT("ACF", "ACFPawnAssetCategory", "ACF Pawn")) / NSLOCTEXT("ACF", "ACFClimbingAssetSubCategory", "Climbing")
    };
    return Categories;
}

FLinearColor UACFSurfaceAssetDefinition::GetAssetColor() const
{
    return FLinearColor(0.0f, 0.5f, 1.0f);
}

const FSlateBrush* UACFSurfaceAssetDefinition::GetThumbnailBrush(const FAssetData& InAssetData, const FName InClassName) const
{
    return FACFSurfaceEditorStyle::Get().GetBrush("ACFSurface.AssetThumbnail");
}
