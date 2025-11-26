// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "FLAssetImportData.h"
#include "Utilities/FLMiscUtils.h"

class FAssetDataHandler {
private:
	static FAssetTypeData JsonObjectToPayloadObject(TSharedPtr<FJsonObject> AssetDataObject);
public:	
	static TArray<FAssetTypeData> JsonStringToPayloadsArray(const FString& AssetsImportJson);
};
