// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "FLAssetImportData.h"

class UMaterialInstanceConstant;
class UTexture;

void ApplyMaterialToSelection(UMaterialInstanceConstant* MaterialInstance);
UTexture* ImportTexture(UAssetImportTask * TextureImportTask, FString TextureType, bool FlipNMapGreenChannel);
UAssetImportTask* CreateImportTask(FString TexturePath, const FString& TexturesDestination);
UMaterialInstanceConstant* ImportMaterial(const FAssetMaterial& MaterialPayload, const FAssetTypeData& Payload, const FString RootDestination);
