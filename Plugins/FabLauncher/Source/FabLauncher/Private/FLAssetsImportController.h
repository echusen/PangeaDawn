// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "FLAssetImportData.h"

class FAssetsImportController
{
private:
	FAssetsImportController() = default;
	static TSharedPtr<FAssetsImportController> AssetsImportController;
public:	
	static TSharedPtr<FAssetsImportController> Get();
	void ImportAssets(const FString & AssetsImportJson);
};

void ImportExchangeAsset(const FString& Source, const FString& Destination, const bool bImportTextures, const TFunction<void(const TArray<UObject*>&)>& Callback);
void ImportExchangeAssetWithInterchange(const FString& Source, const FString& Destination, const bool bImportTextures, const TFunction<void(const TArray<UObject*>&)>& Callback);
bool ImportModel(FString ModelPath, FString RootDestination, UMaterialInstanceConstant* MaterialInstance, TMap<FString, UMaterialInstanceConstant*> LodToMaterialInstance, FAssetTypeData Payload);
