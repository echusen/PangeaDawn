// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "TripoClient.h"

#include "ThreeDParsers.h"
#include "AscentAIToolsSettings.h"
#include "Dom/JsonObject.h"
#include "Misc/Guid.h"

#define LOCTEXT_NAMESPACE "TripoClient"

namespace
{
	const TCHAR* const TripoRigModel = TEXT("v1.0-20240301");
}

FACF3DTripoJob::FACF3DTripoJob(FACF3DRequest InRequest, bool bInFromImage)
	: Request(MoveTemp(InRequest))
	, bFromImage(bInFromImage)
{
	const UACFAIToolsSettings* Settings = UACFAIToolsSettings::Get();
	ApiKey = Settings->TripoApiKey.TrimStartAndEnd();
	BaseUrl = Settings->TripoBaseUrl.TrimStartAndEnd();
	BaseUrl.RemoveFromEnd(TEXT("/"));
	ModelVersion = Settings->TripoModelVersion.TrimStartAndEnd();

	bWantRig = Request.bRig;
	// Rigging needs a textured mesh. Asking for PBR turns texturing on by itself.
	bTextures = bWantRig || Settings->bMeshyGenerateTextures;
	bPbr = bTextures && Settings->bMeshyEnablePBR;
	FaceLimit = Settings->MeshyTargetPolycount;
}

FString FACF3DTripoJob::Describe(int32 HttpCode, const TSharedPtr<FJsonObject>& Json) const
{
	if (HttpCode <= 0)
	{
		return TEXT("Could not reach Tripo. Check your internet connection.");
	}
	return TEXT("Tripo: ") + ACF3D::ExtractServiceError(Json, HttpCode);
}

void FACF3DTripoJob::Begin()
{
	if (ApiKey.IsEmpty())
	{
		Fail(TEXT("The Tripo API key is not set. Add it in the Yarin gear settings or Project Settings > Plugins > Ascent AI Tools."));
		return;
	}
	if (BaseUrl.IsEmpty() || ModelVersion.IsEmpty())
	{
		Fail(TEXT("The Tripo base URL or model version is empty. Check Project Settings > Plugins > Ascent AI Tools > Advanced."));
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
	CreateGeneration(FString());
}

void FACF3DTripoJob::UploadImage()
{
	SetStatus(LOCTEXT("Uploading", "Tripo: uploading image..."));

	const FString Boundary = TEXT("----ACF3D") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	FCall Call;
	Call.Verb = TEXT("POST");
	Call.Url = BaseUrl + TEXT("/files");
	Call.Headers.Add(TEXT("Content-Type"), TEXT("multipart/form-data; boundary=") + Boundary);
	ACF3D::BuildMultipartFile(Boundary, TEXT("file"), TEXT("image.png"), TEXT("image/png"), Request.PngBytes, Call.Body);

	Send(MoveTemp(Call), [this](int32 HttpCode, const TSharedPtr<FJsonObject>& Json)
	{
		FString Token;
		const TSharedPtr<FJsonObject>* Data = nullptr;
		if (HttpCode >= 200 && HttpCode < 300 && Json.IsValid() && Json->TryGetObjectField(TEXT("data"), Data) && Data && (*Data)->TryGetStringField(TEXT("file_token"), Token) && !Token.IsEmpty())
		{
			CreateGeneration(Token);
			return;
		}
		Fail(Describe(HttpCode, Json));
	});
}

void FACF3DTripoJob::CreateGeneration(const FString& ImageToken)
{
	SetStatus(LOCTEXT("Submitting", "Tripo: submitting request..."));

	const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	if (bFromImage)
	{
		Body->SetStringField(TEXT("input"), ImageToken);
	}
	else
	{
		Body->SetStringField(TEXT("prompt"), Request.Prompt.TrimStartAndEnd().Left(1024));
	}
	Body->SetStringField(TEXT("model"), ModelVersion);
	Body->SetBoolField(TEXT("texture"), bTextures);
	Body->SetBoolField(TEXT("pbr"), bPbr);
	if (FaceLimit > 0)
	{
		Body->SetNumberField(TEXT("face_limit"), FaceLimit);
	}

	const FString Path = bFromImage ? TEXT("/generation/image-to-model") : TEXT("/generation/text-to-model");
	SendJson(TEXT("POST"), BaseUrl + Path, Body, [this](int32 HttpCode, const TSharedPtr<FJsonObject>& Json)
	{
		OnTaskCreated(HttpCode, Json);
	});
}

void FACF3DTripoJob::StartRig()
{
	bRigStage = true;
	SetStatus(LOCTEXT("RigSubmitting", "Tripo: rigging character..."));

	// Biped rigging with Mixamo bone names: the names Unreal's IK Retargeter maps out of the box.
	const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("input"), GenerationTaskId);
	Body->SetStringField(TEXT("model"), TripoRigModel);
	Body->SetStringField(TEXT("rig_type"), TEXT("biped"));
	Body->SetStringField(TEXT("spec"), TEXT("mixamo"));
	Body->SetStringField(TEXT("out_format"), TEXT("fbx"));

	SendJson(TEXT("POST"), BaseUrl + TEXT("/animations/rig"), Body, [this](int32 HttpCode, const TSharedPtr<FJsonObject>& Json)
	{
		OnTaskCreated(HttpCode, Json);
	});
}

void FACF3DTripoJob::OnTaskCreated(int32 HttpCode, const TSharedPtr<FJsonObject>& Json)
{
	FString TaskId;
	const TSharedPtr<FJsonObject>* Data = nullptr;
	int32 BodyCode = 0;
	const bool bAccepted = HttpCode >= 200 && HttpCode < 300 && Json.IsValid()
		&& (!Json->TryGetNumberField(TEXT("code"), BodyCode) || BodyCode == 0)
		&& Json->TryGetObjectField(TEXT("data"), Data) && Data && (*Data)->TryGetStringField(TEXT("task_id"), TaskId) && !TaskId.IsEmpty();
	if (!bAccepted)
	{
		Fail(Describe(HttpCode, Json));
		return;
	}

	ActiveTaskId = TaskId;
	if (!bRigStage)
	{
		GenerationTaskId = TaskId;
	}
	StartPolling([this]()
	{
		PollJson(BaseUrl + TEXT("/tasks/") + ActiveTaskId, [this](int32 HttpCode, const TSharedPtr<FJsonObject>& Json)
		{
			OnPollResponse(HttpCode, Json);
		});
	});
}

void FACF3DTripoJob::OnPollResponse(int32 HttpCode, const TSharedPtr<FJsonObject>& Json)
{
	const ACF3D::FPollResult Result = ACF3D::ParseTripoTask(HttpCode, Json);
	switch (Result.State)
	{
	case ACF3D::EPollState::Failed:
		Fail(Result.Error.StartsWith(TEXT("Tripo")) ? Result.Error : TEXT("Tripo: ") + Result.Error);
		return;

	case ACF3D::EPollState::Pending:
	{
		const FText Phase = bRigStage ? LOCTEXT("Rigging", "Tripo: rigging character") : LOCTEXT("Generating", "Tripo: generating 3D mesh");
		SetStatus(Result.Progress >= 0
			? FText::Format(LOCTEXT("Progress", "{0} {1}%"), Phase, FText::AsNumber(Result.Progress))
			: Phase);
		return;
	}

	case ACF3D::EPollState::Succeeded:
		if (bWantRig && !bRigStage)
		{
			StopPolling();
			StartRig();
			return;
		}
		if (bRigStage)
		{
			Download(Result.ModelUrl, TEXT("fbx"), true);
			return;
		}
		Download(Result.ModelUrl, ACF3D::ExtensionFromUrl(Result.ModelUrl), false);
		return;
	}
}

#undef LOCTEXT_NAMESPACE
