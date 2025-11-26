// Copyright Epic Games, Inc. All Rights Reserved.
#include "Utilities/FLMeshOp.h"

#include "AssetImportTask.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "FbxMeshUtils.h"
#include "AbcImportSettings.h"
#include "Utilities/FLMiscUtils.h"
#include "Runtime/Core/Public/Misc/Paths.h"

#include "EditorAssetLibrary.h"
#include "EditorStaticMeshLibrary.h"
#include "Editor/FoliageEdit/Private/FoliageTypeFactory.h"
#include "Runtime/Foliage/Public/InstancedFoliageActor.h"

#include "Runtime/Foliage/Public/FoliageType.h"
#include "Runtime/Foliage/Public/FoliageType_InstancedStaticMesh.h"

#if (ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 5)
#include "UObject/PerPlatformProperties.h"
#else
#include "PerPlatformProperties.h"
#endif

#include "AssetRegistry/AssetRegistryModule.h"
#include "Factories/FbxImportUI.h"
#include "Factories/FbxStaticMeshImportData.h"

#include "HAL/IConsoleManager.h"


TSharedPtr<FMeshOps> FMeshOps::MeshOpsInst;

TSharedPtr<FMeshOps> FMeshOps::Get()
{
	if (!MeshOpsInst.IsValid())
	{
		MeshOpsInst = MakeShareable(new FMeshOps);
	}
	return MeshOpsInst;
}

FString FMeshOps::ImportMesh(const FString& MeshPath, const FString& Destination, const FString& AssetName)
{
	
	FString FileExtension;
	FString FilePath;
	FString AssetPath;

	//UFbxImportUI* ImportOptions;
	MeshPath.Split(TEXT("."), &FilePath, &FileExtension);
	

	FileExtension = FPaths::GetExtension(MeshPath);
	if (FileExtension == TEXT("obj") || FileExtension == TEXT("fbx"))
	{
		UFbxImportUI* ImportOptions;
		ImportOptions = GetFbxOptions();

		if (FileExtension == TEXT("obj"))
		{
			ImportOptions->StaticMeshImportData->ImportRotation = FRotator(0, 0, 90);
		}

		AssetPath = ImportFile(ImportOptions, Destination, AssetName, MeshPath);
	}
	else if (FileExtension == TEXT("abc"))
	{
		UAbcImportSettings* AbcImportOptions = GetAbcSettings();
		AssetPath = ImportFile(AbcImportOptions, Destination, AssetName, MeshPath);

	}
	return AssetPath;
}

void FMeshOps::ApplyLods(const TMap<FString, UMaterialInstanceConstant*>& LodToMaterialInstance, UStaticMesh* SourceMesh)
{
	// TODO: we should handle specific materials for LODs!!!
	DeactivateInterchange();
	int32 LodCounter = 1;
	for (const auto& LodInfo : LodToMaterialInstance)
	{
		if (LodCounter > 7) continue;
		#if (ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 5)
		FbxMeshUtils::ImportStaticMeshLOD(SourceMesh, LodInfo.Key, LodCounter, false);
		#else
		FbxMeshUtils::ImportStaticMeshLOD(SourceMesh, LodInfo.Key, LodCounter);
		#endif
		LodCounter++;
	}
	RestoreInterchange();
}

void FMeshOps::ApplyAbcLods(UStaticMesh* SourceMesh, const TArray<FString>& LodPathList, const FString& AssetDestination)
{
	DeactivateInterchange();
	FString LodDestination = FPaths::Combine(AssetDestination, TEXT("Lods"));
	int32 LodCounter = 1;

	for (FString LodPath : LodPathList)
	{
		if (LodCounter > 7) continue;
		FString ImportedLodPath = ImportMesh(LodPath, LodDestination, "");
		UStaticMesh* ImportedLod = CastChecked<UStaticMesh>(LoadAsset(ImportedLodPath));
		UDEPRECATED_EditorStaticMeshLibrary::SetLodFromStaticMesh(SourceMesh, LodCounter, ImportedLod, 0, true);
		LodCounter++;
	}
	UEditorAssetLibrary::DeleteDirectory(LodDestination);
	RestoreInterchange();
}

void CreateFoliageAsset(const FString& FoliagePath, UStaticMesh* SourceAsset, const FString& FoliageAssetName, bool bSavePackage)
{
	if (SourceAsset == nullptr) return;

	FString FoliageUniqueName = GetUniqueAssetName(FoliagePath, FoliageAssetName);
	
	FString PackageName = FPaths::Combine(FoliagePath, FoliageUniqueName);
	UPackage* Package = CreatePackage(*PackageName);

	UFoliageType_InstancedStaticMesh* FoliageAsset = NewObject<UFoliageType_InstancedStaticMesh>(Package, *FoliageUniqueName, RF_Public | RF_Standalone | RF_MarkAsRootSet);
	FoliageAsset->SetStaticMesh(SourceAsset);
	FAssetRegistryModule::AssetCreated(FoliageAsset);
	FoliageAsset->PostEditChange();
	Package->MarkPackageDirty();
	UWorld* CurrentWorld = GEditor->GetEditorWorldContext().World();
	AInstancedFoliageActor* IFA = nullptr;
	if(CurrentWorld)
	{
		ULevel* CurrentLevel = CurrentWorld->GetCurrentLevel();
		if(CurrentLevel)
		{
			IFA	= CurrentLevel->InstancedFoliageActor.Get();
			if (!IFA)
			{
				FActorSpawnParameters SpawnParams;
				SpawnParams.OverrideLevel = CurrentLevel;
				IFA = CurrentWorld->SpawnActor<AInstancedFoliageActor>(SpawnParams);
				CurrentLevel->InstancedFoliageActor = IFA;
			}
			}
	}
	if(IFA) IFA->AddFoliageType(FoliageAsset);
}



TArray<FString> FMeshOps::ImportLodsAsStaticMesh(const TArray<FString> & LodList, const FString & AssetDestination)
{

	return TArray<FString>();
}

//void FMeshOps::SetFbxOptions(bool bCombineMeshes , bool bGenerateLightmapUVs , bool bAutoGenerateCollision  ,bool bImportMesh , bool bImportAnimations  ,bool bImportMaterials  , bool bImportAsSkeletal ) {
//	ImportOptions = NewObject<UFbxImportUI>();
//
//	ImportOptions->StaticMeshImportData->bCombineMeshes = bCombineMeshes;
//	ImportOptions->StaticMeshImportData->bGenerateLightmapUVs = bGenerateLightmapUVs;
//	ImportOptions->StaticMeshImportData->bAutoGenerateCollision = bAutoGenerateCollision;
//	ImportOptions->StaticMeshImportData->VertexColorImportOption = EVertexColorImportOption::Replace;
//	
//	ImportOptions->bImportMesh = bImportMesh;
//	ImportOptions->bImportAnimations = bImportAnimations;
//	ImportOptions->bImportMaterials = bImportMaterials;
//	ImportOptions->bImportAsSkeletal = bImportAsSkeletal;
//
//}

UFbxImportUI* FMeshOps::GetFbxOptions()
{
	UFbxImportUI* DefaultImportOptions = NewObject<UFbxImportUI>();

	DefaultImportOptions->StaticMeshImportData->bCombineMeshes = bCombineMeshes;
	DefaultImportOptions->StaticMeshImportData->bGenerateLightmapUVs = false;
	DefaultImportOptions->StaticMeshImportData->bAutoGenerateCollision = false;
	DefaultImportOptions->StaticMeshImportData->VertexColorImportOption = EVertexColorImportOption::Replace;

	DefaultImportOptions->bImportMesh = true;
	DefaultImportOptions->bImportAnimations = false;
	DefaultImportOptions->bImportMaterials = false;
	DefaultImportOptions->bImportAsSkeletal = false;
	return DefaultImportOptions;
}

void FMeshOps::RemoveExtraMaterialSlot(UStaticMesh* SourceMesh)
{

	for (int i = 1; i < SourceMesh->GetNumLODs(); i++)
	{
		FMeshSectionInfo MeshSectionInfo = SourceMesh->GetSectionInfoMap().Get(i, 0);
		MeshSectionInfo.MaterialIndex = 0;
		SourceMesh->GetSectionInfoMap().Set(i, 0, MeshSectionInfo);
	}

	int NumOfMaterials = SourceMesh->GetStaticMaterials().Num();
	for (int i = NumOfMaterials; i > 1; i--)
	{
		SourceMesh->GetStaticMaterials().RemoveAt(i - 1);
	}

	SourceMesh->Modify();
	SourceMesh->PostEditChange();
	SourceMesh->MarkPackageDirty();

}

void FMeshOps::DeactivateInterchange() {
	// Deactivate Interchange import in 5.4+
	#if (ENGINE_MAJOR_VERSION >=5 && ENGINE_MINOR_VERSION >=4)
	IConsoleVariable* InterchangeEnable = IConsoleManager::Get().FindConsoleVariable(TEXT("Interchange.FeatureFlags.Import.Enable"));
	InitialInterchangeFeatureFlagValue = InterchangeEnable->GetBool();
	InterchangeEnable->Set(false);
	#endif
}
void FMeshOps::RestoreInterchange() {
	// Reactivate Interchange import if needed in 5.4+
	#if (ENGINE_MAJOR_VERSION >=5 && ENGINE_MINOR_VERSION >=4)
	IConsoleVariable* InterchangeEnable = IConsoleManager::Get().FindConsoleVariable(TEXT("Interchange.FeatureFlags.Import.Enable"));
	InterchangeEnable->Set(InitialInterchangeFeatureFlagValue);
	#endif
}

template<class T>
FString FMeshOps::ImportFile(T * ImportOptions, const FString & Destination, const FString & AssetName, const FString & Source)
{
	DeactivateInterchange();

	FString ImportedMeshPath = TEXT("");
	TArray< UAssetImportTask*> ImportTasks;
	UAssetImportTask* MeshImportTask = NewObject<UAssetImportTask>();
	MeshImportTask->bAutomated = true;
	MeshImportTask->bSave = false;
	MeshImportTask->Filename = Source;

	// In 5.0, bAsync is not available
	#if (ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION > 0)
	MeshImportTask->bAsync = false;
	#endif

	MeshImportTask->DestinationName = AssetName;
	MeshImportTask->DestinationPath = Destination;
	MeshImportTask->bReplaceExisting = true;
	MeshImportTask->Options = ImportOptions;

	ImportTasks.Add(MeshImportTask);

	IAssetTools& AssetTools = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();

	AssetTools.ImportAssetTasks(ImportTasks);

	for (UAssetImportTask* ImpTask : ImportTasks)
	{
		if(ImpTask->ImportedObjectPaths.Num()>0)
			ImportedMeshPath = ImpTask->ImportedObjectPaths[0];
	}

	RestoreInterchange();

	return ImportedMeshPath;
}

UAbcImportSettings* FMeshOps::GetAbcSettings()
{
	UAbcImportSettings* AlembicOptions = NewObject<UAbcImportSettings>();
	AlembicOptions->ImportType = EAlembicImportType::StaticMesh;
	AlembicOptions->StaticMeshSettings.bMergeMeshes = false;
	AlembicOptions->MaterialSettings.bCreateMaterials = false;
	AlembicOptions->SamplingSettings.FrameStart = 0;
	AlembicOptions->SamplingSettings.FrameEnd = 1;
	AlembicOptions->ConversionSettings.Rotation = FVector(90, 00, 00);
	AlembicOptions->ConversionSettings.Scale = FVector(1.0, -1.0, 1.0);
	AlembicOptions->ConversionSettings.bFlipU = false;
	AlembicOptions->ConversionSettings.bFlipV = true;
	return AlembicOptions;
}

void FMeshOps::LodDistanceTest(UStaticMesh* SourceMesh)
{
	SourceMesh->bAutoComputeLODScreenSize = false;
	FStaticMeshSourceModel& SourceModel = SourceMesh->GetSourceModel(0);
	SourceModel.ScreenSize = FPerPlatformFloat(1);
	FStaticMeshSourceModel& SourceModel1 = SourceMesh->GetSourceModel(1);
	SourceModel1.ScreenSize = FPerPlatformFloat(0.5);


}





