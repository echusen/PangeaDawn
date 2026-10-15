// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetDefinitionDefault.h"
#include "ACFSurfaceAssetDefinition.generated.h"

UCLASS()
class CLIMBINGSYSTEMEDITOR_API UACFSurfaceAssetDefinition : public UAssetDefinitionDefault
{
	GENERATED_BODY()
	
public:
    virtual EAssetCommandResult OpenAssets(const FAssetOpenArgs& OpenArgs) const override;

    virtual FText GetAssetDisplayName() const override;

    virtual TSoftClassPtr<UObject> GetAssetClass() const override;

    virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override;

    virtual FLinearColor GetAssetColor() const override;
    
    virtual const FSlateBrush* GetThumbnailBrush(const FAssetData& InAssetData, const FName InClassName) const;
};
