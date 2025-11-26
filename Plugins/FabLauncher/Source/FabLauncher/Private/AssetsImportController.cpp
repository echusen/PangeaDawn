// Copyright Epic Games, Inc. All Rights Reserved.
#include "IFabLauncherModule.h"
#include "FLAssetsImportController.h"
#include "FLAssetImportDataHandler.h"
#include "AssetImporters/FLImportSurface.h"

#if (ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 3)
#include "FabQuixelAssetTypes.h"
#include "FabQuixelGLTFImporter.h"
#include "JsonObjectConverter.h"
#include "Misc/FileHelper.h"
#endif

#include "AssetToolsModule.h"

#include "Logging/LogMacros.h"

#include "Misc/MessageDialog.h"
#include "Runtime/Core/Public/Internationalization/Text.h"
#include "Misc/Paths.h"
#include "Utilities/FLMeshOp.h"
#include "Utilities/FLMiscUtils.h"

// Used for updated imports
#include "AssetImportTask.h"
#include "IAssetTools.h"
#include "InterchangeProjectSettings.h"
#include "InterchangeGenericScenesPipeline.h"
#include "Async/Async.h"
#include "Factories/FbxImportUI.h"
#include "Factories/FbxStaticMeshImportData.h"

#include "EditorAssetLibrary.h"
#include "EditorStaticMeshLibrary.h"
#include "Engine/SkeletalMesh.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/SoftObjectPath.h"

#if (ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 2)
#include "Engine/SkinnedAssetCommon.h"
#else
#include "Engine/SkeletalMesh.h"
#endif

#if (ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION > 0)
#include "InterchangeGenericMaterialPipeline.h"
#include "InterchangeGenericMeshPipeline.h"
#include "InterchangeGenericAssetsPipeline.h"
#include "InterchangeGenericTexturePipeline.h"
#endif

// TODO: we might need to update the versions here, needs testing against 5.4
#if (ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >=3)
#include "InterchangeglTFPipeline.h"
#endif

#if (ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION <3)
// #include "USDStageImporter/USDStageImportContext.h"
#include "USDStageImportOptions.h"
#endif

#include "Modules/ModuleManager.h"
#include "Interfaces/IPluginManager.h"

#include "SocketSubsystem.h"
#include "Runtime/Sockets/Public/Sockets.h"
#include "Sockets.h"

#include "Framework/Notifications/NotificationManager.h"
#include "FabSharedInterchangeHandler.h"
#include "PackedLevelActor/PackedLevelActor.h"
#include "Engine/StaticMeshActor.h"
#include "Animation/SkeletalMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"


TSharedPtr<FAssetsImportController> FAssetsImportController::AssetsImportController;

// Get instance of the class
TSharedPtr<FAssetsImportController> FAssetsImportController::Get()
{
	if (!AssetsImportController.IsValid())
	{
		AssetsImportController = MakeShareable(new FAssetsImportController);	
	}
	return AssetsImportController;
}

bool SendStatusInfo(FString Status, FString Message, FString Id, FString Path)
{
	TSharedRef<FInternetAddr> RemoteAddr = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
	bool bIsValid;
	FString HostIP = "127.0.0.1";
	int Port = 24563;
	RemoteAddr->SetIp(*HostIP, bIsValid);
	RemoteAddr->SetPort(Port);
	if(!bIsValid) {
		UE_LOG(LogFabLauncher, Error, TEXT("Invalid IP address %s"), *HostIP);
		return false;
	}

	FSocket* SenderSocket = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateSocket( NAME_Stream, TEXT( "TCPSender" ) );
	if(!SenderSocket) {
		UE_LOG(LogFabLauncher, Error, TEXT("Invalid sender socket, this should not happen"));
		return false;
	}

	// Get UE Version string
	FString EngineVersion = TEXT("Unknown");
	#if (ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION == 0)
	EngineVersion = TEXT("5.0");
	#elif (ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION == 1)
	EngineVersion = TEXT("5.1");
	#elif (ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION == 2)
	EngineVersion = TEXT("5.2");
	#elif (ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION == 3)
	EngineVersion = TEXT("5.3");
	#elif (ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION == 4)
	EngineVersion = TEXT("5.4");
	#elif (ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION == 5)
	EngineVersion = TEXT("5.5");
	#elif (ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION == 6)
	EngineVersion = TEXT("5.6");
	#elif (ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION == 7)
	EngineVersion = TEXT("5.7");
	#endif

	// Get the plugin version from the .uplugin file
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("FabLauncher"));
	FString PluginVersion = Plugin.IsValid() ? Plugin->GetDescriptor().VersionName : "unknown";

	// Prepare the payload string
	FString CleanedPath = Path.Replace(TEXT("\\\\"), TEXT("\\")).Replace(TEXT("\\"), TEXT("\\\\"));
	FString PayloadString = "{\"status\":\"" + Status + "\", \"id\":\"" + Id + "\", \"path\":\"" + CleanedPath + "\", \"plugin_version\": \"" + PluginVersion + "\", \"message\": \"" + Message + "\", \"app_name\" : \"Unreal Engine\", \"app_version\": \"" + EngineVersion + "\"}";

	int32 BytesSent = 0;
	SenderSocket->Connect(RemoteAddr.Get());
	int dataSize;
	SenderSocket->SetSendBufferSize(8192, dataSize);

	FTCHARToUTF8 UTF8String(*PayloadString);
	int32 NumBytes = UTF8String.Length() + 1; // +1 to take into account \0

	int32 Sent = 0;
	bool bSent = SenderSocket->Send((uint8*)TCHAR_TO_UTF8(*PayloadString), NumBytes, BytesSent);
	SenderSocket->Close();

	if((BytesSent <= 0) || !bSent)
	{
		UE_LOG(LogFabLauncher, Error,TEXT("No data was sent to EGL"));
		return false;
	}

	UE_LOG(LogFabLauncher, Log, TEXT("Status data sent: %s"), *PayloadString)

	return true;
}

FString ExtractTierNameFromFilename(const FString& FileName)
{
	if (FileName.IsEmpty())
		return "";

	// Get clean filename without extension
	const FString CleanFileName = FPaths::GetBaseFilename(FileName);

	// Split filename by '_'
	TArray<FString> SplitString;
	CleanFileName.ParseIntoArray(SplitString, TEXT("_"), true);

	// Pick last part (tier)
	const FString TierString = SplitString.Last();

	// Convert the extracted string to an integer
	const int32 Tier = TierString.IsNumeric() ? FCString::Atoi(*TierString) : -1;
	if (Tier == 0)
		return "Raw";
	if (Tier == 1)
		return "High";
	if (Tier == 2)
		return "Medium";
	if (Tier == 3)
		return "Low";

	return "";
}

#if (ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 3)
TTuple<FString, FString> ExtractMeta(TSharedPtr<FJsonObject> JsonMetadata, const FString& GltfFile)
{
	FFabAssetMetaDataJson AssetMetaDataJson;
	//  return FJsonObjectConverter::JsonObjectToUStruct(Obj.ToSharedRef(), &Out, 0, 0);
	FJsonObjectConverter::JsonObjectToUStruct(JsonMetadata.ToSharedRef(), &AssetMetaDataJson, 0);

	const FString AssetId = AssetMetaDataJson.Id;

	if (!GltfFile.IsEmpty() && AssetMetaDataJson.Displacement_Scale_Tier1 >= 0.0f && AssetMetaDataJson.Displacement_Bias_Tier1 >= 0.0f)
	{
		// Temporary till displacement values are integrated in gltf
		const TSharedPtr<FJsonObject> DisplacementObject = MakeShareable(new FJsonObject);
		DisplacementObject->SetNumberField(TEXT("magnitude"), AssetMetaDataJson.Displacement_Scale_Tier1);
		DisplacementObject->SetNumberField(TEXT("center"), AssetMetaDataJson.Displacement_Bias_Tier1);
		if (FString GltfFileData; FFileHelper::LoadFileToString(GltfFileData, *GltfFile))
		{
			TSharedPtr<FJsonObject> GltfJson = MakeShareable(new FJsonObject);
			if (FJsonSerializer::Deserialize(TJsonReaderFactory<TCHAR>::Create(GltfFileData), GltfJson))
			{
				if (const TArray<TSharedPtr<FJsonValue>>* Materials; GltfJson->TryGetArrayField(TEXT("materials"), Materials))
				{
					for (const TSharedPtr<FJsonValue>& Material : *Materials)
					{
						const TSharedPtr<FJsonObject>& MaterialObject = Material->AsObject();
						if (const TSharedPtr<FJsonObject>* Extras; MaterialObject->TryGetObjectField(TEXT("extras"), Extras))
						{
							if (const TSharedPtr<FJsonObject>* Overrides; Extras->Get()->TryGetObjectField(TEXT("overrides"), Overrides))
							{
								Overrides->Get()->SetObjectField(TEXT("displacement"), DisplacementObject);
							}
							else
							{
								TSharedPtr<FJsonObject> OverridesObject = MakeShareable(new FJsonObject);
								OverridesObject->SetObjectField(TEXT("displacement"), DisplacementObject);
								Extras->Get()->SetObjectField(TEXT("overrides"), OverridesObject);
							}
						}
						else
						{
							TSharedPtr<FJsonObject> OverridesObject = MakeShareable(new FJsonObject);
							OverridesObject->SetObjectField(TEXT("displacement"), DisplacementObject);
							TSharedPtr<FJsonObject> ExtrasObject = MakeShareable(new FJsonObject);
							ExtrasObject->SetObjectField(TEXT("overrides"), OverridesObject);

							MaterialObject->SetObjectField(TEXT("extras"), ExtrasObject);
						}
					}
				}

				if (FString SerializedJson; FJsonSerializer::Serialize(GltfJson.ToSharedRef(), TJsonWriterFactory<TCHAR>::Create(&SerializedJson, 2)))
				{
					FFileHelper::SaveStringToFile(SerializedJson, *GltfFile);
				}
			}
		}
	}

	if (AssetMetaDataJson.Categories.Num() == 0)
	{
		return {
			AssetId,
			""
		};
	}

	if (AssetMetaDataJson.Categories[0] == "3d")
	{
		return {
			AssetId,
			"3D"
		};
	}

	if (AssetMetaDataJson.Categories[0] == "surface")
	{
		return {
			AssetId,
			"Surfaces"
		};
	}

	if (AssetMetaDataJson.Categories[0] == "3dplant")
	{
		return {
			AssetId,
			"Plants"
		};
	}

	if (AssetMetaDataJson.Categories[0] == "atlas" && AssetMetaDataJson.Categories.Num() > 1)
	{
		if (AssetMetaDataJson.Categories[1] == "decals")
		{
			return {
				AssetId,
				"Decals"
			};
		}
		if (AssetMetaDataJson.Categories[1] == "imperfections")
		{
			return {
				AssetId,
				"Imperfections"
			};
		}
	}

	if (AssetMetaDataJson.SemanticTags.Asset_Type == "decal")
	{
		return {
			AssetId,
			"Decals"
		};
	}

	return {
		AssetId,
		""
	};
}
#endif

FString GetImportLocation(const FString& RootLocation, const FAssetTypeData& Payload, bool bMegascan)
{
	FString AssetName = (Payload.MetadataFab->ListingTitle != "") ? Payload.MetadataFab->ListingTitle : Payload.Id;
	const TCHAR* InvalidChar = INVALID_OBJECTPATH_CHARACTERS INVALID_LONGPACKAGE_CHARACTERS;
	while (*InvalidChar)
	{
		AssetName.ReplaceCharInline(*InvalidChar, TCHAR('_'), ESearchCase::CaseSensitive);
		++InvalidChar;
	}
	AssetName.ReplaceCharInline('/', TCHAR('_'), ESearchCase::CaseSensitive);

	if(!bMegascan)
	{
		return FPaths::Combine(RootLocation, GetUniqueAssetName(RootLocation, AssetName));
	}
	else
	{
#if (ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 3)
		TTuple<FString, FString> Meta = ExtractMeta(Payload.MetadataMegascans, "");
		FString MegascanId = Meta.Get<0>();
		FString SubType = Meta.Get<1>();
		if(!MegascanId.IsEmpty())
		{
			AssetName = AssetName + '_' + MegascanId;
		}
		FString ImportLocation = RootLocation / "Megascans" / SubType;
#else
		// Prior to 5.3, put everything in /Fab/Megascans
		FString ImportLocation = RootDestination / "Megascans";
#endif
		if(!Payload.MetadataFab->Quality.IsEmpty())
		{
			// Megascans assets will go into a /Game/Fab/Megascans/3D/name_id directory
			// Overwriting will be handled through suffixing the tiers: in /Game/Fab/Megascans/3D/name_id, there'll be "Low", "Low_1", "Low_2"...
			FString CapitalizedTier = Payload.MetadataFab->Quality.Left(1).ToUpper() + Payload.MetadataFab->Quality.Mid(1).ToLower();
			FString UniquifiedTier = GetUniqueAssetName(ImportLocation / AssetName, CapitalizedTier);
			ImportLocation = ImportLocation / AssetName / UniquifiedTier;
		}
		else
		{
			// If there is no quality tier, we'll have /Game/Fab/Megascans/3D/name_id, /Game/Fab/Megascans/3D/name_id_1
			AssetName = GetUniqueAssetName(ImportLocation, AssetName);
			ImportLocation = ImportLocation / AssetName;
		}
		return ImportLocation;
	}
}

void FocusWithDelay(const FString& Destination)
{
	FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateLambda([Destination](float DeltaTime)
		{
			FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
			IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
			AssetRegistry.ScanPathsSynchronous({ Destination }, true);
			AssetUtils::FocusOnSelected(Destination);
			return false;
		}),
		1.0f
	);
}

void ImportMetahumanPackages(const FString& RootLocation, TArray<FAssetTypeData>& Payloads)
{
	// Some Metahumans require the presence of Metahuman Creator plugin, check if it is available
	IPluginManager& UEPluginManager = IPluginManager::Get();
	bool bMetahumanCreatorPluginActive = UEPluginManager.FindPlugin(TEXT("MetaHumanCharacter")).Get()
		&& FModuleManager::Get().IsModuleLoaded("MetaHumanCharacter")
		&& FModuleManager::Get().IsModuleLoaded("MetaHumanCharacterEditor");

	// If it is not available, offer to cancel or continue nonetheless.
	if (!bMetahumanCreatorPluginActive)
	{
		const FText Title = FText::FromString(TEXT("MetaHuman plugin unavailable"));
		const FText Body  = FText::FromString(TEXT("The MetaHuman Creator plugin is not activated, but some features of metahuman files might require it. Do you want to continue importing files ?"));
		EAppReturnType::Type Response = FMessageDialog::Open(EAppMsgType::OkCancel, Body, Title);
		if (Response != EAppReturnType::Ok)
		{
			UE_LOG(LogTemp, Log, TEXT("User cancelled the Metahuman imports"));
			return;
			// TODO: SendStatusInfo("critical", "MetaHuman Creator plugin is not enabled, please activate it and restart UE editor", Payload.Id, Payload.Path); ?
		}
	}

	// Run the import for all payloads
	IAssetTools& AssetTools = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();
	int32 index = Payloads.Num() - 1;
	while (index >= 0)
	{
		const FAssetTypeData Payload = Payloads[index];

		FString ImportLocation = GetImportLocation(RootLocation, Payload, false);
		TArray<FString> MetahumanPackageFiles;

		for(const FString& NativeFile : Payload.NativeFiles)
		{
			if(FPaths::FileExists(NativeFile) && FPaths::GetExtension(NativeFile).ToLower() == TEXT("mhpkg"))
			{
				MetahumanPackageFiles.Add(NativeFile);
			}
		}

		if(MetahumanPackageFiles.Num() > 0)
		{
			TArray<UObject*> ImportedAssets = AssetTools.ImportAssets(MetahumanPackageFiles, ImportLocation);

			if(ImportedAssets.Num() == 0)
			{
				if(!bMetahumanCreatorPluginActive) {
					// In case of failure, check if the correct plugin is available and enabled, and throw an error message accordingly
					// Note that in this case, users will have an error visible in UE, and would already have aknowledged the issue by clicking "OK"
					// Only throw a warning to be able to differentiate in analytics events (not ideal)
					SendStatusInfo("warning", "Metahuman import failed, and plugin is not available", Payload.Id, Payload.Path);
				}
				else
				{
					// There is is an unknown issue, send an error and go on
					SendStatusInfo("critical", "Metahuman import failed, although the plugin is available", Payload.Id, Payload.Path);
				}
			}
			else
			{
				FocusWithDelay(ImportLocation);
				SendStatusInfo("success", "Metahuman import successful", Payload.Id, Payload.Path);
			}

			// Remove this payload for next iterations (of non-Metahuman content)
			Payloads.RemoveAt(index);
		}

		index -= 1;
	}
}

bool TriggerMegascanGltfImport(
	const FString& GltfSourceFile,
	const FAssetTypeData& Payload,
	const FString& ImportLocation,
	TSharedRef<FThreadSafeCounter, ESPMode::ThreadSafe> NumOngoingMeshImports
)
{
#if (ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 3)
	TTuple<FString, FString> Meta = ExtractMeta(Payload.MetadataMegascans, GltfSourceFile);
	FString MegascanId = Meta.Get<0>();
	FString SubType = Meta.Get<1>();
	FString TierString   = ExtractTierNameFromFilename(GltfSourceFile);

	auto OnDone = [Payload, ImportLocation, NumOngoingMeshImports](const TArray<UObject*>& Objects)
	{
		bool bIsLastImport = NumOngoingMeshImports->Decrement() == 0;
		if (!Objects.IsEmpty())
		{
			FocusWithDelay(ImportLocation);
			if(bIsLastImport) SendStatusInfo("success", "Successfully imported Megascans asset", Payload.Id, Payload.Path);
		}
		else
		{
			if(bIsLastImport) SendStatusInfo("critical", "Nothing imported through Megascans pipeline", Payload.Id, Payload.Path);
		}
	};

	if (SubType == TEXT("3D"))
	{
		FFabQuixelGltfImporter::ImportAsset(GltfSourceFile, ImportLocation, EFabMegascanImportType::Model3D, OnDone);
	}
	else if (SubType == "Plants")
	{
		FFabQuixelGltfImporter::ImportAsset(GltfSourceFile, ImportLocation, EFabMegascanImportType::Plant, OnDone, TierString == "Raw");
	}
	else if (SubType == "Decals")
	{
		FFabQuixelGltfImporter::ImportAsset(GltfSourceFile, ImportLocation, EFabMegascanImportType::Decal, OnDone);
	}
	else if (SubType == "Imperfections")
	{
		FFabQuixelGltfImporter::ImportAsset(GltfSourceFile, ImportLocation, EFabMegascanImportType::Imperfection, OnDone);
	}
	else if (SubType == "Surfaces")
	{
		FFabQuixelGltfImporter::ImportAsset(GltfSourceFile, ImportLocation, EFabMegascanImportType::Surface, OnDone);
	}
	else
	{
		UE_LOG(LogFabLauncher, Error, TEXT("Invalid Quixel asset type: %s"), *SubType);
		return false;
	}

	return true;
#endif 
	return false;
}

bool TriggerNonMegascanImport(
	FString ModelPath,
	FString RootDestination,
	UMaterialInstanceConstant* MaterialInstance,
	TMap<FString, UMaterialInstanceConstant*> LodToMaterialInstance,
	FAssetTypeData Payload,
	TSharedRef<FThreadSafeCounter, ESPMode::ThreadSafe> NumOngoingMeshImports
)
{
	// Check that the file can be imported
	FString Extension = FPaths::GetExtension(ModelPath).ToLower();
	if(!FPaths::FileExists(ModelPath)) {
		UE_LOG(LogFabLauncher, Error, TEXT("Model path does not exist (%s)"), *ModelPath);
		return false;
	}
	if ((Extension == TEXT("usd")) || (Extension == TEXT("usda")) || (Extension == TEXT("usdc")) || (Extension == TEXT("usdz"))) {
		if (!FModuleManager::Get().IsModuleLoaded("USDImporter"))
		{
			UE_LOG(LogFabLauncher, Warning, TEXT("USDImporter is not loaded"));
		}
		IPluginManager& UEPluginManager = IPluginManager::Get();
		if(!UEPluginManager.FindPlugin(TEXT("USDImporter")).Get()) {
			UE_LOG(LogFabLauncher, Error, TEXT("USDImporter plugin not activated, cannot import USD files"));
			return false;
		}
	}
	else if ((Extension == TEXT("gltf")) || (Extension == TEXT("glb"))) {}
	else if ((Extension == TEXT("fbx")) || (Extension == TEXT("obj")))
	{
		// if(MaterialInstance)
		// {
		// 	// FBX and OBJ will be imported in a "Meshes" directory if a material was imported alongside it through payload
		// 	RootDestination = FPaths::Combine(RootDestination, "Meshes");
		// }
	}
	else {
		UE_LOG(LogFabLauncher, Error, TEXT("Extension %s is not recognized"), *Extension);
		return false;
	}

	ImportFabAssetWithInterchange(
		ModelPath,
		RootDestination,
		[MaterialInstance](TArray<UInterchangePipelineBase*> Pipelines, bool bHasInstancesOrComplexHierarchy){
			for (UInterchangePipelineBase* Pipeline : Pipelines)
			{
				if (UInterchangeGenericAssetsPipeline* const GenericAssetsPipeline = Cast<UInterchangeGenericAssetsPipeline>(Pipeline))
				{
					GenericAssetsPipeline->MeshPipeline->bImportStaticMeshes                  = true;
					GenericAssetsPipeline->MeshPipeline->bImportSkeletalMeshes                = true;
					GenericAssetsPipeline->MeshPipeline->bCombineStaticMeshes                 = false;
					GenericAssetsPipeline->MeshPipeline->SkeletalMeshImportContentType        = EInterchangeSkeletalMeshContentType::All;
					GenericAssetsPipeline->MeshPipeline->bGenerateLightmapUVs                 = true;
					GenericAssetsPipeline->MeshPipeline->bBuildNanite                         = false;
					GenericAssetsPipeline->MaterialPipeline->bImportMaterials                 = true;
					GenericAssetsPipeline->MaterialPipeline->TexturePipeline->bImportTextures = MaterialInstance == nullptr;
					GenericAssetsPipeline->MaterialPipeline->MaterialImport                	  = EInterchangeMaterialImportOption::ImportAsMaterialInstances;
					GenericAssetsPipeline->CommonMeshesProperties->bRecomputeNormals       	  = false;
					GenericAssetsPipeline->CommonMeshesProperties->bComputeWeightedNormals 	  = false;
					GenericAssetsPipeline->CommonMeshesProperties->VertexColorImportOption 	  = EInterchangeVertexColorImportOption::IVCIO_Replace;
					if(bHasInstancesOrComplexHierarchy)
					{
						GenericAssetsPipeline->MeshPipeline->bCombineStaticMeshes = false;
						GenericAssetsPipeline->MeshPipeline->CommonMeshesProperties->bBakeMeshes = false;
						#if (ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 5)
						GenericAssetsPipeline->MeshPipeline->CommonMeshesProperties->bBakePivotMeshes = false;
						#endif
					}
				}
				else if (UInterchangeGenericTexturePipeline* const GenericTexturePipeline = Cast<UInterchangeGenericTexturePipeline>(Pipeline))
				{
					GenericTexturePipeline->bAllowNonPowerOfTwo     = true;
					GenericTexturePipeline->bDetectNormalMapTexture = true;
				}
				// else if(UInterchangeGenericLevelPipeline* LevelPipeline = Cast<UInterchangeGenericLevelPipeline>(Pipeline))
				// {
				// 	#if (ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 5)
				// 	LevelPipeline->SceneHierarchyType = EInterchangeSceneHierarchyType::CreatePackedActor;
				// 	#endif
				// }
				#if (ENGINE_MAJOR_VERSION >=5 && ENGINE_MINOR_VERSION <=3)
				else if (UInterchangeGLTFPipeline* const GLTFGeneratedPipeline = Cast<UInterchangeGLTFPipeline>(Pipeline))
				{
					GLTFGeneratedPipeline->bUseGLTFMaterialInstanceLibrary = true;
				}
				#endif
			}
		},
		[MaterialInstance, RootDestination, Payload, NumOngoingMeshImports](const TArray<UObject*>& Objects)
		{
			if(MaterialInstance)
			{
				TSet<UObject*> MaterialsToDelete;

				for(UObject* Object : Objects)
				{
					if (UStaticMesh* ImportedStaticMesh = Cast<UStaticMesh>(Object))
					{
						const TArray<FStaticMaterial>& Materials = ImportedStaticMesh->GetStaticMaterials();
						for (int32 Index = 0; Index < Materials.Num(); ++Index)
						{
							MaterialsToDelete.Add(Materials[Index].MaterialInterface);
							ImportedStaticMesh->SetMaterial(Index, CastChecked<UMaterialInterface>(MaterialInstance));
						}
						ImportedStaticMesh->MarkPackageDirty();
						ImportedStaticMesh->PostEditChange();
					}
					else if(AStaticMeshActor* ImportedMeshActor = Cast<AStaticMeshActor>(Object))
					{
						if (UStaticMeshComponent* Component = ImportedMeshActor->GetStaticMeshComponent())
						{
							for(int32 SlotIndex = 0 ; SlotIndex < Component->GetNumMaterials() ; SlotIndex++)
							{
								// MaterialsToDelete.Add(Component->GetMaterial(SlotIndex));
								Component->SetMaterial(SlotIndex, MaterialInstance);
								Component->MarkRenderStateDirty();
							}
						}
						ImportedMeshActor->MarkPackageDirty();
						ImportedMeshActor->PostEditChange();
					}
					else if(ASkeletalMeshActor* MeshActor = Cast<ASkeletalMeshActor>(Object))
					{
						if (UStaticMeshComponent* Component = ImportedMeshActor->GetStaticMeshComponent())
						{
							for(int32 SlotIndex = 0 ; SlotIndex < Component->GetNumMaterials() ; SlotIndex++)
							{
								// MaterialsToDelete.Add(Component->GetMaterial(SlotIndex));
								Component->SetMaterial(SlotIndex, MaterialInstance);
								Component->MarkRenderStateDirty();
							}
						}
						MeshActor->MarkPackageDirty();
						MeshActor->PostEditChange();
					}
					else if (USkeletalMesh* ImportedSkeletalMesh = Cast<USkeletalMesh>(Object))
					{
						FSkeletalMaterial SkeletalMaterial(MaterialInstance, true, false, MaterialInstance->GetFName(), FName("None"));
						for (int32 Index = 0; Index < ImportedSkeletalMesh->GetMaterials().Num(); ++Index)
						{
							MaterialsToDelete.Add(ImportedSkeletalMesh->GetMaterials()[Index].MaterialInterface);
							ImportedSkeletalMesh->GetMaterials()[Index] = SkeletalMaterial;
						}
						ImportedSkeletalMesh->MarkPackageDirty();
						ImportedSkeletalMesh->PostEditChange();
					}
					else if (UMaterialInstanceConstant* ImportedMaterialInstance = Cast<UMaterialInstanceConstant>(Object))
					{
						FSoftObjectPath SoftPath(ImportedMaterialInstance);
						FString AssetPath = SoftPath.ToString();
						if (AssetPath != "")
						{
							UEditorAssetLibrary::DeleteAsset(AssetPath);
						}
					}
				}

				// Remove old materials
				if(MaterialsToDelete.Num() > 0)
				{
					TArray<UObject*> MaterialsToDeleteArray;
					for(const auto& Material : MaterialsToDelete)
					{
						if(Material && Material->GetOutermost()->GetName().StartsWith("/Game"))
						{
							MaterialsToDeleteArray.Add(Material);
						}
					}
					UE_LOG(LogTemp, Error, TEXT("%d OBJECTS TO DELETE"), MaterialsToDeleteArray.Num());
					UEditorAssetLibrary::DeleteLoadedAssets(MaterialsToDeleteArray);
				}
			}
			
			bool bIsLastImport = NumOngoingMeshImports->Decrement() == 0;
			if(Objects.Num() != 0)
			{
				FocusWithDelay(RootDestination);
				if(bIsLastImport) SendStatusInfo("success", "Import finished", Payload.Id, Payload.Path);
			}
			else
			{
				if(bIsLastImport) SendStatusInfo("critical", "Empty result list after import", Payload.Id, Payload.Path);
			}
		}
	);

	return true;
}

void ImportAdditionalTextures(const TArray<FString>& Textures, const FString& ImportLocation)
{
	for(const FString& TexturePath : Textures) {
		UAssetImportTask* TextureImportTask = CreateImportTask(TexturePath, ImportLocation);
		if(!TextureImportTask) continue;
		// TODO: We could do those async
		UTexture* TextureAsset = ImportTexture(TextureImportTask, "", false);
		if(!TextureAsset) continue;
		TextureAsset->VirtualTextureStreaming = 0;
		TextureAsset->MarkPackageDirty();
		TextureAsset->PostEditChange();
	}
}

void FAssetsImportController::ImportAssets(const FString& AssetsImportJson)
{
	// Temporarily disable notifications
	const bool bNotificationsPreviouslyAllowed = FSlateNotificationManager::Get().AreNotificationsAllowed();
	FSlateNotificationManager::Get().SetAllowNotifications(false);

	// All assets will be imported under a Fab/ directory, potentially with Metahuman or Megascan subdirectories
	FString RootLocation = "/Game/Fab/";

	// Gather the payloads objects from the json string
	TArray<FAssetTypeData> Payloads = FAssetDataHandler::JsonStringToPayloadsArray(AssetsImportJson);
	{
		if(Payloads.IsEmpty())
		{
			UE_LOG(LogFabLauncher, Error, TEXT("Error reading data from payload received through EGL"));
			UE_LOG(LogFabLauncher, Log, TEXT("Received data: %s"), *AssetsImportJson);
			return;
		}
	}

	// To simplify further handling, do a first pass to import Metahuman files
	// The Payloads array will only contain non-Metahuman payloads afterwards
	bool bHasMetahumanPackages = Payloads.ContainsByPredicate(
		[](const FAssetTypeData& Payload)
		{
			return Payload.NativeFiles.ContainsByPredicate(
				[](const FString& NativeFile)
				{
					return FPaths::GetExtension(NativeFile).ToLower() == TEXT("mhpkg");
				}
			);
		}
	);
	if(bHasMetahumanPackages)
	{
		ImportMetahumanPackages(RootLocation, Payloads);
	}

	// Megascans import would later vary according to different criteria, let's get them here
	bool bFabPluginEnabled = IPluginManager::Get().FindPlugin(TEXT("Fab")).Get() && FModuleManager::Get().IsModuleLoaded("Fab");
	bool bUEAfter53 = false;
#if (ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 3)
	bUEAfter53 = true;
#endif

	for (FAssetTypeData& Payload : Payloads)
	{
		// Checking if we need a custom handling of Megascan assets
		bool bMegascan = Payload.MetadataFab->IsQuixel && Payload.MetadataMegascans.IsValid();
		bool bHasGltf = Payload.Meshes.ContainsByPredicate(
			[](const FAssetMesh& ModelPayload)
			{
				FString Extension = FPaths::GetExtension(ModelPayload.File).ToLower();
				return Extension == TEXT("gltf") || Extension == TEXT("glb");
			}
		);

		// Get a unique import location
		FString ImportLocation = GetImportLocation(RootLocation, Payload, bMegascan);
	
		// Track the number of Pending meshes to import
		// This will be used to display only one success/error per payload
		// This is not ideal as it would not represent partial failures correctly, only the last operation
		TSharedRef<FThreadSafeCounter, ESPMode::ThreadSafe> NumOngoingMeshImports = MakeShared<FThreadSafeCounter, ESPMode::ThreadSafe>();
		NumOngoingMeshImports->Set(Payload.Meshes.Num());

		// Handle the case of Megascan gltf differently if needed (custom pipeline, fallback on standard gltf, fab plugin or not...)
		if(bMegascan && bHasGltf)
		{
			// For recent versions of UE, we try to import gltf assets with the Fab plugin pipelines/content
			if(bUEAfter53)
			{
				if(bFabPluginEnabled)
				{
					ImportAdditionalTextures(Payload.AdditionalTextures, FPaths::Combine(ImportLocation, "Additional_Textures"));
					// We can use the custom pipeline based on the Fab plugin
					for(const FAssetMesh& ModelPayload : Payload.Meshes)
					{
						FString FilePath = ModelPayload.File;
						FString Extension = FPaths::GetExtension(ModelPayload.File).ToLower();
						if(Extension == TEXT("gltf") || Extension == TEXT("glb"))
						{
							if(!TriggerMegascanGltfImport(FilePath, Payload, ImportLocation, NumOngoingMeshImports))
							{
								SendStatusInfo("critical", "An error happened exporting Megascans gltf files", Payload.Id, Payload.Path);
								break;
							}
						}
					}
					continue; // Move to the next payload item
				}
				else
				{
					// By not continuing or returning, assets will be processed as "normal" gltf
					UE_LOG(LogFabLauncher, Warning, TEXT("In UE versions posterior to 5.3, it is recommended to activate the Fab plugin when importing Megascans as .gltf files"));
					SendStatusInfo("warning", "Fab plugin is not active, Megascans gltf imports won't be optimal", Payload.Id, Payload.Path);
				}
			}
			else
			{
				// If the import was not already started, try to replace the .gltf file with the one in "standard" (data hack for Quixel plants)
				for(FAssetMesh& ModelPayload : Payload.Meshes)
				{
					FString FilePath = ModelPayload.File;
					FString Extension = FPaths::GetExtension(ModelPayload.File).ToLower();
					if(Extension == TEXT("gltf") || Extension == TEXT("glb"))
					{
						FString StandardGltfAlternative = "";
						const FString FileDirectory = FPaths::GetPath(FilePath);
						const FString Wildcard = FileDirectory / TEXT("*");
						TArray<FString> CandidateDirectories;
						IFileManager::Get().FindFiles(CandidateDirectories, *Wildcard, false, true);
						for(const FString& CandidateDirectory : CandidateDirectories)
						{
							if(CandidateDirectory == TEXT("standard"))
							{
								TArray<FString> CandidateFiles;
								const FString FilesWildcard = FileDirectory / "standard" / TEXT("*.gltf");
								IFileManager::Get().FindFiles(CandidateFiles, *FilesWildcard, true, false);
								if(CandidateFiles.Num() > 0)
								{
									StandardGltfAlternative = FileDirectory / "standard" / CandidateFiles[0];
								}
								break;
							}
						}
						if(!StandardGltfAlternative.IsEmpty())
						{
							UE_LOG(LogFabLauncher, Warning, TEXT("%s will be imported instead of the initial file"), *StandardGltfAlternative);
							ModelPayload.File = StandardGltfAlternative;
						}
					}
				}
			}
		}
	
		// Import Materials
		TArray<UMaterialInstanceConstant*> ImportedMaterials;
		for(const FAssetMaterial& MaterialPayload : Payload.Materials)
		{
			UMaterialInstanceConstant* ImportedMaterial = ImportMaterial(MaterialPayload, Payload, ImportLocation);
			ImportedMaterials.Add(ImportedMaterial);
		}

		for(const FAssetMesh& ModelPayload : Payload.Meshes)
		{
			// Get a material instance if referenced in the payload
			UMaterialInstanceConstant* MaterialInstance = nullptr;
			if(ModelPayload.MaterialIndex >= 0 && ModelPayload.MaterialIndex < ImportedMaterials.Num()) 
			{
				MaterialInstance = ImportedMaterials[ModelPayload.MaterialIndex];
			} 
			
			// Prepare a map of LOD paths to Material Instance
			TMap<FString, UMaterialInstanceConstant*> LodToMaterialInstance;
			for(const FAssetMeshLod& Lod : ModelPayload.Lods)
			{
				// Get the material instance used for the LOD if necessary
				UMaterialInstanceConstant* LodMaterialInstance = nullptr;
				int LodMaterialIndex = Lod.MaterialIndex == -1 ? ModelPayload.MaterialIndex : Lod.MaterialIndex;
				if(LodMaterialIndex >= 0 && LodMaterialIndex < ImportedMaterials.Num()) 
				{
					MaterialInstance = ImportedMaterials[LodMaterialIndex];
				}
				LodToMaterialInstance.Add(Lod.File, LodMaterialInstance); 
			}

			// Import a file, with a pointer to the material instance
			if(!TriggerNonMegascanImport(ModelPayload.File, ImportLocation, MaterialInstance, LodToMaterialInstance, Payload, NumOngoingMeshImports))
			{
				SendStatusInfo("critical", "An error happened exporting files", Payload.Id, Payload.Path);
				break;
			}
		}

		// Import additional textures if any
		ImportAdditionalTextures(Payload.AdditionalTextures, FPaths::Combine(ImportLocation, "Additional_Textures"));
	}

	// TODO: ideally, we should do this only at the very end of the imports
	FSlateNotificationManager::Get().SetAllowNotifications(bNotificationsPreviouslyAllowed);
}

// // TODO: handle USD and previous versions of UE
// FString Extension = FPaths::GetExtension(Source).ToLower();
// if(Extension == TEXT("usda") || Extension == TEXT("usdc") || Extension == TEXT("usd") || Extension == TEXT("usdz"))
// {
// #if (ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION < 3)
// 	UUsdStageImportOptions* ImportOptions = NewObject<UUsdStageImportOptions>(OptionsOuter);
// 	return ImportOptions;
// #endif
// }