// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Runtime/Json/Public/Dom/JsonObject.h"
#include "Runtime/Json/Public/Serialization/JsonSerializer.h"

struct FAssetMeshLod {
	FString File;
	int MaterialIndex;
};

struct FAssetMesh {
	FString File;
	FString Name;
	int MaterialIndex;
	TArray<FAssetMeshLod> Lods;
};

struct FAssetMaterial {
	TMap<FString, FString> Textures;
	bool FlipNMapGreenChannel;
	FString Name;
	FString File;
};

struct FAssetMetadataLauncher {
	FString Version;
	int ListeningPort;
};

struct FAssetMetadataFab {
	FString ListingType;
	FString ListingTitle;
	FString Category;
	TArray<FString> Tags;
	FString Format;
	bool IsQuixel;
	FString Quality;
};

struct FAssetTypeData {
	FString Id;
	FString Path;
	TArray<FString> NativeFiles;
	TArray<FString> AdditionalTextures;
	TArray<FAssetMesh> Meshes;
	TArray<FAssetMaterial> Materials;
	TSharedPtr<FAssetMetadataLauncher> MetadataLauncher;
	TSharedPtr<FAssetMetadataFab> MetadataFab;
	TSharedPtr<FJsonObject> MetadataMegascans;
};

struct FAssetsData {
	TArray<FAssetTypeData> AllAssetsData;
};
