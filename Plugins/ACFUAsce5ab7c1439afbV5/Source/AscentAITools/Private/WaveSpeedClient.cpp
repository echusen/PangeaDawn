// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "WaveSpeedClient.h"

#include "ThreeDParsers.h"
#include "AscentAIToolsSettings.h"
#include "Dom/JsonObject.h"

#define LOCTEXT_NAMESPACE "WaveSpeedClient"

FACF3DWaveSpeedJob::FACF3DWaveSpeedJob(FACF3DRequest InRequest, bool bInFromImage)
	: Request(MoveTemp(InRequest))
	, bFromImage(bInFromImage)
{
	const UACFAIToolsSettings* Settings = UACFAIToolsSettings::Get();
	ApiKey = Settings->WaveSpeedApiKey.TrimStartAndEnd();
	BaseUrl = Settings->WaveSpeedBaseUrl.TrimStartAndEnd();
	BaseUrl.RemoveFromEnd(TEXT("/"));
	ModelId = (bInFromImage ? Settings->WaveSpeedImageModel : Settings->WaveSpeedTextModel).TrimStartAndEnd();
	ModelId.RemoveFromStart(TEXT("/"));

	bTextures = Settings->bMeshyGenerateTextures;
	bPbr = bTextures && Settings->bMeshyEnablePBR;
	FaceCount = ACF3D::ClampWaveSpeedFaceCount(Settings->MeshyTargetPolycount);
}

FString FACF3DWaveSpeedJob::Describe(int32 HttpCode, const TSharedPtr<FJsonObject>& Json) const
{
	if (HttpCode <= 0)
	{
		return TEXT("Could not reach WaveSpeed. Check your internet connection.");
	}
	return TEXT("WaveSpeed: ") + ACF3D::ExtractServiceError(Json, HttpCode);
}

void FACF3DWaveSpeedJob::Begin()
{
	if (ApiKey.IsEmpty())
	{
		Fail(TEXT("The WaveSpeed API key is not set. Add it in the Yarin gear settings or Project Settings > Plugins > Ascent AI Tools."));
		return;
	}
	if (BaseUrl.IsEmpty() || ModelId.IsEmpty())
	{
		Fail(TEXT("The WaveSpeed base URL or model id is empty. Check Project Settings > Plugins > Ascent AI Tools > Advanced."));
		return;
	}

	if (bFromImage)
	{
		if (Request.PngBytes.Num() == 0)
		{
			Fail(TEXT("No source image available. Generate an image first."));
			return;
		}
		UploadImage();
		return;
	}

	if (Request.Prompt.TrimStartAndEnd().IsEmpty())
	{
		Fail(TEXT("Please enter a prompt before generating."));
		return;
	}
	Submit(FString());
}

void FACF3DWaveSpeedJob::UploadImage()
{
	SetStatus(LOCTEXT("Uploading", "WaveSpeed: uploading image..."));

	// Step 1: ask for a signed upload target. WaveSpeed model inputs are public URLs, not bytes.
	const TSharedRef<FJsonObject> Ticket = MakeShared<FJsonObject>();
	Ticket->SetStringField(TEXT("filename"), TEXT("image.png"));
	Ticket->SetNumberField(TEXT("size"), Request.PngBytes.Num());
	Ticket->SetStringField(TEXT("content_type"), TEXT("image/png"));

	SendJson(TEXT("POST"), BaseUrl + TEXT("/media/uploads"), Ticket, [this](int32 HttpCode, const TSharedPtr<FJsonObject>& Json)
	{
		const TSharedPtr<FJsonObject>* Data = nullptr;
		const TSharedPtr<FJsonObject>* Upload = nullptr;
		FString DownloadUrl, PutUrl, Method;
		const bool bTicketOk = HttpCode >= 200 && HttpCode < 300 && Json.IsValid()
			&& Json->TryGetObjectField(TEXT("data"), Data) && Data
			&& (*Data)->TryGetStringField(TEXT("download_url"), DownloadUrl) && !DownloadUrl.IsEmpty()
			&& (*Data)->TryGetObjectField(TEXT("upload"), Upload) && Upload
			&& (*Upload)->TryGetStringField(TEXT("url"), PutUrl) && !PutUrl.IsEmpty();
		if (!bTicketOk)
		{
			Fail(Describe(HttpCode, Json));
			return;
		}
		(*Upload)->TryGetStringField(TEXT("method"), Method);

		// Step 2: put the bytes on the returned URL with the returned headers. The API key never goes to the storage host.
		FCall Put;
		Put.Verb = Method.IsEmpty() ? TEXT("PUT") : Method;
		Put.Url = PutUrl;
		Put.bAuthorize = false;
		Put.Body = Request.PngBytes;
		const TSharedPtr<FJsonObject>* Headers = nullptr;
		if ((*Upload)->TryGetObjectField(TEXT("headers"), Headers) && Headers)
		{
			for (const TPair<FString, TSharedPtr<FJsonValue>>& Header : (*Headers)->Values)
			{
				FString Value;
				if (Header.Value.IsValid() && Header.Value->TryGetString(Value))
				{
					Put.Headers.Add(Header.Key, Value);
				}
			}
		}
		SetStatus(LOCTEXT("Storing", "WaveSpeed: storing image..."));
		Send(MoveTemp(Put), [this, DownloadUrl](int32 PutCode, const TSharedPtr<FJsonObject>&)
		{
			if (PutCode < 200 || PutCode >= 300)
			{
				Fail(PutCode <= 0
					? TEXT("Could not reach WaveSpeed storage. Check your internet connection.")
					: FString::Printf(TEXT("WaveSpeed could not store the image (HTTP %d)."), PutCode));
				return;
			}
			Submit(DownloadUrl);
		});
	});
}

void FACF3DWaveSpeedJob::Submit(const FString& ImageUrl)
{
	SetStatus(LOCTEXT("Submitting", "WaveSpeed: submitting request..."));

	const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	if (bFromImage)
	{
		Body->SetStringField(TEXT("image"), ImageUrl);
	}
	else
	{
		Body->SetStringField(TEXT("prompt"), Request.Prompt.TrimStartAndEnd().Left(1024));
	}
	Body->SetStringField(TEXT("generate_type"), bTextures ? TEXT("Normal") : TEXT("Geometry"));
	Body->SetBoolField(TEXT("enable_pbr"), bPbr);
	Body->SetNumberField(TEXT("face_count"), FaceCount);

	SendJson(TEXT("POST"), BaseUrl + TEXT("/") + ModelId, Body, [this](int32 HttpCode, const TSharedPtr<FJsonObject>& Json)
	{
		OnSubmitted(HttpCode, Json);
	});
}

void FACF3DWaveSpeedJob::OnSubmitted(int32 HttpCode, const TSharedPtr<FJsonObject>& Json)
{
	const TSharedPtr<FJsonObject>* Data = nullptr;
	FString Id;
	const bool bAccepted = HttpCode >= 200 && HttpCode < 300 && Json.IsValid()
		&& Json->TryGetObjectField(TEXT("data"), Data) && Data && (*Data)->TryGetStringField(TEXT("id"), Id) && !Id.IsEmpty();
	if (!bAccepted)
	{
		Fail(Describe(HttpCode, Json));
		return;
	}

	PredictionId = Id;
	StartPolling([this]()
	{
		PollJson(BaseUrl + TEXT("/predictions/") + PredictionId + TEXT("/result"), [this](int32 PollCode, const TSharedPtr<FJsonObject>& PollJsonBody)
		{
			OnPollResponse(PollCode, PollJsonBody);
		});
	});
}

void FACF3DWaveSpeedJob::OnPollResponse(int32 HttpCode, const TSharedPtr<FJsonObject>& Json)
{
	const ACF3D::FPollResult Result = ACF3D::ParseWaveSpeedResult(HttpCode, Json);
	switch (Result.State)
	{
	case ACF3D::EPollState::Failed:
		Fail(Result.Error.StartsWith(TEXT("WaveSpeed")) ? Result.Error : TEXT("WaveSpeed: ") + Result.Error);
		return;

	case ACF3D::EPollState::Pending:
		// WaveSpeed reports no percentage, so show the time spent instead.
		SetStatus(FText::Format(LOCTEXT("Generating", "WaveSpeed: generating 3D mesh ({0}s)"), FText::AsNumber(FMath::FloorToInt(ElapsedSeconds()))));
		return;

	case ACF3D::EPollState::Succeeded:
		Download(Result.ModelUrl, ACF3D::ExtensionFromUrl(Result.ModelUrl), false);
		return;
	}
}

#undef LOCTEXT_NAMESPACE
