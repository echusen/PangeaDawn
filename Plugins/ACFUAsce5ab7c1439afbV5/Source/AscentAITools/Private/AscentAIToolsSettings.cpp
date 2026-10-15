// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.

#include "AscentAIToolsSettings.h"
#include "UObject/UnrealType.h"

namespace
{
	FString (*GGeneratedContentFolderResolver)() = nullptr;
}

UACFAIToolsSettings::UACFAIToolsSettings()
{
	ImageProvider = EACFImageProvider::GptImage;
	NanoBananaModel = TEXT("gemini-2.5-flash-image");
	OpenAIModel = TEXT("gpt-image-2");
	bMeshyGenerateTextures = true;
	bMeshyEnablePBR = true;
	bMeshyAutoRefine = true;
	bMeshyRigHumanoid = false;
	MeshyCharacterHeightMeters = 1.7f;
	MeshyArtStyle = EACFMeshyArtStyle::Realistic;
	// Quad topology is the better default for game-ready assets: cleaner edge flow,
	// deforms/animates better and is easier to retopo (the engine triangulates at runtime anyway).
	MeshyTopology = EACFMeshyTopology::Quad;
	MeshyTextureResolution = EACFMeshyTextureResolution::FourK;
	MeshyTargetPolycount = 30000;
	MeshyPollInterval = 5.0f;

	// Advanced defaults
	ImageSize = TEXT("1024x1024");
	ImageRequestTimeout = 600.0f;
	GeminiBaseUrl = TEXT("https://generativelanguage.googleapis.com/v1beta/models");
	OpenAIImageEndpoint = TEXT("https://api.openai.com/v1/images/generations");
	MeshyBaseUrl = TEXT("https://api.meshy.ai");
	TextTo3DPath = TEXT("/openapi/v2/text-to-3d");
	ImageTo3DPath = TEXT("/openapi/v1/image-to-3d");
	RiggingPath = TEXT("/openapi/v1/rigging");
	TripoBaseUrl = TEXT("https://openapi.tripo3d.ai/v3");
	TripoModelVersion = TEXT("v3.1-20260211");
	WaveSpeedBaseUrl = TEXT("https://api.wavespeed.ai/api/v3");
	WaveSpeedImageModel = TEXT("wavespeed-ai/hunyuan3d-v3/image-to-3d");
	WaveSpeedTextModel = TEXT("wavespeed-ai/hunyuan3d-v3/text-to-3d");
	MeshRequestTimeout = 60.0f;
	MeshDownloadTimeout = 300.0f;
	MeshFormat = TEXT("glb");
	bShouldRemesh = true;
}

FString UACFAIToolsSettings::GetThreeDApiKey(EACF3DProvider Provider) const
{
	switch (Provider)
	{
	case EACF3DProvider::Tripo: return TripoApiKey;
	case EACF3DProvider::WaveSpeed: return WaveSpeedApiKey;
	case EACF3DProvider::Meshy:
	default: return ThreeDApiKey;
	}
}

void UACFAIToolsSettings::SetActiveThreeDApiKey(const FString& Key)
{
	switch (ThreeDProvider)
	{
	case EACF3DProvider::Tripo: TripoApiKey = Key; break;
	case EACF3DProvider::WaveSpeed: WaveSpeedApiKey = Key; break;
	case EACF3DProvider::Meshy:
	default: ThreeDApiKey = Key; break;
	}
}

FString UACFAIToolsSettings::GetArtStyleString() const
{
	return MeshyArtStyle == EACFMeshyArtStyle::Sculpture ? TEXT("sculpture") : TEXT("realistic");
}

FString UACFAIToolsSettings::GetTopologyString() const
{
	return MeshyTopology == EACFMeshyTopology::Quad ? TEXT("quad") : TEXT("triangle");
}

FString UACFAIToolsSettings::GetTextureResolutionString() const
{
	switch (MeshyTextureResolution)
	{
	case EACFMeshyTextureResolution::EightK: return TEXT("8k");
	case EACFMeshyTextureResolution::TwoK: return TEXT("2k");
	case EACFMeshyTextureResolution::FourK:
	default: return TEXT("4k");
	}
}

const UACFAIToolsSettings* UACFAIToolsSettings::Get()
{
	return GetDefault<UACFAIToolsSettings>();
}

void UACFAIToolsSettings::SetGeneratedContentFolderResolver(FString (*Resolver)())
{
	GGeneratedContentFolderResolver = Resolver;
}

FString UACFAIToolsSettings::GetGeneratedContentFolder()
{
	FString Folder;
	if (GGeneratedContentFolderResolver)
	{
		Folder = GGeneratedContentFolderResolver();
	}
	else if (UClass* SettingsClass = FindObject<UClass>(nullptr, TEXT("/Script/AscentACFAssistant.ACFAssistantSettings")))
	{
		if (const FStrProperty* Property = FindFProperty<FStrProperty>(SettingsClass, TEXT("DefaultContentFolder")))
		{
			Folder = Property->GetPropertyValue_InContainer(SettingsClass->GetDefaultObject());
		}
	}
	Folder.TrimStartAndEndInline();
	while (Folder.EndsWith(TEXT("/")))
	{
		Folder.LeftChopInline(1);
	}
	return Folder;
}
