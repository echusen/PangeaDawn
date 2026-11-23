// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.

#include "AIToolsImporter.h"
#include "AscentAIToolsSettings.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Texture2D.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/Skeleton.h"
#include "AssetImportTask.h"
#include "Factories/TextureFactory.h"
#include "Factories/SoundFactory.h"
#include "Factories/FbxFactory.h"
#include "Sound/SoundWave.h"
#include "Factories/FbxImportUI.h"
#include "Factories/FbxSkeletalMeshImportData.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "ObjectTools.h"
#include "UObject/SavePackage.h"
#include "FileHelpers.h"

namespace
{
	FString SanitizeName(const FString& InName)
	{
		FString Clean = ObjectTools::SanitizeObjectName(InName);
		if (Clean.IsEmpty())
		{
			Clean = TEXT("Generated");
		}
		return Clean;
	}

	/** Asset name only. A /Game path passed as the name must not become a second folder. */
	FString AssetLeafName(const FString& InName)
	{
		FString Name = InName.TrimStartAndEnd();
		int32 Slash = INDEX_NONE;
		if (Name.FindLastChar(TEXT('/'), Slash) || Name.FindLastChar(TEXT('\\'), Slash))
		{
			if (Slash != INDEX_NONE && Slash + 1 < Name.Len())
			{
				Name = Name.Mid(Slash + 1);
			}
		}
		int32 Dot = INDEX_NONE;
		if (Name.FindChar(TEXT('.'), Dot) && Dot > 0)
		{
			Name = Name.Left(Dot);
		}
		return SanitizeName(Name);
	}

	FString WriteTempFile(const TArray<uint8>& Bytes, const FString& Extension)
	{
		const FString Dir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("AscentAITools"));
		IFileManager::Get().MakeDirectory(*Dir, true);

		const FString FileName = FString::Printf(TEXT("%s_%s.%s"),
			TEXT("AIGen"), *FGuid::NewGuid().ToString(EGuidFormats::Digits), *Extension);
		const FString FullPath = FPaths::Combine(Dir, FileName);

		if (!FFileHelper::SaveArrayToFile(Bytes, *FullPath))
		{
			return FString();
		}
		return FullPath;
	}

	/** Returns the standard asset-name prefix for the given object's type (empty if none). */
	FString TypePrefixFor(const UObject* Obj)
	{
		if (!Obj)
		{
			return FString();
		}
		const FString ClassName = Obj->GetClass()->GetName();
		if (ClassName.Contains(TEXT("MaterialInstance")))	return TEXT("MI_");
		if (ClassName.Contains(TEXT("Material")))			return TEXT("M_");
		if (ClassName.Contains(TEXT("SkeletalMesh")))		return TEXT("SK_");
		if (ClassName.Contains(TEXT("Skeleton")))			return TEXT("SKEL_");
		if (ClassName.Contains(TEXT("StaticMesh")))			return TEXT("SM_");
		if (ClassName.Contains(TEXT("Texture")))			return TEXT("T_");
		return FString();
	}

	/** Renames imported assets to follow the standard type prefixes (SM_, T_, M_, MI_, ...). */
	void ApplyTypePrefixes(FAssetToolsModule& AssetToolsModule, const TArray<UObject*>& Objects)
	{
		TArray<FAssetRenameData> Renames;
		for (UObject* Obj : Objects)
		{
			const FString Prefix = TypePrefixFor(Obj);
			if (Prefix.IsEmpty())
			{
				continue;
			}
			const FString CurrentName = Obj->GetName();
			if (CurrentName.StartsWith(Prefix))
			{
				continue;
			}
			const FString PackagePath = FPackageName::GetLongPackagePath(Obj->GetOutermost()->GetName());
			Renames.Add(FAssetRenameData(Obj, PackagePath, Prefix + CurrentName));
		}

		if (Renames.Num() > 0)
		{
			AssetToolsModule.Get().RenameAssets(Renames);
		}
	}

	void SaveImportedAssets(const TArray<UObject*>& Objects)
	{
		TArray<UPackage*> Packages;
		for (UObject* Obj : Objects)
		{
			if (Obj)
			{
				Packages.AddUnique(Obj->GetOutermost());
			}
		}
		if (Packages.Num() > 0)
		{
			UEditorLoadingAndSavingUtils::SavePackages(Packages, /*bOnlyDirty*/ false);
		}
	}
}

/** A sound already in use must not be overwritten by the next generation, so repeats get a _V2 name. */
static FString UnusedAssetName(const FString& PackageDir, const FString& BaseName)
{
	const FString Dir = PackageDir.EndsWith(TEXT("/")) ? PackageDir : PackageDir + TEXT("/");
	if (!FPackageName::DoesPackageExist(Dir + BaseName))
	{
		return BaseName;
	}

	for (int32 Version = 2; Version <= 999; ++Version)
	{
		const FString Candidate = FString::Printf(TEXT("%s_V%d"), *BaseName, Version);
		if (!FPackageName::DoesPackageExist(Dir + Candidate))
		{
			return Candidate;
		}
	}
	return FString::Printf(TEXT("%s_%s"), *BaseName, *FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8));
}

UTexture2D* FACFAIToolsImporter::ImportTexturePng(const TArray<uint8>& PngBytes, const FString& BaseName)
{
	if (PngBytes.Num() == 0)
	{
		return nullptr;
	}

	const FString TempFile = WriteTempFile(PngBytes, TEXT("png"));
	if (TempFile.IsEmpty())
	{
		return nullptr;
	}

	const FString DestPath = UACFAIToolsSettings::GetGeneratedContentFolder();
	const FString CleanName = AssetLeafName(BaseName);

	UAssetImportTask* Task = NewObject<UAssetImportTask>();
	Task->Filename = TempFile;
	// Use the asset name as the destination folder too, so each generation is tidy.
	Task->DestinationPath = DestPath / CleanName;
	Task->DestinationName = FString::Printf(TEXT("T_%s"), *CleanName);
	Task->bAutomated = true;
	Task->bSave = false;
	Task->bReplaceExisting = true;
	Task->Factory = NewObject<UTextureFactory>();

	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
	TArray<UAssetImportTask*> Tasks;
	Tasks.Add(Task);
	AssetToolsModule.Get().ImportAssetTasks(Tasks);

	IFileManager::Get().Delete(*TempFile);

	UTexture2D* Result = nullptr;
	for (UObject* Obj : Task->GetObjects())
	{
		if (UTexture2D* Tex = Cast<UTexture2D>(Obj))
		{
			Result = Tex;
			break;
		}
	}

	if (Result)
	{
		SaveImportedAssets({ Result });
	}
	return Result;
}

UObject* FACFAIToolsImporter::ImportMeshGlb(const TArray<uint8>& GlbBytes, const FString& BaseName, TArray<UObject*>& OutImported)
{
	OutImported.Reset();
	if (GlbBytes.Num() == 0)
	{
		return nullptr;
	}

	const UACFAIToolsSettings* Adv = UACFAIToolsSettings::Get();
	FString Extension = Adv ? Adv->MeshFormat : FString();
	if (Extension.IsEmpty())
	{
		Extension = TEXT("glb");
	}

	const FString TempFile = WriteTempFile(GlbBytes, Extension);
	if (TempFile.IsEmpty())
	{
		return nullptr;
	}

	const FString DestPath = UACFAIToolsSettings::GetGeneratedContentFolder();
	const FString CleanName = AssetLeafName(BaseName);

	UAssetImportTask* Task = NewObject<UAssetImportTask>();
	Task->Filename = TempFile;
	Task->DestinationPath = DestPath / CleanName;
	Task->DestinationName = CleanName;
	Task->bAutomated = true;
	Task->bSave = false;
	Task->bReplaceExisting = true;

	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
	TArray<UAssetImportTask*> Tasks;
	Tasks.Add(Task);
	AssetToolsModule.Get().ImportAssetTasks(Tasks);

	IFileManager::Get().Delete(*TempFile);

	UObject* Primary = nullptr;
	UObject* SkeletalPrimary = nullptr;
	for (UObject* Obj : Task->GetObjects())
	{
		if (!Obj)
		{
			continue;
		}
		OutImported.Add(Obj);
		const FString ClassName = Obj->GetClass()->GetName();
		if (!SkeletalPrimary && ClassName.Contains(TEXT("SkeletalMesh")))
		{
			SkeletalPrimary = Obj;
		}
		if (!Primary && ClassName.Contains(TEXT("StaticMesh")))
		{
			Primary = Obj;
		}
	}

	if (SkeletalPrimary)
	{
		Primary = SkeletalPrimary;
	}
	else if (!Primary && OutImported.Num() > 0)
	{
		Primary = OutImported[0];
	}

	if (OutImported.Num() > 0)
	{
		ApplyTypePrefixes(AssetToolsModule, OutImported);
		SaveImportedAssets(OutImported);
	}

	return Primary;
}

USkeletalMesh* FACFAIToolsImporter::ImportRiggedFbx(const TArray<uint8>& FbxBytes, const FString& BaseName, TArray<UObject*>& OutImported, FString& OutError)
{
	OutImported.Reset();
	OutError.Reset();

	if (FbxBytes.Num() == 0)
	{
		OutError = TEXT("Rigged FBX bytes are empty.");
		return nullptr;
	}

	const FString TempFile = WriteTempFile(FbxBytes, TEXT("fbx"));
	if (TempFile.IsEmpty())
	{
		OutError = TEXT("Failed to write temporary FBX file.");
		return nullptr;
	}

	const FString DestPath = UACFAIToolsSettings::GetGeneratedContentFolder();
	const FString CleanName = AssetLeafName(BaseName);

	// Keep Meshy's own skeleton (Mixamo-style). Do not remap onto ACF/UE5 — retarget in editor.
	UFbxFactory* Factory = NewObject<UFbxFactory>();
	Factory->ImportUI = NewObject<UFbxImportUI>(Factory);
	Factory->ImportUI->bImportAsSkeletal = true;
	Factory->ImportUI->bImportMesh = true;
	Factory->ImportUI->bImportAnimations = false;
	Factory->ImportUI->bImportMaterials = true;
	Factory->ImportUI->bImportTextures = true;
	Factory->ImportUI->MeshTypeToImport = FBXIT_SkeletalMesh;
	Factory->ImportUI->Skeleton = nullptr;
	if (!Factory->ImportUI->SkeletalMeshImportData)
	{
		Factory->ImportUI->SkeletalMeshImportData = NewObject<UFbxSkeletalMeshImportData>(Factory->ImportUI);
	}
	Factory->ImportUI->SkeletalMeshImportData->bImportMorphTargets = false;

	UAssetImportTask* Task = NewObject<UAssetImportTask>();
	Task->Filename = TempFile;
	Task->DestinationPath = DestPath / CleanName;
	Task->DestinationName = CleanName;
	Task->bAutomated = true;
	Task->bSave = false;
	Task->bReplaceExisting = true;
	Task->Factory = Factory;

	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
	TArray<UAssetImportTask*> Tasks;
	Tasks.Add(Task);
	AssetToolsModule.Get().ImportAssetTasks(Tasks);

	IFileManager::Get().Delete(*TempFile);

	USkeletalMesh* SkelMesh = nullptr;
	for (UObject* Obj : Task->GetObjects())
	{
		if (!Obj)
		{
			continue;
		}
		OutImported.Add(Obj);
		if (!SkelMesh)
		{
			SkelMesh = Cast<USkeletalMesh>(Obj);
		}
	}

	if (!SkelMesh)
	{
		OutError = TEXT("FBX import did not produce a Skeletal Mesh.");
		return nullptr;
	}

	ApplyTypePrefixes(AssetToolsModule, OutImported);
	SkelMesh = nullptr;
	for (UObject* Obj : OutImported)
	{
		if (USkeletalMesh* Candidate = Cast<USkeletalMesh>(Obj))
		{
			SkelMesh = Candidate;
			break;
		}
	}
	if (!SkelMesh)
	{
		OutError = TEXT("Skeletal Mesh was lost after renaming imported assets.");
		return nullptr;
	}

	SaveImportedAssets(OutImported);
	return SkelMesh;
}

USoundWave* FACFAIToolsImporter::ImportSound(const TArray<uint8>& AudioBytes, const FString& BaseName, const FString& Extension, bool bLooping)
{
	if (AudioBytes.Num() == 0)
	{
		return nullptr;
	}

	const FString TempFile = WriteTempFile(AudioBytes, Extension.IsEmpty() ? FString(TEXT("mp3")) : Extension);
	if (TempFile.IsEmpty())
	{
		return nullptr;
	}

	const FString DestPath = UACFAIToolsSettings::GetGeneratedAudioFolder();
	const FString AssetName = UnusedAssetName(DestPath, FString::Printf(TEXT("A_%s"), *AssetLeafName(BaseName)));

	USoundFactory* Factory = NewObject<USoundFactory>();
	Factory->SuppressImportDialogs();
	Factory->bAutoCreateCue = false;

	UAssetImportTask* Task = NewObject<UAssetImportTask>();
	Task->Filename = TempFile;
	// Every generated sound shares one folder: it is a library to browse, not one folder per generation.
	Task->DestinationPath = DestPath;
	Task->DestinationName = AssetName;
	Task->bAutomated = true;
	Task->bSave = false;
	Task->bReplaceExisting = true;
	Task->Factory = Factory;

	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
	TArray<UAssetImportTask*> Tasks;
	Tasks.Add(Task);
	AssetToolsModule.Get().ImportAssetTasks(Tasks);

	IFileManager::Get().Delete(*TempFile);

	USoundWave* Result = nullptr;
	for (UObject* Obj : Task->GetObjects())
	{
		Result = Cast<USoundWave>(Obj);
		if (Result)
		{
			break;
		}
	}

	if (!Result)
	{
		return nullptr;
	}

	if (bLooping)
	{
		Result->bLooping = true;
		Result->MarkPackageDirty();
	}
	SaveImportedAssets({ Result });
	return Result;
}
