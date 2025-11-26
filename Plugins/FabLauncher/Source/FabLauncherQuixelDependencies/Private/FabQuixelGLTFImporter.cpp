// Copyright Epic Games, Inc. All Rights Reserved.
// From Fab plugin, UE 5.6

#include "FabQuixelGLTFImporter.h"
#include "FabQuixelAssetTypes.h"
#include "IFabLauncherQuixelDependenciesModule.h"

#include "Editor.h"
#include "InterchangeManager.h"
#include "InterchangeGenericAssetsPipeline.h"
#include "InterchangeGenericMaterialPipeline.h"
#include "InterchangeGenericMeshPipeline.h"
#include "InterchangeGenericScenesPipeline.h"
#include "InterchangeGenericTexturePipeline.h"
#if (ENGINE_MAJOR_VERSION >=5 && ENGINE_MINOR_VERSION <=3)
#include "InterchangeglTFPipeline.h"
#endif

#include "Runtime/Launch/Resources/Version.h"

#include "MaterialTypes.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Kismet2/KismetEditorUtilities.h"

#include "Materials/Material.h"
#include "Materials/MaterialInstance.h"

#include "Misc/FileHelper.h"

#include "Pipelines/FabInterchangeMegascansPipeline.h"

#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonTypes.h"

#include "UObject/SoftObjectPath.h"

#include "FabSharedInterchangeHandler.h"
#include "PackedLevelActor/PackedLevelActor.h"

void FFabQuixelGltfImporter::SetupGlobalFoliageActor(const FString& ImportPath)
{
	const FString GlobalFoliageActorPackageName     = "BP_GlobalFoliageActor";
	const FString GlobalFoliageActorDestinationPath = FPaths::GetPath(FPaths::GetPath(ImportPath)) / GlobalFoliageActorPackageName;

	if (IAssetRegistry::Get()->DoesPackageExistOnDisk(*GlobalFoliageActorDestinationPath) || FindPackage(nullptr, *GlobalFoliageActorDestinationPath))
	{
		return;
	}

	const FString GlobalFoliageActorClass     = "BP_GlobalFoliageActor_UE5.BP_GlobalFoliageActor_UE5_C";
	const FString GlobalFoliageActorClassPath = "/Fab/Actors/GlobalFoliageActor" / GlobalFoliageActorClass;

	if (UPackage* Package = CreatePackage(*GlobalFoliageActorDestinationPath))
	{
		UClass* ParentClass   = Cast<UClass>(FSoftObjectPath(GlobalFoliageActorClassPath).TryLoad());
		UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
			ParentClass,
			Package,
			*GlobalFoliageActorPackageName,
			BPTYPE_Const,
			UBlueprint::StaticClass(),
			UBlueprintGeneratedClass::StaticClass()
		);

		if (Blueprint)
		{
			FAssetRegistryModule::AssetCreated(Blueprint);
			Package->MarkPackageDirty();
		}
		else
		{
			UE_LOG(LogFabLauncherQuixelDependencies, Warning, TEXT("Failed to create blueprint for Foliage Actor"));
		}
	}
	else
	{
		UE_LOG(LogFabLauncherQuixelDependencies, Warning, TEXT("Failed to create a global foliage actor"));
	}
}

void FFabQuixelGltfImporter::ImportAsset(
	const FString& Source,
	const FString& Destination, 
	EFabMegascanImportType ImportType,
	TFunction<void(const TArray<UObject*>&)> Callback,
	bool bBuildNanite
) {
	ImportFabAssetWithInterchange(
		Source,
		Destination, 
		[ImportType, bBuildNanite](TArray<UInterchangePipelineBase*>& Pipelines, bool bHasInstancesOrComplexHierarchy)
		{
			UFabInterchangeMegascansPipeline* MegascansPipeline = NewObject<UFabInterchangeMegascansPipeline>();
			MegascansPipeline->AddToRoot();
			Pipelines.Add(MegascansPipeline);

			for (UInterchangePipelineBase* Pipeline : Pipelines)
			{
				if (UInterchangeGenericAssetsPipeline* AssetPipeline = Cast<UInterchangeGenericAssetsPipeline>(Pipeline))
				{
					AssetPipeline->MeshPipeline->bImportStaticMeshes   = ImportType == EFabMegascanImportType::Plant || ImportType == EFabMegascanImportType::Model3D;
					AssetPipeline->MeshPipeline->bBuildNanite = bBuildNanite;
					AssetPipeline->MeshPipeline->bImportSkeletalMeshes = false;
					AssetPipeline->MaterialPipeline->MaterialImport = EInterchangeMaterialImportOption::ImportAsMaterialInstances;
					if(ImportType == EFabMegascanImportType::Imperfection)
					{
						AssetPipeline->MaterialPipeline->bImportMaterials = false;
						AssetPipeline->MaterialPipeline->TexturePipeline->bImportTextures = true;
					}
					else if (ImportType == EFabMegascanImportType::Plant)
					{
						AssetPipeline->CommonMeshesProperties->bRecomputeNormals = true;
						AssetPipeline->CommonMeshesProperties->bComputeWeightedNormals = true;
						AssetPipeline->MaterialPipeline->TexturePipeline->bFlipNormalMapGreenChannel = true;
						#if (ENGINE_MAJOR_VERSION >=5 && ENGINE_MINOR_VERSION >= 5)
						AssetPipeline->MeshPipeline->bCollision = false;
						#else
						AssetPipeline->MeshPipeline->bImportCollision = false;
						#endif
					}
					if(bHasInstancesOrComplexHierarchy)
					{
						AssetPipeline->MeshPipeline->bCombineStaticMeshes = false;
						AssetPipeline->MeshPipeline->CommonMeshesProperties->bBakeMeshes = false;
						#if (ENGINE_MAJOR_VERSION >=5 && ENGINE_MINOR_VERSION >=5)
						AssetPipeline->MeshPipeline->CommonMeshesProperties->bBakePivotMeshes = true;
						#endif
					}
				}
				else if(UFabInterchangeMegascansPipeline* MegascanPipeline = Cast<UFabInterchangeMegascansPipeline>(Pipeline))
				{
					MegascanPipeline->MegascanImportType = ImportType;
				}
				// #if (ENGINE_MAJOR_VERSION >=5 && ENGINE_MINOR_VERSION >= 5)
				// else if (bHasInstancesOrComplexHierarchy)
				// {
				// 	if(UInterchangeGenericLevelPipeline* LevelPipeline = Cast<UInterchangeGenericLevelPipeline>(Pipeline))
				// 	{
				// 		LevelPipeline->SceneHierarchyType = EInterchangeSceneHierarchyType::CreatePackedActor;
				// 	}
				// }
				// #endif
				#if (ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION <=3)
				else if (UInterchangeGLTFPipeline* const GLTFGeneratedPipeline = Cast<UInterchangeGLTFPipeline>(Pipeline))
				{
					GLTFGeneratedPipeline->bUseGLTFMaterialInstanceLibrary = true;
				}
				#endif
			}
		},
		[Callback, ImportType, Destination] (const TArray<UObject*>& ImportedObjects) {
			if(ImportedObjects.Num() > 0 && ImportType == EFabMegascanImportType::Plant)
			{
				SetupGlobalFoliageActor(Destination);
			}
			Callback(ImportedObjects);
		}
	);
}