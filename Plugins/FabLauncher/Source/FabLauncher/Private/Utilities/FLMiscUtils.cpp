// Copyright Epic Games, Inc. All Rights Reserved.
#include "Utilities/FLMiscUtils.h"
#include "IFabLauncherModule.h"

#include "Runtime/Core/Public/Misc/Paths.h"
#include "Runtime/Core/Public/HAL/PlatformFilemanager.h"
#include "Runtime/Core/Public/GenericPlatform/GenericPlatformFile.h"
#include "Runtime/Core/Public/HAL/FileManager.h"

#include "UObject/SoftObjectPath.h"
#include "EditorAssetLibrary.h"

#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "Runtime/Engine/Classes/Engine/Selection.h"

#include "Editor/UnrealEd/Classes/Factories/MaterialInstanceConstantFactoryNew.h"

#include "Runtime/Engine/Classes/Engine/StaticMesh.h"

#include "Runtime/Core/Public/Misc/MessageDialog.h"
#include "Runtime/Core/Public/Internationalization/Text.h"

#include "PackageTools.h"

#include <regex>
#include <ostream>
#include <sstream>

#include "Misc/ScopedSlowTask.h"
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "Misc/FileHelper.h"
#include "Kismet/GameplayStatics.h"

TSharedPtr<FJsonObject> DeserializeJson(const FString& JsonStringData)
{
	TSharedPtr<FJsonObject> JsonDataObject;
	bool bIsDeseriliazeSuccessful = false;
	TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(JsonStringData);
	bIsDeseriliazeSuccessful = FJsonSerializer::Deserialize(JsonReader, JsonDataObject);
	return JsonDataObject;
}

FString GetFLPresetsName()
{
	return TEXT("MSPresets");
}

FString GetSourceFLPresetsPath()
{
	// TODO: hack depending on toolchain versions, workarounds might be needed to install plugins in Marketplace
	// Try to handle this case as fallback, and display a message if such is the case
	// See https://forums.unrealengine.com/t/ue5-1-expecting-to-find-a-type-to-be-declared-in-a-module-rules-named/741811
	FString MarketplacePresetsPath = FPaths::Combine(FPaths::EnginePluginsDir(), TEXT("Marketplace"), TEXT("FabLauncher"), TEXT("Content"), GetFLPresetsName());
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (PlatformFile.DirectoryExists(*MarketplacePresetsPath))
	{
		return MarketplacePresetsPath;
	}
	// Return typical path if plugin is not in marketplace. If it's not valid, errors will be caught later
	return FPaths::Combine(FPaths::EnginePluginsDir(), TEXT("FabLauncher"), TEXT("Content"), GetFLPresetsName());
}

FString GetMaterial(const FString& MaterialName)
{
	FString MaterialPath = FPaths::Combine(TEXT("/Game"), GetFLPresetsName(), MaterialName, MaterialName);

	if (!UEditorAssetLibrary::DoesAssetExist(MaterialPath))
	{
		if (CopyMaterialPreset(MaterialName))
		{
			if (UEditorAssetLibrary::DoesAssetExist(MaterialPath))
			{
				return MaterialPath;
			}
			else {
				UE_LOG(LogFabLauncher, Error, TEXT("Failed copying a Parent material to %s"), *MaterialPath);
				return TEXT("");
			}
		}
		else {
			UE_LOG(LogFabLauncher, Error, TEXT("Failed copying a Parent material to %s"), *MaterialPath);
			return TEXT("");
		}
	}
	else {
		return MaterialPath;
	}
}

bool CopyMaterialPreset(const FString& MaterialName)
{
	FString MaterialSourceFolderPath = FPaths::Combine(GetSourceFLPresetsPath(), MaterialName);
	MaterialSourceFolderPath = FPaths::ConvertRelativePathToFull(MaterialSourceFolderPath);
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.DirectoryExists(*MaterialSourceFolderPath))
	{
		UE_LOG(LogFabLauncher, Error, TEXT("%s does not exist"), *MaterialSourceFolderPath);
		return false;
	}

	FString MaterialDestinationPath = FPaths::ProjectContentDir();
	MaterialDestinationPath = FPaths::Combine(MaterialDestinationPath, GetFLPresetsName());
	MaterialDestinationPath = FPaths::Combine(MaterialDestinationPath, MaterialName);

	if (!PlatformFile.DirectoryExists(*MaterialDestinationPath))
	{
		if (!PlatformFile.CreateDirectoryTree(*MaterialDestinationPath))
		{
			UE_LOG(LogFabLauncher, Error, TEXT("Could not create directory %s"), *MaterialDestinationPath);
			return false;
		}
		if (!PlatformFile.CopyDirectoryTree(*MaterialDestinationPath, *MaterialSourceFolderPath, true))
		{
			UE_LOG(LogFabLauncher, Error, TEXT("Could not copy directory from %s to %s"), *MaterialSourceFolderPath, *MaterialDestinationPath)
			return false;
		}

	}


	FAssetRegistryModule& AssetRegistryModule = FModuleManager::GetModuleChecked<FAssetRegistryModule>("AssetRegistry");
	TArray<FString> MaterialBasePath;
	FString BasePath = FPaths::Combine(FPaths::ProjectContentDir(), GetFLPresetsName());
	MaterialBasePath.Add(BasePath);
	MaterialBasePath.Add("/Game/MSPresets");
	MaterialBasePath.Add(FPaths::Combine(TEXT("/Game/MSPresets"), MaterialName));
	if (PlatformFile.DirectoryExists(*FPaths::Combine(TEXT("/Game/MSPresets"), MaterialName, TEXT("Functions"))))
	{
		MaterialBasePath.Add(FPaths::Combine(TEXT("/Game/MSPresets"), MaterialName, TEXT("Functions")));
	}
	AssetRegistryModule.Get().ScanPathsSynchronous(MaterialBasePath, true);
	return true;

}

bool CopyPresetTextures()
{
	FString TexturesDestinationPath = FPaths::ProjectContentDir();

	TexturesDestinationPath = FPaths::Combine(TexturesDestinationPath, GetFLPresetsName(), TEXT("MSTextures"));

	FString TexturesSourceFolderPath = FPaths::Combine(GetSourceFLPresetsPath(), TEXT("MSTextures"));
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.DirectoryExists(*TexturesDestinationPath))
	{
		if (!PlatformFile.CreateDirectoryTree(*TexturesDestinationPath))
		{

			return false;
		}

		if (!PlatformFile.CopyDirectoryTree(*TexturesDestinationPath, *TexturesSourceFolderPath, true))
		{

			return false;
		}
	}

	return true;
}

// Added with Opacity/Emission release after updating the list of textures in FLTextures
bool CopyUpdatedPresetTextures()
{
	FString TexturesDestinationPath = FPaths::ProjectContentDir();

	TexturesDestinationPath = FPaths::Combine(TexturesDestinationPath, GetFLPresetsName(), TEXT("MS_DefaultTextures"));

	FString TexturesSourceFolderPath = FPaths::Combine(GetSourceFLPresetsPath(), TEXT("MS_DefaultTextures"));
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.DirectoryExists(*TexturesDestinationPath))
	{
		if (!PlatformFile.CreateDirectoryTree(*TexturesDestinationPath))
		{

			return false;
		}

		if (!PlatformFile.CopyDirectoryTree(*TexturesDestinationPath, *TexturesSourceFolderPath, true))
		{

			return false;
		}
	}

	return true;
}

UObject* LoadAsset(const FString& AssetPath)
{
	return UEditorAssetLibrary::LoadAsset(AssetPath);
}

TArray<FString> GetAssetsList(const FString& DirectoryPath)
{
	return UEditorAssetLibrary::ListAssets(DirectoryPath, false);

}
void SaveAsset(const FString& AssetPath)
{
	UEditorAssetLibrary::SaveAsset(AssetPath);
}


FString NormalizeString(FString InputString)
{
	const std::string SInputString = std::string(TCHAR_TO_UTF8(*InputString));
	const std::regex SpecialCharacters("[^a-zA-Z0-9-]+");
	std::stringstream Result;
	std::regex_replace(std::ostream_iterator<char>(Result), SInputString.begin(), SInputString.end(), SpecialCharacters, "_");
	FString NormalizedString = FString(Result.str().c_str());

	return NormalizedString;
}

FString RemoveReservedKeywords(const FString& Name)
{
	FString ResolvedName = Name;
	TArray<FString> ReservedKeywrods = { "CON","PRN", "AUX", "CLOCK$", "NUL", "NONE", "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8", "COM9", "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9" };
	TArray<FString> NameArray;
	Name.ParseIntoArray(NameArray, TEXT("_"));
	for (FString NameTag : NameArray)
	{
		if (ReservedKeywrods.Contains(NameTag))
		{
			ResolvedName = ResolvedName.Replace(*NameTag, TEXT(""));
		}
	}
	return ResolvedName;
}

FString GetRootDestination(const FString& ExportPath)
{
	TArray<FString> PathTokens;
	FString BasePath;
	FString DestinationPath;
	ExportPath.Split(TEXT("Content"), &BasePath, &DestinationPath);
	FString RootDestination = TEXT("/Game/");

#if PLATFORM_WINDOWS
	DestinationPath.ParseIntoArray(PathTokens, TEXT("\\"));
#elif PLATFORM_MAC
	DestinationPath.ParseIntoArray(PathTokens, TEXT("/"));
#elif PLATFORM_LINUX
	DestinationPath.ParseIntoArray(PathTokens, TEXT("/"));
#endif	

	for (FString Token : PathTokens)
	{
		FString NormalizedToken = RemoveReservedKeywords(NormalizeString(Token));
		RootDestination = FPaths::Combine(RootDestination, NormalizedToken);
	}
	if (RootDestination == TEXT("/Game/"))
	{
		RootDestination = FPaths::Combine(RootDestination, TEXT("FabLauncher"));
	}
	return RootDestination;
}




FString ResolveDestination(const FString& AssetDestination)
{
	return TEXT("Resolved destination path");
}

FString GetAssetName(TSharedPtr<FAssetTypeData> AssetImportData)
{
	return FString();
}

FString GetUniqueAssetName(const FString& AssetDestination, const FString AssetName, bool FileSearch)
{

	FString ResolvedAssetName = RemoveReservedKeywords(NormalizeString(AssetName));
	TArray<FString> AssetDirectories;
	auto& FileManager = IFileManager::Get();
	FileManager.FindFiles(AssetDirectories, *AssetDestination, FileSearch, !FileSearch);

	if (!UEditorAssetLibrary::DoesDirectoryExist(FPaths::Combine(AssetDestination, ResolvedAssetName)))
	{
		return ResolvedAssetName;
	}

	for (int i = 0; i < 200; i++)
	{
		FString ResolvedAssetDirectory = TEXT("");
		if (i < 10) {
			ResolvedAssetDirectory = ResolvedAssetName + TEXT("_0") + FString::FromInt(i);
		}
		else
		{
			ResolvedAssetDirectory = ResolvedAssetName + TEXT("_") + FString::FromInt(i);
		}

		if (!UEditorAssetLibrary::DoesDirectoryExist(FPaths::Combine(AssetDestination, ResolvedAssetDirectory)))
		{

			return ResolvedAssetDirectory;
		}

	}

	return TEXT("");
}

FString SanitizeName(const FString& InputName)
{
	return RemoveReservedKeywords(NormalizeString(InputName));
}




//template<typename T>
// TArray<UMaterialInstanceConstant*> AssetUtils::GetSelectedAssets(const FString& AssetClass)
// {
// 	TArray<UMaterialInstanceConstant*> ObjectArray;

// 	TArray<FAssetData> AssetDatas;
// 	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
// 	IContentBrowserSingleton& ContentBrowserSingleton = ContentBrowserModule.Get();
// 	ContentBrowserSingleton.GetSelectedAssets(AssetDatas);

// 	for (FAssetData SelectedAsset : AssetDatas)
// 	{
// 		// In recent versions of UE, AssetClass can be "None", the correct name being in AssetClassPath.AssetName
// 		#if (ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION > 0)
// 		FName AssetClassName = SelectedAsset.AssetClassPath.GetAssetName();
// 		#else
// 		FName AssetClassName = SelectedAsset.AssetClass;
// 		#endif
// 		if (AssetClassName == FName(*AssetClass))
// 		{
// 			ObjectArray.Add(CastChecked<UMaterialInstanceConstant>(UEditorAssetLibrary::LoadAsset(SelectedAsset.GetObjectPathString())));
// 		}
// 	}
// 	return ObjectArray;
// }

void AssetUtils::FocusOnSelected(const FString& Path)
{
	TArray<FString> Folders;
	Folders.Add(Path);
	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
	IContentBrowserSingleton& ContentBrowserSingleton = ContentBrowserModule.Get();
	ContentBrowserSingleton.SyncBrowserToFolders(Folders);
}


void AssetUtils::AddStaticMaterial(UStaticMesh* SourceMesh, UMaterialInstanceConstant* NewMaterial)
{
	if (NewMaterial == nullptr) return;

	UMaterialInterface* NewMatInterface = CastChecked<UMaterialInterface>(NewMaterial);
	FStaticMaterial MaterialSlot = FStaticMaterial(NewMatInterface);
	SourceMesh->GetStaticMaterials().Add(MaterialSlot);
}

void AssetUtils::SavePackage(UObject* SourceObject)
{
	TArray<UObject*> InputObjects;
	InputObjects.Add(SourceObject);
	UPackageTools::SavePackagesForObjects(InputObjects);

}

void AssetUtils::DeleteDirectory(FString TargetDirectory)
{

	TArray<FString> PreviewAssets = UEditorAssetLibrary::ListAssets(TargetDirectory);

	for (FString AssetPath : PreviewAssets)
	{
		DeleteAsset(AssetPath);
	}

	UEditorAssetLibrary::DeleteDirectory(TargetDirectory);


}

bool AssetUtils::DeleteAsset(const FString& AssetPath)
{
	IAssetRegistry& AssetRegistry = FModuleManager::GetModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	
	#if (ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 1)
	FAssetData AssetData = AssetRegistry.GetAssetByObjectPath(FSoftObjectPath(*AssetPath));
	#else
	FAssetData AssetData = AssetRegistry.GetAssetByObjectPath(FName(*AssetPath));
	#endif

	if (AssetData.IsAssetLoaded())
	{
		if (UEditorAssetLibrary::DeleteLoadedAsset(AssetData.GetAsset()))
		{
			return true;
		}
	}
	else
	{
		if (UEditorAssetLibrary::DeleteAsset(AssetPath))
		{
			return true;
		}
	}

	return false;
}
