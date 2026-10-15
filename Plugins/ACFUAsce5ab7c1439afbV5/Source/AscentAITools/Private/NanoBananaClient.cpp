// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.

#include "NanoBananaClient.h"
#include "AscentAIToolsSettings.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/Base64.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"

namespace
{
	bool ExtractInlineImage(const TSharedPtr<FJsonObject>& Root, TArray<uint8>& OutBytes, FString& OutError)
	{
		const TArray<TSharedPtr<FJsonValue>>* Candidates = nullptr;
		if (!Root->TryGetArrayField(TEXT("candidates"), Candidates) || Candidates->Num() == 0)
		{
			// Surface API error if present.
			const TSharedPtr<FJsonObject>* ErrorObj = nullptr;
			if (Root->TryGetObjectField(TEXT("error"), ErrorObj))
			{
				OutError = (*ErrorObj)->GetStringField(TEXT("message"));
			}
			else
			{
				OutError = TEXT("The image service returned no result.");
			}
			return false;
		}

		const TSharedPtr<FJsonObject> Candidate = (*Candidates)[0]->AsObject();
		if (!Candidate.IsValid())
		{
			OutError = TEXT("Malformed image generation response.");
			return false;
		}

		const TSharedPtr<FJsonObject>* Content = nullptr;
		if (!Candidate->TryGetObjectField(TEXT("content"), Content))
		{
			OutError = TEXT("Empty image generation response.");
			return false;
		}

		const TArray<TSharedPtr<FJsonValue>>* Parts = nullptr;
		if (!(*Content)->TryGetArrayField(TEXT("parts"), Parts))
		{
			OutError = TEXT("Empty image generation response.");
			return false;
		}

		for (const TSharedPtr<FJsonValue>& PartValue : *Parts)
		{
			const TSharedPtr<FJsonObject> Part = PartValue->AsObject();
			if (!Part.IsValid())
			{
				continue;
			}

			const TSharedPtr<FJsonObject>* InlineData = nullptr;
			if (Part->TryGetObjectField(TEXT("inlineData"), InlineData) ||
				Part->TryGetObjectField(TEXT("inline_data"), InlineData))
			{
				FString Base64Data;
				if ((*InlineData)->TryGetStringField(TEXT("data"), Base64Data) && !Base64Data.IsEmpty())
				{
					if (FBase64::Decode(Base64Data, OutBytes))
					{
						return true;
					}
				}
			}
		}

		OutError = TEXT("The response did not contain any image data.");
		return false;
	}
}

void FACFNanoBananaClient::GenerateImage(const FString& Prompt, const TArray<uint8>& ReferenceImagePng, FACFOnImageReady OnComplete)
{
	const UACFAIToolsSettings* Settings = UACFAIToolsSettings::Get();
	const FString ApiKey = Settings ? Settings->ImageApiKey : FString();
	FString Model = Settings ? Settings->NanoBananaModel : FString();
	if (Model.IsEmpty())
	{
		Model = TEXT("gemini-2.5-flash-image");
	}

	if (ApiKey.IsEmpty())
	{
		OnComplete.ExecuteIfBound(false, TArray<uint8>(), TEXT("Image API key is not set. Add it in Project Settings > Plugins > Ascent AI Tools."));
		return;
	}

	// Build the request payload.
	TSharedRef<FJsonObject> RootObj = MakeShared<FJsonObject>();

	TArray<TSharedPtr<FJsonValue>> Parts;

	TSharedRef<FJsonObject> TextPart = MakeShared<FJsonObject>();
	TextPart->SetStringField(TEXT("text"), Prompt);
	Parts.Add(MakeShared<FJsonValueObject>(TextPart));

	if (ReferenceImagePng.Num() > 0)
	{
		TSharedRef<FJsonObject> InlineData = MakeShared<FJsonObject>();
		InlineData->SetStringField(TEXT("mimeType"), TEXT("image/png"));
		InlineData->SetStringField(TEXT("data"), FBase64::Encode(ReferenceImagePng));

		TSharedRef<FJsonObject> ImagePart = MakeShared<FJsonObject>();
		ImagePart->SetObjectField(TEXT("inlineData"), InlineData);
		Parts.Add(MakeShared<FJsonValueObject>(ImagePart));
	}

	TSharedRef<FJsonObject> ContentObj = MakeShared<FJsonObject>();
	ContentObj->SetStringField(TEXT("role"), TEXT("user"));
	ContentObj->SetArrayField(TEXT("parts"), Parts);

	TArray<TSharedPtr<FJsonValue>> Contents;
	Contents.Add(MakeShared<FJsonValueObject>(ContentObj));
	RootObj->SetArrayField(TEXT("contents"), Contents);

	// Force both modalities, otherwise the model may return text only.
	TSharedRef<FJsonObject> GenConfig = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> Modalities;
	Modalities.Add(MakeShared<FJsonValueString>(TEXT("TEXT")));
	Modalities.Add(MakeShared<FJsonValueString>(TEXT("IMAGE")));
	GenConfig->SetArrayField(TEXT("responseModalities"), Modalities);
	RootObj->SetObjectField(TEXT("generationConfig"), GenConfig);

	FString Body;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Body);
	FJsonSerializer::Serialize(RootObj, Writer);

	const UACFAIToolsSettings* Adv = UACFAIToolsSettings::Get();
	FString BaseUrl = Adv ? Adv->GeminiBaseUrl : FString();
	if (BaseUrl.IsEmpty())
	{
		BaseUrl = TEXT("https://generativelanguage.googleapis.com/v1beta/models");
	}
	BaseUrl.RemoveFromEnd(TEXT("/"));

	const FString Url = FString::Printf(TEXT("%s/%s:generateContent"), *BaseUrl, *Model);

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(Url);
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetHeader(TEXT("x-goog-api-key"), ApiKey);
	Request->SetContentAsString(Body);
	const float ImgTimeout = Adv ? Adv->ImageRequestTimeout : 180.0f;
	Request->SetTimeout(ImgTimeout);
	// The server sends no data while generating; raise the idle/activity timeout too.
	Request->SetActivityTimeout(ImgTimeout);

	Request->OnProcessRequestComplete().BindLambda(
		[OnComplete](FHttpRequestPtr /*Req*/, FHttpResponsePtr Response, bool bConnectedSuccessfully)
		{
			if (!bConnectedSuccessfully || !Response.IsValid())
			{
				OnComplete.ExecuteIfBound(false, TArray<uint8>(), TEXT("Could not reach the image generation service. Check your internet connection."));
				return;
			}

			const FString ResponseStr = Response->GetContentAsString();
			TSharedPtr<FJsonObject> Root;
			TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseStr);
			if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
			{
				OnComplete.ExecuteIfBound(false, TArray<uint8>(),
					FString::Printf(TEXT("Invalid response from the image service (HTTP %d)."), Response->GetResponseCode()));
				return;
			}

			TArray<uint8> ImageBytes;
			FString Error;
			if (ExtractInlineImage(Root, ImageBytes, Error))
			{
				OnComplete.ExecuteIfBound(true, ImageBytes, FString());
			}
			else
			{
				OnComplete.ExecuteIfBound(false, TArray<uint8>(), Error);
			}
		});

	Request->ProcessRequest();
}
