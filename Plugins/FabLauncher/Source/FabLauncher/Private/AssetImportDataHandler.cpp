// Copyright Epic Games, Inc. All Rights Reserved.
#include "IFabLauncherModule.h"
#include "FLAssetImportDataHandler.h"

TArray<FAssetTypeData> FAssetDataHandler::JsonStringToPayloadsArray(const FString& AssetsImportJson)
{
	TArray<FAssetTypeData> AllAssetsData;

	// Convert the json data into an array to deserialize it
	FString ImportJsonAsString = AssetsImportJson;
	// ImportJsonAsString.RemoveFromStart("[");
	// ImportJsonAsString.RemoveFromEnd("]");
	ImportJsonAsString = TEXT("{\"Assets\":") + ImportJsonAsString + TEXT("}");	
	TSharedPtr<FJsonObject> JsonObject = DeserializeJson(ImportJsonAsString);

	// Ensure the deserialization went fine
	if(!JsonObject.IsValid())
	{
		UE_LOG(LogFabLauncher, Error, TEXT("Invalid Json deserialization"))
		return AllAssetsData;
	}
	else
	{
		UE_LOG(LogFabLauncher, Log, TEXT("Debug payload: %s"), *AssetsImportJson);
	}
	
	// Convert the json object into an array of Payloads
	TArray<TSharedPtr<FJsonValue>> JsonArray = JsonObject->GetArrayField(TEXT("Assets"));
	for (TSharedPtr<FJsonValue> PayloadJsonObject : JsonArray)
	{
		AllAssetsData.Add(JsonObjectToPayloadObject(PayloadJsonObject->AsObject()));
	}	
	return AllAssetsData;
}

FAssetTypeData FAssetDataHandler::JsonObjectToPayloadObject(TSharedPtr<FJsonObject> AssetDataObject)
{
	FAssetTypeData Payload;

	Payload.Id = AssetDataObject->GetStringField(TEXT("id"));
	Payload.Path = AssetDataObject->GetStringField(TEXT("path"));

	TArray<TSharedPtr<FJsonValue>> NativeFiles = AssetDataObject->GetArrayField(TEXT("native_files"));
	for (TSharedPtr<FJsonValue> File : NativeFiles)
	{
		Payload.NativeFiles.Add(File->AsString());
	}

	TArray<TSharedPtr<FJsonValue>> AdditionalTextures = AssetDataObject->GetArrayField(TEXT("additional_textures"));
	for (TSharedPtr<FJsonValue> Texture : AdditionalTextures)
	{
		Payload.AdditionalTextures.Add(Texture->AsString());
	}

	TArray<TSharedPtr<FJsonValue>> Meshes = AssetDataObject->GetArrayField(TEXT("meshes"));
	for (TSharedPtr<FJsonValue> Mesh : Meshes)
	{
		FAssetMesh ParsedMesh;
		ParsedMesh.Name = Mesh->AsObject()->GetStringField(TEXT("name"));
		ParsedMesh.File = Mesh->AsObject()->GetStringField(TEXT("file"));
		ParsedMesh.MaterialIndex = Mesh->AsObject()->GetIntegerField(TEXT("material_index"));
		TArray<TSharedPtr<FJsonValue>> Lods = Mesh->AsObject()->GetArrayField(TEXT("lods"));
		for (TSharedPtr<FJsonValue> Lod : Lods)
		{
			FAssetMeshLod AssetLod;
			AssetLod.File = Lod->AsObject()->GetStringField(TEXT("file"));
			AssetLod.MaterialIndex = Lod->AsObject()->GetIntegerField(TEXT("material_index"));
			ParsedMesh.Lods.Add(AssetLod);
		}
		Payload.Meshes.Add(ParsedMesh);
	}

	TArray<TSharedPtr<FJsonValue>> Materials = AssetDataObject->GetArrayField(TEXT("materials"));
	for (TSharedPtr<FJsonValue> Material : Materials)
	{
		FAssetMaterial ParsedMaterial;
		ParsedMaterial.Name = Material->AsObject()->GetStringField(TEXT("name"));
		ParsedMaterial.File = Material->AsObject()->GetStringField(TEXT("file"));
		ParsedMaterial.FlipNMapGreenChannel = Material->AsObject()->HasTypedField<EJson::Boolean>(TEXT("flipnmapgreenchannel")) ? Material->AsObject()->GetBoolField(TEXT("flipnmapgreenchannel")) : false;
		TSharedPtr<FJsonObject> Textures = Material->AsObject()->GetObjectField(TEXT("textures"));
		for (const auto& texture : Textures->Values)
		{
			FString Channel = texture.Key;
			FString TexturePath = texture.Value->AsString();
			ParsedMaterial.Textures.Add(TTuple<FString, FString>(Channel, TexturePath));
		}
		Payload.Materials.Add(ParsedMaterial);
	}

	TSharedPtr<FJsonObject> Metadata = AssetDataObject->GetObjectField(TEXT("metadata"));

	Payload.MetadataFab = MakeShareable(new FAssetMetadataFab);
	TSharedPtr<FJsonObject> MetadataFab = Metadata->GetObjectField(TEXT("fab"));
	Payload.MetadataFab->Format = MetadataFab->GetStringField(TEXT("format"));
	Payload.MetadataFab->IsQuixel = MetadataFab->GetBoolField(TEXT("isQuixel"));
	Payload.MetadataFab->Quality = MetadataFab->GetStringField(TEXT("quality"));
	TSharedPtr<FJsonObject> MetadataFabListing = MetadataFab->GetObjectField(TEXT("listing"));
	Payload.MetadataFab->Category = MetadataFabListing->GetObjectField(TEXT("category"))->GetStringField(TEXT("slug"));
	Payload.MetadataFab->ListingType = MetadataFabListing->GetStringField(TEXT("listingType"));
	Payload.MetadataFab->ListingTitle = MetadataFabListing->GetStringField(TEXT("title"));
	// Payload.MetadataFab->Tags = MetadataFabListing->GetStringField("tags");
	
	Payload.MetadataLauncher = MakeShareable(new FAssetMetadataLauncher);
	TSharedPtr<FJsonObject> MetadataLauncher = Metadata->GetObjectField(TEXT("launcher"));
	Payload.MetadataLauncher->Version = MetadataLauncher->GetStringField(TEXT("version"));
	Payload.MetadataLauncher->ListeningPort= MetadataLauncher->GetIntegerField(TEXT("listening_port"));
	
	TSharedPtr<FJsonObject> MetadataMegascans = Metadata->GetObjectField(TEXT("megascans"));
	Payload.MetadataMegascans = MetadataMegascans;

	return Payload;
}