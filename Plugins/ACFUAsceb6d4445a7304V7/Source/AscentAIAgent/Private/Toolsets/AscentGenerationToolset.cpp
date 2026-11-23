// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "Toolsets/AscentGenerationToolset.h"

#include "ACFKimodoSubsystem.h"
#include "Animation/AnimSequence.h"
#include "ThreeDBackend.h"
#include "AscentAIToolsSettings.h"
#include "AscentAIToolsSubsystem.h"
#include "AssetToolsModule.h"
#include "AutomatedAssetImportData.h"
#include "Editor.h"
#include "Engine/SkeletalMesh.h"
#include "Sound/SoundWave.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
	FString EncodeResult(const TSharedRef<FJsonObject>& Json)
	{
		FString Text;
		FJsonSerializer::Serialize(Json, TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Text));
		return Text;
	}

	UACFAIToolsSubsystem* GetGenerationSubsystem()
	{
		return GEditor ? GEditor->GetEditorSubsystem<UACFAIToolsSubsystem>() : nullptr;
	}

	// Stops generation before any work or spend happens when the required provider key is missing. Returns the user-facing error, or empty when the key is present.
	FString MissingImageKeyError()
	{
		if (UACFAIToolsSettings::Get()->ImageApiKey.TrimStartAndEnd().IsEmpty())
		{
			return TEXT("The image API key is missing. On Codex, generate the image with the image-generation skill and call ImportImageFile. ")
				TEXT("Otherwise ask the user to set the Image API key in Yarin Settings and retry.");
		}
		return FString();
	}

	// The sound effects key is the ElevenLabs one, shared with dialogue voice generation.
	FString MissingSfxKeyError()
	{
		if (UACFAIToolsSettings::GetSfxApiKey().IsEmpty())
		{
			return TEXT("The ElevenLabs API key is missing. Ask the user to set the Voice API Key in Yarin settings > Voice ")
				TEXT("(sound effects share the ElevenLabs key), or a separate Sound Effects API Key in Project Settings > ")
				TEXT("Ascent AI Tools, then retry.");
		}
		return FString();
	}

	FString MissingMeshKeyError()
	{
		FString ResolveError;
		ACF3D::ResolveEndpoints(ACF3D::GetActiveBackend().GetProvider(), &ResolveError);
		if (!ResolveError.IsEmpty())
		{
			return FString::Printf(TEXT("%s Ask the user to fix it, then retry."), *ResolveError);
		}
		return FString();
	}

	// Blocks a tool that the selected 3D provider cannot serve by capability. Names the provider and points at the setting to change.
	UToolCallAsyncResultString* UnsupportedCapabilityError(const FString& SourceKind)
	{
		UToolCallAsyncResultString* Result = NewObject<UToolCallAsyncResultString>();
		Result->SetError(FString::Printf(TEXT("%s cannot generate a mesh from %s. Ask the user to switch the 3D provider to one that supports it in the Yarin gear settings, then retry."), *ACF3D::GetActiveBackend().GetDisplayName().ToString(), *SourceKind));
		return Result;
	}

	// Saves the last generated PNG next to the project so the chat can preview it and re-send it to the model.
	FString SaveGeneratedPng(const UACFAIToolsSubsystem& Subsystem, const FString& BaseName)
	{
		if (Subsystem.GetLastImagePng().IsEmpty()) { return FString(); }
		const FString Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("ACFAssistant/Generated"));
		IFileManager::Get().MakeDirectory(*Directory, true);
		const FString File = Directory / FString::Printf(TEXT("%s_%s.png"), *BaseName, *FDateTime::Now().ToString(TEXT("%H%M%S")));
		return FFileHelper::SaveArrayToFile(Subsystem.GetLastImagePng(), *File) ? File : FString();
	}

	// Unbind destroys the bound lambda, including any TStrongObjectPtr it captured. Take the result by value so SetError/SetValue still have a live pointer.
	template<typename DelegateType>
	TStrongObjectPtr<UToolCallAsyncResultString> TakePinnedResultAndUnbind(TStrongObjectPtr<UToolCallAsyncResultString> LiveResult, FDelegateHandle Handle, DelegateType& Delegate)
	{
		Delegate.Remove(Handle);
		return LiveResult;
	}
}

UToolCallAsyncResultString* UAscentGenerationToolset::GenerateImage(const FString& Prompt, const FString& AssetName)
{
	UToolCallAsyncResultString* Result = NewObject<UToolCallAsyncResultString>();
	UACFAIToolsSubsystem* Subsystem = GetGenerationSubsystem();
	if (!Subsystem) { Result->SetError(TEXT("The AI Tools subsystem is unavailable.")); return Result; }
	if (const FString KeyError = MissingImageKeyError(); !KeyError.IsEmpty()) { Result->SetError(KeyError); return Result; }
	if (Subsystem->IsImageBusy()) { Result->SetError(TEXT("An image generation is already running. Wait for it to finish, then retry.")); return Result; }
	const TSharedRef<FDelegateHandle> Handle = MakeShared<FDelegateHandle>();
	TStrongObjectPtr<UToolCallAsyncResultString> Pinned(Result);
	*Handle = Subsystem->OnImageGeneratedNative.AddLambda([Pinned, Handle, Subsystem](UTexture2D* Texture, bool bSuccess, const FString& ErrorMessage)
	{
		UACFAIToolsSubsystem* LiveSubsystem = Subsystem;
		TStrongObjectPtr<UToolCallAsyncResultString> LiveResult = TakePinnedResultAndUnbind(Pinned, *Handle, LiveSubsystem->OnImageGeneratedNative);
		if (!LiveResult)
		{
			return;
		}
		if (!bSuccess || !Texture)
		{
			LiveResult->SetError(ErrorMessage.IsEmpty() ? TEXT("Image generation failed.") : ErrorMessage);
			return;
		}
		const TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetBoolField(TEXT("success"), true);
		Json->SetStringField(TEXT("asset_path"), Texture->GetPathName());
		const FString ImageFile = SaveGeneratedPng(*LiveSubsystem, Texture->GetName());
		if (!ImageFile.IsEmpty()) { Json->SetStringField(TEXT("image_file"), ImageFile); }
		Json->SetStringField(TEXT("note"), TEXT("The image was imported as a Texture asset. You can now call GenerateMeshFromImage to turn it into a 3D mesh."));
		LiveResult->SetValue(EncodeResult(Json));
	});
	Subsystem->GenerateImage(Prompt, AssetName);
	return Result;
}

UToolCallAsyncResultString* UAscentGenerationToolset::GenerateSoundEffect(const FString& Prompt, const FString& AssetName, float DurationSeconds, bool bLoop)
{
	UToolCallAsyncResultString* Result = NewObject<UToolCallAsyncResultString>();
	UACFAIToolsSubsystem* Subsystem = GetGenerationSubsystem();
	if (!Subsystem) { Result->SetError(TEXT("The AI Tools subsystem is unavailable.")); return Result; }
	if (const FString KeyError = MissingSfxKeyError(); !KeyError.IsEmpty()) { Result->SetError(KeyError); return Result; }
	if (Subsystem->IsSoundBusy()) { Result->SetError(TEXT("A sound effect generation is already running. Wait for it to finish, then retry.")); return Result; }
	const TSharedRef<FDelegateHandle> Handle = MakeShared<FDelegateHandle>();
	TStrongObjectPtr<UToolCallAsyncResultString> Pinned(Result);
	*Handle = Subsystem->OnSoundEffectGeneratedNative.AddLambda([Pinned, Handle, Subsystem, DurationSeconds, bLoop](USoundWave* Sound, bool bSuccess, const FString& ErrorMessage)
	{
		UACFAIToolsSubsystem* LiveSubsystem = Subsystem;
		TStrongObjectPtr<UToolCallAsyncResultString> LiveResult = TakePinnedResultAndUnbind(Pinned, *Handle, LiveSubsystem->OnSoundEffectGeneratedNative);
		if (!LiveResult)
		{
			return;
		}
		if (!bSuccess || !Sound)
		{
			LiveResult->SetError(ErrorMessage.IsEmpty() ? TEXT("Sound effect generation failed.") : ErrorMessage);
			return;
		}
		const TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetBoolField(TEXT("success"), true);
		Json->SetStringField(TEXT("asset_path"), Sound->GetPathName());
		Json->SetStringField(TEXT("model"), UACFAIToolsSettings::GetSfxModel());
		Json->SetNumberField(TEXT("duration_seconds"), DurationSeconds > 0.0f ? FMath::Clamp(DurationSeconds, 0.5f, 30.0f) : Sound->Duration);
		Json->SetBoolField(TEXT("loop"), bLoop);
		LiveResult->SetValue(EncodeResult(Json));
	});
	Subsystem->GenerateSoundEffect(Prompt, AssetName, DurationSeconds, bLoop);
	return Result;
}

namespace
{
	UToolCallAsyncResultString* StartMeshGeneration(const FString& AssetName, TFunction<void(UACFAIToolsSubsystem&, const FString&)> Launch)
	{
		UToolCallAsyncResultString* Result = NewObject<UToolCallAsyncResultString>();
		UACFAIToolsSubsystem* Subsystem = GetGenerationSubsystem();
		if (!Subsystem) { Result->SetError(TEXT("The AI Tools subsystem is unavailable.")); return Result; }
		if (const FString KeyError = MissingMeshKeyError(); !KeyError.IsEmpty()) { Result->SetError(KeyError); return Result; }
		if (Subsystem->IsMeshBusy()) { Result->SetError(TEXT("A 3D generation is already running. Wait for it to finish, then retry.")); return Result; }
		const TSharedRef<FDelegateHandle> Handle = MakeShared<FDelegateHandle>();
		TStrongObjectPtr<UToolCallAsyncResultString> Pinned(Result);
		*Handle = Subsystem->OnMeshGeneratedNative.AddLambda([Pinned, Handle, Subsystem](UObject* Mesh, bool bSuccess, const FString& ErrorMessage)
		{
			TStrongObjectPtr<UToolCallAsyncResultString> LiveResult = TakePinnedResultAndUnbind(Pinned, *Handle, Subsystem->OnMeshGeneratedNative);
			if (!LiveResult)
			{
				return;
			}
			if (!bSuccess || !Mesh)
			{
				LiveResult->SetError(ErrorMessage.IsEmpty() ? TEXT("3D generation failed.") : ErrorMessage);
				return;
			}
			const TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
			Json->SetBoolField(TEXT("success"), true);
			Json->SetStringField(TEXT("asset_path"), Mesh->GetPathName());
			LiveResult->SetValue(EncodeResult(Json));
		});
		Launch(*Subsystem, AssetName);
		return Result;
	}
}

UToolCallAsyncResultString* UAscentGenerationToolset::GenerateMeshFromText(const FString& Prompt, const FString& AssetName)
{
	if (!ACF3D::GetActiveBackend().GetCapabilities().bTextTo3D)
	{
		return UnsupportedCapabilityError(TEXT("text"));
	}
	return StartMeshGeneration(AssetName, [Prompt](UACFAIToolsSubsystem& Subsystem, const FString& Name) { Subsystem.GenerateMeshFromText(Prompt, Name); });
}

FString UAscentGenerationToolset::ImportImageFile(const FString& FilePath, const FString& DestinationPath)
{
	auto MakeError = [](const FString& Message)
	{
		const TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetBoolField(TEXT("success"), false);
		Json->SetStringField(TEXT("error"), Message);
		return EncodeResult(Json);
	};
	const FString FullPath = FPaths::ConvertRelativePathToFull(FilePath.TrimStartAndEnd().TrimQuotes());
	const FString Extension = FPaths::GetExtension(FullPath).ToLower();
	if (!FPaths::FileExists(FullPath)) { return MakeError(TEXT("The image file does not exist: ") + FullPath); }
	if (Extension != TEXT("png") && Extension != TEXT("jpg") && Extension != TEXT("jpeg")) { return MakeError(TEXT("Only .png and .jpg images can be imported.")); }
	FString Destination = DestinationPath.TrimStartAndEnd();
	const FString ConfiguredFolder = UACFAIToolsSettings::GetGeneratedContentFolder();
	if (Destination.IsEmpty() || Destination.StartsWith(TEXT("/Game/AIGenerated")))
	{
		Destination = ConfiguredFolder;
	}
	UAutomatedAssetImportData* ImportData = NewObject<UAutomatedAssetImportData>();
	ImportData->Filenames = {FullPath};
	ImportData->DestinationPath = Destination;
	ImportData->bReplaceExisting = true;
	const TArray<UObject*> Imported = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get().ImportAssetsAutomated(ImportData);
	if (Imported.IsEmpty() || !Imported[0]) { return MakeError(TEXT("The image could not be imported. Check the file and the destination path.")); }
	const TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetBoolField(TEXT("success"), true);
	Json->SetStringField(TEXT("asset_path"), Imported[0]->GetPathName());
	Json->SetStringField(TEXT("image_file"), FullPath);
	return EncodeResult(Json);
}

UToolCallAsyncResultString* UAscentGenerationToolset::GenerateMeshFromImage(const FString& FilePath, const FString& AssetName)
{
	if (!ACF3D::GetActiveBackend().GetCapabilities().bImageTo3D)
	{
		return UnsupportedCapabilityError(TEXT("an image"));
	}
	UACFAIToolsSubsystem* Subsystem = GetGenerationSubsystem();
	const FString CleanPath = FilePath.TrimStartAndEnd().TrimQuotes();
	if (Subsystem && !CleanPath.IsEmpty() && !Subsystem->LoadPendingImageFromFile(CleanPath))
	{
		UToolCallAsyncResultString* Result = NewObject<UToolCallAsyncResultString>();
		Result->SetError(TEXT("The source image file could not be read: ") + CleanPath);
		return Result;
	}
	if (Subsystem && CleanPath.IsEmpty() && !Subsystem->HasGeneratedImage())
	{
		UToolCallAsyncResultString* Result = NewObject<UToolCallAsyncResultString>();
		Result->SetError(TEXT("No source image exists. Pass a PNG file path, or generate an image first."));
		return Result;
	}
	return StartMeshGeneration(AssetName, [](UACFAIToolsSubsystem& InSubsystem, const FString& Name) { InSubsystem.GenerateMeshFromImage(Name); });
}

UToolCallAsyncResultString* UAscentGenerationToolset::GenerateMotionFromText(const FString& Prompt, float DurationSeconds, const FString& AssetName, const FString& TargetMesh)
{
	UToolCallAsyncResultString* Result = NewObject<UToolCallAsyncResultString>();
	UACFKimodoSubsystem* Kimodo = GEditor ? GEditor->GetEditorSubsystem<UACFKimodoSubsystem>() : nullptr;
	if (!Kimodo)
	{
		Result->SetError(TEXT("Kimodo is unavailable in this editor."));
		return Result;
	}
	TStrongObjectPtr<UToolCallAsyncResultString> Pinned(Result);
	Kimodo->GenerateMotion(Prompt, DurationSeconds, AssetName, TargetMesh, [Pinned](const FACFMotionImportResult& Motion, const FString& Error, const FString& BvhFile)
	{
		if (!Pinned)
		{
			return;
		}
		if (!Motion.bSuccess || !Motion.Animation || !Motion.TargetMesh)
		{
			FString Message = Error.IsEmpty() ? TEXT("Kimodo could not generate the motion.") : Error;
			if (!BvhFile.IsEmpty())
			{
				Message += FString::Printf(TEXT(" The motion was generated at %s; ImportMotion can place it on another mesh."), *BvhFile);
			}
			Pinned->SetError(Message);
			return;
		}
		const TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetBoolField(TEXT("success"), true);
		Json->SetStringField(TEXT("asset_path"), Motion.Animation->GetPathName());
		Json->SetStringField(TEXT("mesh"), Motion.TargetMesh->GetName());
		Json->SetStringField(TEXT("mesh_path"), Motion.TargetMesh->GetPathName());
		Json->SetBoolField(TEXT("fallback"), Motion.bUsedFallbackMesh);
		if (Motion.SourceAnimation)
		{
			Json->SetStringField(TEXT("source_path"), Motion.SourceAnimation->GetPathName());
		}
		Json->SetNumberField(TEXT("frames"), Motion.Frames);
		Json->SetNumberField(TEXT("frame_rate"), Motion.FrameRate);
		Json->SetStringField(TEXT("bvh_file"), BvhFile);
		Pinned->SetValue(EncodeResult(Json));
	});
	return Result;
}
