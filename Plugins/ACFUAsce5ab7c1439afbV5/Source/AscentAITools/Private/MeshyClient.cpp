// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.

#include "MeshyClient.h"
#include "AscentAIToolsSettings.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/Base64.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"

#define LOCTEXT_NAMESPACE "MeshyClient"

namespace
{
	FString SerializeJson(const TSharedRef<FJsonObject>& Obj)
	{
		FString Out;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
		FJsonSerializer::Serialize(Obj, Writer);
		return Out;
	}

	FString MakeEndpoint(const FString& Base, const FString& Path, const FString& Fallback)
	{
		FString B = Base.IsEmpty() ? TEXT("https://api.meshy.ai") : Base;
		B.RemoveFromEnd(TEXT("/"));
		FString P = Path.IsEmpty() ? Fallback : Path;
		if (!P.StartsWith(TEXT("/")))
		{
			P = TEXT("/") + P;
		}
		return B + P;
	}

	FString GetMeshFormat()
	{
		const UACFAIToolsSettings* Adv = UACFAIToolsSettings::Get();
		const FString Fmt = Adv ? Adv->MeshFormat : FString();
		return Fmt.IsEmpty() ? TEXT("glb") : Fmt;
	}

	bool GetShouldRemesh()
	{
		const UACFAIToolsSettings* Adv = UACFAIToolsSettings::Get();
		return Adv ? Adv->bShouldRemesh : true;
	}

	float ResolveHeightMeters(float HeightMeters)
	{
		if (HeightMeters > 0.0f)
		{
			return HeightMeters;
		}
		const UACFAIToolsSettings* Settings = UACFAIToolsSettings::Get();
		return (Settings && Settings->MeshyCharacterHeightMeters > 0.0f) ? Settings->MeshyCharacterHeightMeters : 1.7f;
	}

	FString GetTextureResolution()
	{
		const UACFAIToolsSettings* Settings = UACFAIToolsSettings::Get();
		return Settings ? Settings->GetTextureResolutionString() : TEXT("4k");
	}

	/** 4k/8k textures need a Meshy model that supports them. */
	void ApplyTextureResolutionFields(const TSharedRef<FJsonObject>& Body, bool bTextures)
	{
		if (!bTextures)
		{
			return;
		}
		const FString Resolution = GetTextureResolution();
		Body->SetStringField(TEXT("texture_resolution"), Resolution);
		if (Resolution == TEXT("4k") || Resolution == TEXT("8k"))
		{
			Body->SetStringField(TEXT("ai_model"), TEXT("latest"));
		}
	}
}

TSharedPtr<FACFMeshyClient> FACFMeshyClient::TextTo3D(const FString& Prompt, bool bRig, float HeightMeters, FACF3DStatusDelegate OnStatus, FACF3DFinishedDelegate OnFinished)
{
	const UACFAIToolsSettings* Settings = UACFAIToolsSettings::Get();
	const UACFAIToolsSettings* Adv = UACFAIToolsSettings::Get();

	TSharedPtr<FACFMeshyClient> Client = MakeShared<FACFMeshyClient>();
	Client->ApiKey = Settings ? Settings->ThreeDApiKey : FString();
	Client->Endpoint = MakeEndpoint(Adv ? Adv->MeshyBaseUrl : FString(), Adv ? Adv->TextTo3DPath : FString(), TEXT("/openapi/v2/text-to-3d"));
	Client->bIsImageTo3D = false;
	Client->bWantRig = bRig;
	Client->HeightMeters = ResolveHeightMeters(HeightMeters);
	Client->PollInterval = Settings ? Settings->MeshyPollInterval : 5.0f;
	Client->OnStatusDelegate = OnStatus;
	Client->OnFinishedDelegate = OnFinished;
	Client->KeepAlive = Client;

	if (Client->ApiKey.IsEmpty())
	{
		Client->CompleteError(TEXT("3D API key is not set. Add it in Project Settings > Plugins > Ascent AI Tools."));
		return Client;
	}

	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("mode"), TEXT("preview"));
	Body->SetStringField(TEXT("prompt"), Prompt);
	Body->SetBoolField(TEXT("should_remesh"), GetShouldRemesh());
	if (Settings)
	{
		Body->SetStringField(TEXT("art_style"), Settings->GetArtStyleString());
	}
	TArray<TSharedPtr<FJsonValue>> Formats;
	Formats.Add(MakeShared<FJsonValueString>(GetMeshFormat()));
	Body->SetArrayField(TEXT("target_formats"), Formats);

	Client->CreateTask(SerializeJson(Body));
	return Client;
}

TSharedPtr<FACFMeshyClient> FACFMeshyClient::ImageTo3D(const TArray<uint8>& PngBytes, bool bRig, float HeightMeters, FACF3DStatusDelegate OnStatus, FACF3DFinishedDelegate OnFinished)
{
	const UACFAIToolsSettings* Settings = UACFAIToolsSettings::Get();
	const UACFAIToolsSettings* Adv = UACFAIToolsSettings::Get();

	TSharedPtr<FACFMeshyClient> Client = MakeShared<FACFMeshyClient>();
	Client->ApiKey = Settings ? Settings->ThreeDApiKey : FString();
	Client->Endpoint = MakeEndpoint(Adv ? Adv->MeshyBaseUrl : FString(), Adv ? Adv->ImageTo3DPath : FString(), TEXT("/openapi/v1/image-to-3d"));
	Client->bIsImageTo3D = true;
	Client->bWantRig = bRig;
	Client->HeightMeters = ResolveHeightMeters(HeightMeters);
	Client->PollInterval = Settings ? Settings->MeshyPollInterval : 5.0f;
	Client->OnStatusDelegate = OnStatus;
	Client->OnFinishedDelegate = OnFinished;
	Client->KeepAlive = Client;

	if (Client->ApiKey.IsEmpty())
	{
		Client->CompleteError(TEXT("3D API key is not set. Add it in Project Settings > Plugins > Ascent AI Tools."));
		return Client;
	}

	if (PngBytes.Num() == 0)
	{
		Client->CompleteError(TEXT("No source image available. Generate an image first."));
		return Client;
	}

	const FString DataUri = FString::Printf(TEXT("data:image/png;base64,%s"), *FBase64::Encode(PngBytes));

	// Rigging requires a textured humanoid mesh.
	const bool bTextures = bRig || (Settings ? Settings->bMeshyGenerateTextures : true);

	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("image_url"), DataUri);
	Body->SetBoolField(TEXT("should_remesh"), GetShouldRemesh());
	Body->SetBoolField(TEXT("should_texture"), bTextures);
	Body->SetBoolField(TEXT("enable_pbr"), bTextures && (Settings ? Settings->bMeshyEnablePBR : true));
	ApplyTextureResolutionFields(Body, bTextures);
	if (Settings)
	{
		Body->SetStringField(TEXT("topology"), Settings->GetTopologyString());
		Body->SetNumberField(TEXT("target_polycount"), Settings->MeshyTargetPolycount);
	}
	TArray<TSharedPtr<FJsonValue>> Formats;
	Formats.Add(MakeShared<FJsonValueString>(GetMeshFormat()));
	Body->SetArrayField(TEXT("target_formats"), Formats);

	Client->CreateTask(SerializeJson(Body));
	return Client;
}

FACFMeshyClient::~FACFMeshyClient()
{
	if (PollHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(PollHandle);
		PollHandle.Reset();
	}
}

void FACFMeshyClient::SetStatus(const FText& Text)
{
	OnStatusDelegate.ExecuteIfBound(Text);
}

void FACFMeshyClient::CreateTask(const FString& JsonBody)
{
	SetStatus(LOCTEXT("MeshySubmitting", "Submitting 3D request..."));

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(Endpoint);
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *ApiKey));
	Request->SetContentAsString(JsonBody);
	{
		const UACFAIToolsSettings* Adv = UACFAIToolsSettings::Get();
		const float T = Adv ? Adv->MeshRequestTimeout : 60.0f;
		Request->SetTimeout(T);
		Request->SetActivityTimeout(T);
	}
	Request->OnProcessRequestComplete().BindSP(this, &FACFMeshyClient::OnTaskCreated);
	Request->ProcessRequest();
}

void FACFMeshyClient::OnTaskCreated(FHttpRequestPtr /*Request*/, FHttpResponsePtr Response, bool bConnected)
{
	if (!bConnected || !Response.IsValid())
	{
		CompleteError(TEXT("Could not reach the 3D generation service. Check your internet connection."));
		return;
	}

	const int32 Code = Response->GetResponseCode();
	const FString ResponseStr = Response->GetContentAsString();

	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseStr);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		CompleteError(FString::Printf(TEXT("Invalid response from the 3D service (HTTP %d)."), Code));
		return;
	}

	if (Code < 200 || Code >= 300)
	{
		FString Message;
		if (!Root->TryGetStringField(TEXT("message"), Message))
		{
			Message = FString::Printf(TEXT("The 3D service returned HTTP %d."), Code);
		}
		CompleteError(Message);
		return;
	}

	if (!Root->TryGetStringField(TEXT("result"), TaskId) || TaskId.IsEmpty())
	{
		CompleteError(TEXT("The 3D service did not return a task id."));
		return;
	}

	StartPolling();
}

void FACFMeshyClient::StartPolling()
{
	if (PollHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(PollHandle);
	}

	// Poll immediately, then on the configured interval.
	Poll(0.0f);
	PollHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateSP(this, &FACFMeshyClient::Poll), PollInterval);
}

bool FACFMeshyClient::Poll(float /*DeltaTime*/)
{
	if (TaskId.IsEmpty())
	{
		return false;
	}

	const FString Url = FString::Printf(TEXT("%s/%s"), *Endpoint, *TaskId);

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(Url);
	Request->SetVerb(TEXT("GET"));
	Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *ApiKey));
	Request->OnProcessRequestComplete().BindSP(this, &FACFMeshyClient::OnPollResponse);
	Request->ProcessRequest();

	return true; // keep ticking
}

void FACFMeshyClient::OnPollResponse(FHttpRequestPtr /*Request*/, FHttpResponsePtr Response, bool bConnected)
{
	if (!bConnected || !Response.IsValid())
	{
		return; // transient failure; keep polling
	}

	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		return;
	}

	const FString Status = Root->GetStringField(TEXT("status"));
	int32 Progress = 0;
	Root->TryGetNumberField(TEXT("progress"), Progress);

	if (Status == TEXT("SUCCEEDED"))
	{
		// Stop polling this task.
		if (PollHandle.IsValid())
		{
			FTSTicker::GetCoreTicker().RemoveTicker(PollHandle);
			PollHandle.Reset();
		}

		if (bRigPending)
		{
			const TSharedPtr<FJsonObject>* ResultObj = nullptr;
			FString FbxUrl;
			if (Root->TryGetObjectField(TEXT("result"), ResultObj) && ResultObj && (*ResultObj).IsValid())
			{
				(*ResultObj)->TryGetStringField(TEXT("rigged_character_fbx_url"), FbxUrl);
			}

			if (FbxUrl.IsEmpty())
			{
				CompleteError(TEXT("Rigging finished but no rigged character FBX was returned."));
				return;
			}

			SetStatus(LOCTEXT("MeshyDownloadingRig", "Downloading rigged character..."));
			DownloadModel(FbxUrl);
			return;
		}

		const UACFAIToolsSettings* Settings = UACFAIToolsSettings::Get();
		// Text-to-3D preview is untextured; refine bakes textures. Always refine when
		// textures are requested or when we intend to rig (Meshy requires texture).
		const bool bWantRefine = !bIsImageTo3D && !bRefinePending && Settings &&
			(bWantRig || Settings->bMeshyAutoRefine || Settings->bMeshyGenerateTextures);
		if (bWantRefine)
		{
			StartRefine();
			return;
		}

		if (bWantRig)
		{
			StartRigging();
			return;
		}

		const TSharedPtr<FJsonObject>* ModelUrls = nullptr;
		FString GlbUrl;
		if (Root->TryGetObjectField(TEXT("model_urls"), ModelUrls))
		{
			(*ModelUrls)->TryGetStringField(*GetMeshFormat(), GlbUrl);
		}

		if (GlbUrl.IsEmpty())
		{
			CompleteError(TEXT("3D generation finished but no model was returned."));
			return;
		}

		SetStatus(LOCTEXT("MeshyDownloading", "Downloading 3D mesh..."));
		DownloadModel(GlbUrl);
	}
	else if (Status == TEXT("FAILED") || Status == TEXT("CANCELED"))
	{
		FString TaskError = bRigPending ? TEXT("Rigging failed.") : TEXT("3D generation failed.");
		const TSharedPtr<FJsonObject>* ErrorObj = nullptr;
		if (Root->TryGetObjectField(TEXT("task_error"), ErrorObj))
		{
			(*ErrorObj)->TryGetStringField(TEXT("message"), TaskError);
		}
		CompleteError(TaskError);
	}
	else
	{
		FText Phase;
		if (bRigPending)
		{
			Phase = LOCTEXT("MeshyRigging", "Rigging character");
		}
		else if (bRefinePending)
		{
			Phase = LOCTEXT("MeshyRefining", "Refining 3D mesh");
		}
		else
		{
			Phase = LOCTEXT("MeshyGenerating", "Generating 3D mesh");
		}
		SetStatus(FText::Format(LOCTEXT("MeshyProgress", "{0} {1}%"), Phase, FText::AsNumber(Progress)));
	}
}

void FACFMeshyClient::StartRefine()
{
	const UACFAIToolsSettings* Settings = UACFAIToolsSettings::Get();

	bRefinePending = true;
	SetStatus(LOCTEXT("MeshyStartRefine", "Starting 3D refinement..."));

	const bool bTextures = bWantRig || (Settings ? Settings->bMeshyGenerateTextures : true);

	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("mode"), TEXT("refine"));
	Body->SetStringField(TEXT("preview_task_id"), TaskId);
	Body->SetBoolField(TEXT("enable_pbr"), bTextures && (Settings ? Settings->bMeshyEnablePBR : true));
	ApplyTextureResolutionFields(Body, bTextures);
	if (Settings)
	{
		Body->SetStringField(TEXT("topology"), Settings->GetTopologyString());
		Body->SetNumberField(TEXT("target_polycount"), Settings->MeshyTargetPolycount);
	}
	TArray<TSharedPtr<FJsonValue>> Formats;
	Formats.Add(MakeShared<FJsonValueString>(GetMeshFormat()));
	Body->SetArrayField(TEXT("target_formats"), Formats);

	const UACFAIToolsSettings* Adv = UACFAIToolsSettings::Get();

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(Endpoint);
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *ApiKey));
	Request->SetContentAsString(SerializeJson(Body));
	{
		const float T = Adv ? Adv->MeshRequestTimeout : 60.0f;
		Request->SetTimeout(T);
		Request->SetActivityTimeout(T);
	}
	Request->OnProcessRequestComplete().BindSP(this, &FACFMeshyClient::OnTaskCreated);
	Request->ProcessRequest();
}

void FACFMeshyClient::StartRigging()
{
	const UACFAIToolsSettings* Adv = UACFAIToolsSettings::Get();

	bRigPending = true;
	SetStatus(LOCTEXT("MeshyStartRig", "Starting character rigging..."));

	Endpoint = MakeEndpoint(Adv ? Adv->MeshyBaseUrl : FString(), Adv ? Adv->RiggingPath : FString(), TEXT("/openapi/v1/rigging"));

	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("input_task_id"), TaskId);
	Body->SetNumberField(TEXT("height_meters"), HeightMeters);

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(Endpoint);
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *ApiKey));
	Request->SetContentAsString(SerializeJson(Body));
	{
		const float T = Adv ? Adv->MeshRequestTimeout : 60.0f;
		Request->SetTimeout(T);
		Request->SetActivityTimeout(T);
	}
	Request->OnProcessRequestComplete().BindSP(this, &FACFMeshyClient::OnRiggingTaskCreated);
	Request->ProcessRequest();
}

void FACFMeshyClient::OnRiggingTaskCreated(FHttpRequestPtr /*Request*/, FHttpResponsePtr Response, bool bConnected)
{
	if (!bConnected || !Response.IsValid())
	{
		CompleteError(TEXT("Could not reach the rigging service. Check your internet connection."));
		return;
	}

	const int32 Code = Response->GetResponseCode();
	const FString ResponseStr = Response->GetContentAsString();

	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseStr);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		CompleteError(FString::Printf(TEXT("Invalid response from the rigging service (HTTP %d)."), Code));
		return;
	}

	if (Code < 200 || Code >= 300)
	{
		FString Message;
		if (!Root->TryGetStringField(TEXT("message"), Message))
		{
			Message = FString::Printf(TEXT("The rigging service returned HTTP %d."), Code);
		}
		CompleteError(Message);
		return;
	}

	if (!Root->TryGetStringField(TEXT("result"), TaskId) || TaskId.IsEmpty())
	{
		CompleteError(TEXT("The rigging service did not return a task id."));
		return;
	}

	StartPolling();
}

void FACFMeshyClient::DownloadModel(const FString& Url)
{
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(Url);
	Request->SetVerb(TEXT("GET"));
	{
		const UACFAIToolsSettings* Adv = UACFAIToolsSettings::Get();
		const float T = Adv ? Adv->MeshDownloadTimeout : 300.0f;
		Request->SetTimeout(T);
		Request->SetActivityTimeout(T);
	}
	Request->OnProcessRequestComplete().BindSP(this, &FACFMeshyClient::OnModelDownloaded);
	Request->ProcessRequest();
}

void FACFMeshyClient::OnModelDownloaded(FHttpRequestPtr /*Request*/, FHttpResponsePtr Response, bool bConnected)
{
	if (!bConnected || !Response.IsValid() || Response->GetContentLength() <= 0)
	{
		CompleteError(TEXT("Failed to download generated mesh."));
		return;
	}

	FACF3DMeshResult Mesh;
	Mesh.Bytes = Response->GetContent();
	Mesh.bRigged = bRigPending;
	Mesh.Extension = bRigPending ? TEXT("fbx") : GetMeshFormat();
	Complete(true, Mesh, FString());
}

void FACFMeshyClient::CompleteError(const FString& Error)
{
	Complete(false, FACF3DMeshResult(), Error);
}

void FACFMeshyClient::Complete(bool bSuccess, const FACF3DMeshResult& Mesh, const FString& Error)
{
	if (PollHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(PollHandle);
		PollHandle.Reset();
	}

	OnFinishedDelegate.ExecuteIfBound(bSuccess, Mesh, Error);

	// Release the self reference; this may destroy the client.
	KeepAlive.Reset();
}

#undef LOCTEXT_NAMESPACE
