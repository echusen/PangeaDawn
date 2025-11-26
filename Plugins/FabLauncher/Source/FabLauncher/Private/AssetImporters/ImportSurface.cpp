// Copyright Epic Games, Inc. All Rights Reserved.
#include "IFabLauncherModule.h"
#include "FLImportSurface.h"

#include "Utilities/FLMiscUtils.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "AssetImportTask.h"
#include "Editor/UnrealEd/Classes/Factories/MaterialInstanceConstantFactoryNew.h"
#if (ENGINE_MAJOR_VERSION >=5 && ENGINE_MINOR_VERSION >=5)
#include "Runtime/Engine/Public/Materials/MaterialInstanceConstant.h"
#else
#include "Runtime/Engine/Classes/Materials/MaterialInstanceConstant.h"
#endif
#include "Runtime/Engine/Classes/Engine/Texture.h"
#include "MaterialEditingLibrary.h"
#include "EditorAssetLibrary.h"

#include "Runtime/Engine/Classes/Engine/Selection.h"
#include "Editor.h"
#include "Factories/TextureFactory.h"

#include "Runtime/Launch/Resources/Version.h"

UAssetImportTask* CreateImportTask(FString TexturePath, const FString& TexturesDestination)
{
	// UE 5.1 and 5.2 act incorrectly on exr textures (displacement) when packed on R or RGB
	// In such cases, try to fallback to jpg version of the files
	#if (ENGINE_MAJOR_VERSION >=5 && ((ENGINE_MINOR_VERSION == 1) || (ENGINE_MINOR_VERSION == 2)))
	if (FPaths::GetExtension(TexturePath) == TEXT("exr"))
	{
		// Check if there is a jpeg version of the exr file
		FString PathAsJPG = TexturePath.Replace(TEXT("exr"), TEXT("jpg"));
		FString PathAsJPEG = TexturePath.Replace(TEXT("exr"), TEXT("jpeg"));
		if(FPaths::FileExists(PathAsJPG))
		{
			TexturePath = PathAsJPG;
		}
		else if(FPaths::FileExists(PathAsJPEG))
		{
			TexturePath = PathAsJPEG;
		}
		else
		{
			return nullptr;
		}
	}
	#endif

	const FString Filename = FPaths::GetBaseFilename(TexturePath);

	UAssetImportTask* TextureImportTask = NewObject<UAssetImportTask>();
	TextureImportTask->AddToRoot();
	TextureImportTask->bAutomated = true;
	TextureImportTask->bSave = false;
	TextureImportTask->bReplaceExisting = true;
	TextureImportTask->Filename = TexturePath;
	TextureImportTask->DestinationName = GetUniqueAssetName(TexturesDestination, RemoveReservedKeywords(NormalizeString(Filename)));
	TextureImportTask->DestinationPath = TexturesDestination;
	
	// In 5.0, bAsync is not available
	#if (ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION > 0)
	TextureImportTask->bAsync = false;
	#endif

	return TextureImportTask;
}

void ApplyMaterialToSelection(UMaterialInstanceConstant* MaterialInstance)
{
	USelection* SelectedActors = GEditor->GetSelectedActors();
	TArray<AActor*> Actors;
	TArray<ULevel*> UniqueLevels;
	//EAppReturnType::Type applyMat = EAppReturnType::NoAll;
	for (FSelectionIterator Iter(*SelectedActors); Iter; ++Iter)
	{
		//if(applyMat == EAppReturnType::NoAll)
		//	applyMat = FMessageDialog::Open(EAppMsgType::OkCancel, FText(FText::FromString("Do you want to apply imported material to selected assets ?")));
		//if (applyMat == EAppReturnType::Ok) {
		


			AActor* Actor = Cast<AActor>(*Iter);
			TArray<UStaticMeshComponent*> Components;
			Actor->GetComponents<UStaticMeshComponent>(Components);
			for (int32 i = 0; i < Components.Num(); i++)
			{
				UStaticMeshComponent* MeshComponent = Components[i];
				int32 mCnt = MeshComponent->GetNumMaterials();
				for (int j = 0; j < mCnt; j++)
					MeshComponent->SetMaterial(j, MaterialInstance);

			}
		//}
	}
}

UTexture* ImportTexture(UAssetImportTask* TextureImportTask, FString TextureType, bool FlipNMapGreenChannel)
{
	const TArray<FString> NonLinearMaps = { "albedo", "diffuse", "translucency", "specular", "emission"};
	TMap< FString, TextureCompressionSettings> MapCompressionType = {
		{TEXT("albedo"),  TextureCompressionSettings::TC_Default},
		{TEXT("emission"),  TextureCompressionSettings::TC_Default},
		{TEXT("ao"),  TextureCompressionSettings::TC_Alpha},
		{TEXT("roughness"),  TextureCompressionSettings::TC_Alpha},
		{TEXT("metal"),  TextureCompressionSettings::TC_Alpha},
		{TEXT("displacement"),  TextureCompressionSettings::TC_Alpha},
		{TEXT("translucency"),  TextureCompressionSettings::TC_Default},
		{TEXT("opacity"),  TextureCompressionSettings::TC_Alpha},
		{TEXT("normal"),  TextureCompressionSettings::TC_Normalmap}
	};

	UTextureFactory* Factory = NewObject<UTextureFactory>(UTextureFactory::StaticClass());
	Factory->AddToRoot();
	Factory->bCreateMaterial = false;
	if (TextureType == TEXT("normal"))
	{
		Factory->bFlipNormalMapGreenChannel = FlipNMapGreenChannel;
	}

	if (TextureType == TEXT("opacity"))
	{
		Factory->MipGenSettings = TextureMipGenSettings::TMGS_NoMipmaps;
	}

	// Textures without an import type (empty, coming from packed textures) will be Linear masks by default
	Factory->CompressionSettings = MapCompressionType.Contains(TextureType) ? MapCompressionType[TextureType] : TextureCompressionSettings::TC_Masks;
	Factory->ColorSpaceMode = NonLinearMaps.Contains(TextureType) ? ETextureSourceColorSpace::SRGB : ETextureSourceColorSpace::Linear;
	
	TextureImportTask->Options = nullptr;
	TextureImportTask->Factory = Factory;

	const FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
	TArray<UAssetImportTask*> Tasks;
	Tasks.Add(TextureImportTask);
	AssetToolsModule.Get().ImportAssetTasks(Tasks);
	
	UTexture* TextureAsset = nullptr;
	if (TextureImportTask->ImportedObjectPaths.Num() > 0)
	{
		TextureAsset = CastChecked<UTexture>(UEditorAssetLibrary::LoadAsset(TextureImportTask->ImportedObjectPaths[0]));
	} 
	
	TextureImportTask->RemoveFromRoot();
	Factory->RemoveFromRoot();
	
	return TextureAsset;
}

FString GetMasterMaterialName(const FAssetMaterial& MaterialPayload, const FAssetTypeData& FullPayload)
{
	// Gather some generic setings
	bool IsQuixel = FullPayload.MetadataFab->IsQuixel;
	bool IsPlant = FullPayload.MetadataFab->Category.StartsWith("nature-plants--plants");
	bool HasDecal = false;

	if(IsQuixel)
	{
		if(FullPayload.MetadataMegascans.IsValid())
		{
			const auto& Tags = FullPayload.MetadataMegascans->GetArrayField(TEXT("tags"));
			for(const auto& Tag : Tags)
			{
				FString TagString = "";
				if(Tag.IsValid() && Tag->TryGetString(TagString)) 
				{
					if(TagString.Contains(TEXT("surface imperfection"))) 
					{
						return "Imperfection_MasterMaterial";
					}
					else if(TagString.Contains(TEXT("tileable displacement"))) 
					{
						return "Displacement_MasterMaterial";
					}
					else if(TagString.Contains(TEXT("decal"))) 
					{
						HasDecal = true;
					}
				}
			}

			const auto& Categories = FullPayload.MetadataMegascans->GetArrayField(TEXT("categories"));
			FString Category = "";
			if(Categories.Num() > 0) 
			{
				FString FirstCategory = "";
				if(Categories[0].IsValid() && Categories[0]->TryGetString(FirstCategory))
				{
					Category = FirstCategory;
				}
			}

			if(Category == TEXT("brush")) return "BrushDecal_Material";
			else if(Category == TEXT("atlas"))
			{
				if(HasDecal) return "Decal_MasterMaterial";
				else return "Atlas_Material";
			}
		}
		if(IsPlant)
		{
			// TODO: this could be handled differently, but as of today billboards always come in alongside normal textures
			for (const auto& Texture : MaterialPayload.Textures)
			{
				if(Texture.Value.ToLower().Contains(TEXT("_billboard_")))
				{
					return "Billboard_Material";
				}
			}
			return "Foliage_Material";
		}
	}
	// Following, generic materials
	else if(MaterialPayload.Textures.Contains(TEXT("fuzz")))
	{
		return "MS_DefaultMaterial_Fuzzy";
	} 
	else if(MaterialPayload.Textures.Contains(TEXT("opacity")))
	{
		return "MS_Transparent";
	}
	else if(MaterialPayload.Textures.Contains(TEXT("mask"))) 
	{
		return "MS_Cutout";
	}
	else if(MaterialPayload.Textures.Contains(TEXT("displacement")))
	{
		return "MS_DefaultMaterial_Displacement";
	} 
	return "MS_DefaultMaterial";
}

UMaterialInstanceConstant* ImportMaterial(const FAssetMaterial& MaterialPayload, const FAssetTypeData& FullPayload, const FString RootDestination)
{
	// Setup some parameters
	FString MaterialDestination = FPaths::Combine(RootDestination, "Materials");
	FString TexturesDestination = FPaths::Combine(RootDestination, "Textures");
	
	// Get the Master material to use
	CopyPresetTextures();
	CopyUpdatedPresetTextures();
	FString MasterMaterialName = GetMasterMaterialName(MaterialPayload, FullPayload);
	FString MasterMaterialPath = GetMaterial(MasterMaterialName);
	if (!UEditorAssetLibrary::DoesAssetExist(MasterMaterialPath)){
		UE_LOG(LogFabLauncher, Error, TEXT("Failed copying a parent material (should be %s)"), *MasterMaterialPath);
		return nullptr;
	}
	UMaterialInterface* MasterMaterial = CastChecked<UMaterialInterface>(LoadAsset(MasterMaterialPath));
	
	// Find a unique name for the asset
	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
	IAssetTools& AssetTools = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();
	FString MaterialInstanceName = (MaterialPayload.Name != "") ? SanitizeName(MaterialPayload.Name) : "Material";
	FString UniquePackageName, UniqueAssetName;
	AssetTools.CreateUniqueAssetName(MaterialDestination + "/" + MaterialInstanceName, TEXT(""), UniquePackageName, UniqueAssetName);
	
	// Make a Material instance from the master material
	UMaterialInstanceConstantFactoryNew* Factory = NewObject<UMaterialInstanceConstantFactoryNew>();
	UObject* MaterialInstAsset = AssetTools.CreateAsset(UniqueAssetName, MaterialDestination, UMaterialInstanceConstant::StaticClass(), Factory);
	if (MaterialInstAsset == nullptr) return NULL; 
	UMaterialInstanceConstant* MaterialInstance = CastChecked<UMaterialInstanceConstant>(MaterialInstAsset);

	// Parent it
	UMaterialEditingLibrary::SetMaterialInstanceParent(MaterialInstance, MasterMaterial);

	// Import and assign the textures to the material instance
	for(const auto& Texture : MaterialPayload.Textures) {

		FString TextureType = Texture.Key;
		FString TexturePath = Texture.Value;

		// Create the task
		UAssetImportTask* TextureImportTask = CreateImportTask(TexturePath, TexturesDestination);
		if(!TextureImportTask) continue;

		// Actually import the texture
		UTexture* TextureAsset = ImportTexture(TextureImportTask, TextureType, MaterialPayload.FlipNMapGreenChannel);
		if(!TextureAsset) continue;
		TextureAsset->VirtualTextureStreaming = 0;
		TextureAsset->MarkPackageDirty();
		TextureAsset->PostEditChange();

		// Try to assign the texture to a parameter in the material instance
		bool WasSetAsRawParameter = UMaterialEditingLibrary::SetMaterialInstanceTextureParameterValue(MaterialInstance, FName(*TextureType), TextureAsset);
		// Try to adjust for metal and masks
		if(!WasSetAsRawParameter)
		{
			// Try to use metalness, then metallic
			if(TextureType == TEXT("metal"))
			{
				if(!UMaterialEditingLibrary::SetMaterialInstanceTextureParameterValue(MaterialInstance, FName(TEXT("metalness")), TextureAsset))
				{
					UMaterialEditingLibrary::SetMaterialInstanceTextureParameterValue(MaterialInstance, FName(TEXT("metallic")), TextureAsset);
				}
			}
			// Try to use masks as opacity
			if((TextureType == TEXT("mask")) && !(MaterialPayload.Textures.Contains(TEXT("opacity"))))
			{
				UMaterialEditingLibrary::SetMaterialInstanceTextureParameterValue(MaterialInstance, FName(TEXT("opacity")), TextureAsset);
			}
		}
	}

	MaterialInstance->SetFlags(RF_Standalone);
	MaterialInstance->MarkPackageDirty();
	MaterialInstance->PostEditChange();

	// We could save assets
	// AssetUtils::SavePackage(MaterialInstance);

	return MaterialInstance;
}
