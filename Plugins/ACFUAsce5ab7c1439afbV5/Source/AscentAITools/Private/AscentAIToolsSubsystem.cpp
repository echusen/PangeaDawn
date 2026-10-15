// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.

#include "AscentAIToolsSubsystem.h"
#include "AscentAIToolsSettings.h"
#include "AIToolsNotification.h"
#include "AIToolsImporter.h"
#include "NanoBananaClient.h"
#include "GptImageClient.h"
#include "ThreeDBackend.h"
#include "Engine/Texture2D.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/Skeleton.h"
#include "Misc/FileHelper.h"

#define LOCTEXT_NAMESPACE "AscentAIToolsSubsystem"

UACFAIToolsSettings* UACFAIToolsSubsystem::GetAIToolsSettings()
{
	return GetMutableDefault<UACFAIToolsSettings>();
}

FString UACFAIToolsSubsystem::ResolveBaseName(const FString& AssetName, const FString& Prompt, const FString& Fallback)
{
	if (!AssetName.IsEmpty())
	{
		return AssetName;
	}
	if (!Prompt.IsEmpty())
	{
		return Prompt.Left(40);
	}
	return Fallback;
}

void UACFAIToolsSubsystem::BroadcastStatus(const FText& Status)
{
	LastStatus = Status;
	OnStatusChangedNative.Broadcast(Status);
	OnStatusChanged.Broadcast(Status.ToString());
}

void UACFAIToolsSubsystem::GenerateImage(const FString& Prompt, const FString& AssetName)
{
	if (Prompt.IsEmpty())
	{
		BroadcastStatus(LOCTEXT("ErrNoPrompt", "Please enter a prompt before generating."));
		OnImageGeneratedNative.Broadcast(nullptr, false, TEXT("Please enter a prompt before generating."));
		OnImageGenerated.Broadcast(nullptr, false, TEXT("Please enter a prompt before generating."));
		return;
	}

	const UACFAIToolsSettings* Settings = UACFAIToolsSettings::Get();
	const bool bUseGpt = Settings && Settings->ImageProvider == EACFImageProvider::GptImage;
	const FString ImageKey = Settings ? Settings->ImageApiKey : FString();
	if (ImageKey.IsEmpty())
	{
		const FString Msg = TEXT("The image API key for the selected provider is not set. Open Settings and add it.");
		BroadcastStatus(FText::FromString(Msg));
		OnImageGeneratedNative.Broadcast(nullptr, false, Msg);
		OnImageGenerated.Broadcast(nullptr, false, Msg);
		return;
	}

	bImageBusy = true;
	BroadcastStatus(LOCTEXT("ImgRunning", "Generating image..."));

	TSharedPtr<FACFAIToolsNotification> Notification = MakeShared<FACFAIToolsNotification>();
	Notification->Start(LOCTEXT("ImgRunningN", "Generating image..."));

	const FString ResolvedName = ResolveBaseName(AssetName, Prompt, TEXT("Generated_Image"));

	TWeakObjectPtr<UACFAIToolsSubsystem> WeakThis(this);
	FACFOnImageReady OnReady;
	OnReady.BindLambda([WeakThis, Notification, ResolvedName](bool bSuccess, const TArray<uint8>& Png, const FString& Error)
	{
		if (UACFAIToolsSubsystem* Self = WeakThis.Get())
		{
			Self->HandleImageResult(bSuccess, Png, Error, ResolvedName, Notification);
		}
	});

	if (bUseGpt)
	{
		FACFGptImageClient::GenerateImage(Prompt, OnReady);
	}
	else
	{
		FACFNanoBananaClient::GenerateImage(Prompt, TArray<uint8>(), OnReady);
	}
}

void UACFAIToolsSubsystem::HandleImageResult(bool bSuccess, const TArray<uint8>& Png, const FString& Error, FString AssetName, TSharedPtr<FACFAIToolsNotification> Notification)
{
	bImageBusy = false;

	if (!bSuccess)
	{
		const FText Msg = FText::Format(LOCTEXT("ImgFailMsg", "Image generation failed: {0}"), FText::FromString(Error));
		BroadcastStatus(Msg);
		if (Notification.IsValid())
		{
			Notification->Finish(LOCTEXT("ImgFail", "Image generation failed."), false);
		}
		OnImageGeneratedNative.Broadcast(nullptr, false, Error);
		OnImageGenerated.Broadcast(nullptr, false, Error);
		return;
	}

	LastImageBytes = Png;

	UTexture2D* Texture = FACFAIToolsImporter::ImportTexturePng(Png, AssetName);
	if (Texture)
	{
		LastTexture = Texture;
	}

	const bool bImported = Texture != nullptr;
	const FText Msg = bImported ? LOCTEXT("ImgOk", "Image generated and imported.") : LOCTEXT("ImgImportFail", "Image generated but the import failed.");
	BroadcastStatus(Msg);
	if (Notification.IsValid())
	{
		Notification->Finish(Msg, bImported);
	}

	const FString OutError = bImported ? FString() : TEXT("Import failed.");
	OnImageGeneratedNative.Broadcast(Texture, bImported, OutError);
	OnImageGenerated.Broadcast(Texture, bImported, OutError);
}

void UACFAIToolsSubsystem::FailMeshStart(const FString& Message)
{
	BroadcastStatus(FText::FromString(Message));
	OnMeshGeneratedNative.Broadcast(nullptr, false, Message);
	OnMeshGenerated.Broadcast(nullptr, false, Message);
}

bool UACFAIToolsSubsystem::CheckMeshBackend(const IACF3DBackend& Backend, bool bFromImage)
{
	const FString Provider = Backend.GetDisplayName().ToString();
	const FACF3DCapabilities Capabilities = Backend.GetCapabilities();
	if (bFromImage ? !Capabilities.bImageTo3D : !Capabilities.bTextTo3D)
	{
		FailMeshStart(FString::Printf(TEXT("%s cannot generate meshes from %s. Switch the 3D provider in Settings."), *Provider, bFromImage ? TEXT("images") : TEXT("text")));
		return false;
	}
	if (UACFAIToolsSettings::Get()->GetThreeDApiKey(Backend.GetProvider()).TrimStartAndEnd().IsEmpty())
	{
		FailMeshStart(FString::Printf(TEXT("The %s API key is not set. Open Settings and add it."), *Provider));
		return false;
	}
	return true;
}

void UACFAIToolsSubsystem::StartMeshJob(const IACF3DBackend& Backend, FACF3DRequest Request, bool bFromImage, const FString& ResolvedName)
{
	bMeshBusy = true;
	LastAssignedSkeleton = nullptr;

	const FText Running = Request.bRig
		? (bFromImage
			? LOCTEXT("ImgMeshRigRunning", "Generating and rigging character from image...")
			: LOCTEXT("MeshRigRunning", "Generating and rigging 3D character..."))
		: (bFromImage
			? LOCTEXT("ImgMeshRunning", "Generating 3D mesh from image...")
			: LOCTEXT("MeshRunning", "Generating 3D mesh..."));
	BroadcastStatus(Running);

	TSharedPtr<FACFAIToolsNotification> Notification = MakeShared<FACFAIToolsNotification>();
	Notification->Start(Running);

	const FText ProviderName = Backend.GetDisplayName();
	TWeakObjectPtr<UACFAIToolsSubsystem> WeakThis(this);

	FACF3DStatusDelegate OnStatus;
	OnStatus.BindLambda([WeakThis, Notification](const FText& Status)
	{
		if (Notification.IsValid())
		{
			Notification->Update(Status);
		}
		if (UACFAIToolsSubsystem* Self = WeakThis.Get())
		{
			Self->BroadcastStatus(Status);
		}
	});

	FACF3DFinishedDelegate OnFinished;
	OnFinished.BindLambda([WeakThis, Notification, ResolvedName, ProviderName](bool bSuccess, const FACF3DMeshResult& Mesh, const FString& Error)
	{
		if (UACFAIToolsSubsystem* Self = WeakThis.Get())
		{
			Self->HandleMeshResult(bSuccess, Mesh, Error, ResolvedName, ProviderName, Notification);
		}
	});

	if (bFromImage)
	{
		Backend.ImageTo3D(MoveTemp(Request), MoveTemp(OnStatus), MoveTemp(OnFinished));
	}
	else
	{
		Backend.TextTo3D(MoveTemp(Request), MoveTemp(OnStatus), MoveTemp(OnFinished));
	}
}

void UACFAIToolsSubsystem::GenerateMeshFromText(const FString& Prompt, const FString& AssetName, bool bRig, float HeightMeters)
{
	if (Prompt.IsEmpty())
	{
		BroadcastStatus(LOCTEXT("ErrNoPrompt", "Please enter a prompt before generating."));
		OnMeshGeneratedNative.Broadcast(nullptr, false, TEXT("Please enter a prompt before generating."));
		OnMeshGenerated.Broadcast(nullptr, false, TEXT("Please enter a prompt before generating."));
		return;
	}

	const IACF3DBackend& Backend = ACF3D::GetActiveBackend();
	if (!CheckMeshBackend(Backend, /*bFromImage*/ false))
	{
		return;
	}

	FACF3DRequest Request;
	Request.Prompt = Prompt;
	Request.bRig = bRig && Backend.GetCapabilities().bRigging;
	Request.HeightMeters = HeightMeters;
	StartMeshJob(Backend, MoveTemp(Request), /*bFromImage*/ false, ResolveBaseName(AssetName, Prompt, TEXT("Generated_Mesh")));
}

bool UACFAIToolsSubsystem::LoadPendingImageFromFile(const FString& FilePath)
{
	TArray<uint8> Bytes;
	if (!FFileHelper::LoadFileToArray(Bytes, *FilePath) || Bytes.IsEmpty()) { return false; }
	LastImageBytes = MoveTemp(Bytes);
	return true;
}

void UACFAIToolsSubsystem::GenerateMeshFromImage(const FString& AssetName, bool bRig, float HeightMeters)
{
	if (LastImageBytes.Num() == 0)
	{
		FailMeshStart(TEXT("Generate an image first, then convert it to a 3D mesh."));
		return;
	}

	const IACF3DBackend& Backend = ACF3D::GetActiveBackend();
	if (!CheckMeshBackend(Backend, /*bFromImage*/ true))
	{
		return;
	}

	FACF3DRequest Request;
	Request.PngBytes = LastImageBytes;
	Request.bRig = bRig && Backend.GetCapabilities().bRigging;
	Request.HeightMeters = HeightMeters;
	StartMeshJob(Backend, MoveTemp(Request), /*bFromImage*/ true, ResolveBaseName(AssetName, FString(), TEXT("Generated_Mesh")));
}

void UACFAIToolsSubsystem::HandleMeshResult(bool bSuccess, const FACF3DMeshResult& MeshResult, const FString& Error, FString AssetName, const FText& ProviderName, TSharedPtr<FACFAIToolsNotification> Notification)
{
	bMeshBusy = false;

	if (!bSuccess)
	{
		const FText Msg = FText::Format(LOCTEXT("MeshFailMsg", "3D generation failed: {0}"), FText::FromString(Error));
		BroadcastStatus(Msg);
		if (Notification.IsValid())
		{
			Notification->Finish(LOCTEXT("MeshFail", "3D generation failed."), false);
		}
		OnMeshGeneratedNative.Broadcast(nullptr, false, Error);
		OnMeshGenerated.Broadcast(nullptr, false, Error);
		return;
	}

	TArray<UObject*> Imported;
	UObject* Mesh = nullptr;
	FString ImportError;

	if (MeshResult.bRigged)
	{
		USkeletalMesh* SkelMesh = FACFAIToolsImporter::ImportRiggedFbx(MeshResult.Bytes, AssetName, Imported, ImportError);
		Mesh = SkelMesh;
		if (SkelMesh)
		{
			LastAssignedSkeleton = SkelMesh->GetSkeleton();
		}
	}
	else
	{
		Mesh = FACFAIToolsImporter::ImportMeshGlb(MeshResult.Bytes, AssetName, Imported);
		if (!Mesh)
		{
			ImportError = TEXT("Import failed.");
		}
	}

	const bool bImported = Mesh != nullptr;
	const FText Msg = bImported
		? (MeshResult.bRigged
			? FText::Format(LOCTEXT("MeshRigOk", "Rigged character imported with {0}'s skeleton (retarget it onto your own skeleton if needed)."), ProviderName)
			: LOCTEXT("MeshOk", "3D mesh generated and imported."))
		: FText::Format(LOCTEXT("MeshImportFailFmt", "3D generation finished but the import failed: {0}"),
			FText::FromString(ImportError.IsEmpty() ? TEXT("unknown error") : ImportError));
	BroadcastStatus(Msg);
	if (Notification.IsValid())
	{
		Notification->Finish(Msg, bImported);
	}

	const FString OutError = bImported ? FString() : (ImportError.IsEmpty() ? TEXT("Import failed.") : ImportError);
	OnMeshGeneratedNative.Broadcast(Mesh, bImported, OutError);
	OnMeshGenerated.Broadcast(Mesh, bImported, OutError);
}

#undef LOCTEXT_NAMESPACE
