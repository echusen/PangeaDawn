// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Runtime/Json/Public/Dom/JsonObject.h"
#include "Runtime/Json/Public/Serialization/JsonSerializer.h"
#include "Materials/MaterialInstanceConstant.h"
#include "FLAssetImportData.h"

class UStaticMesh;



TSharedPtr<FJsonObject>  DeserializeJson(const FString& JsonStringData);
FString GetSourceFLPresetsPath();
FString GetMaterial(const FString & MaterialName);
bool CopyMaterialPreset(const FString & MaterialName);
FString GetFLPresetsName();
UObject* LoadAsset(const FString& AssetPath);
void SaveAsset(const FString& AssetPath);
FString RemoveReservedKeywords(const FString& Name);
FString NormalizeString(FString InputString);
TArray<FString> GetAssetsList(const FString& DirectoryPath);
FString GetRootDestination(const FString& ExportPath);
FString ResolveDestination(const FString& AssetDestination);
bool CopyPresetTextures();
bool CopyUpdatedPresetTextures();

FString GetAssetName(TSharedPtr<FAssetTypeData> AssetImportData);
FString GetUniqueAssetName(const FString& AssetDestination, const FString AssetName, bool FileSearch=false);

FString SanitizeName(const FString& InputName);

namespace AssetUtils {
	//template<typename T>
	// TArray<UMaterialInstanceConstant*> GetSelectedAssets(const FString& AssetClass);
	void FocusOnSelected(const FString& Path);
	void AddStaticMaterial(UStaticMesh* SourceMesh, UMaterialInstanceConstant* NewMaterial);
	void SavePackage(UObject* SourceObject);

	void DeleteDirectory(FString TargetDirectory);
	bool DeleteAsset(const FString& AssetPath);
}
