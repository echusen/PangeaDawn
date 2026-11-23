// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.

#include "GptImageClient.h"
#include "AscentAIToolsSettings.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/Base64.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"

void FACFGptImageClient::GenerateImage(const FString& Prompt, FACFOnImageReady OnComplete)
{
	const UACFAIToolsSettings* Settings = UACFAIToolsSettings::Get();
	const FString ApiKey = Settings ? Settings->ImageApiKey : FString();
	FString Model = Settings ? Settings->OpenAIModel : FString();
	if (Model.IsEmpty())
	{
		Model = TEXT("gpt-image-2");
	}

	if (ApiKey.IsEmpty())
	{
		OnComplete.ExecuteIfBound(false, TArray<uint8>(), TEXT("GPT Image API key is not set. Add it in Project Settings > Plugins > Ascent AI Tools."));
		return;
	}

	const UACFAIToolsSettings* Adv = Settings;
	FString Endpoint = Adv ? Adv->OpenAIImageEndpoint : FString();
	if (Endpoint.IsEmpty())
	{
		Endpoint = TEXT("https://api.openai.com/v1/images/generations");
	}
	FString ImageSize = Adv ? Adv->ImageSize : FString();
	if (ImageSize.IsEmpty())
	{
		ImageSize = TEXT("1024x1024");
	}

	TSharedRef<FJsonObject> RootObj = MakeShared<FJsonObject>();
	RootObj->SetStringField(TEXT("model"), Model);
	RootObj->SetStringField(TEXT("prompt"), Prompt);
	RootObj->SetNumberField(TEXT("n"), 1);
	RootObj->SetStringField(TEXT("size"), ImageSize);
	// Note: gpt-image-* models always return base64 and reject the legacy
	// "response_format" parameter, so we must not send it.

	FString Body;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Body);
	FJsonSerializer::Serialize(RootObj, Writer);

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(Endpoint);
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *ApiKey));
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

			TSharedPtr<FJsonObject> Root;
			TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());
			if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
			{
				OnComplete.ExecuteIfBound(false, TArray<uint8>(),
					FString::Printf(TEXT("Invalid response from the image service (HTTP %d)."), Response->GetResponseCode()));
				return;
			}

			// Surface API errors with their message.
			const TSharedPtr<FJsonObject>* ErrorObj = nullptr;
			if (Root->TryGetObjectField(TEXT("error"), ErrorObj))
			{
				FString Message = TEXT("Image generation failed.");
				(*ErrorObj)->TryGetStringField(TEXT("message"), Message);
				OnComplete.ExecuteIfBound(false, TArray<uint8>(), Message);
				return;
			}

			const TArray<TSharedPtr<FJsonValue>>* Data = nullptr;
			if (!Root->TryGetArrayField(TEXT("data"), Data) || Data->Num() == 0)
			{
				OnComplete.ExecuteIfBound(false, TArray<uint8>(), TEXT("The image service returned no result."));
				return;
			}

			const TSharedPtr<FJsonObject> First = (*Data)[0]->AsObject();
			FString Base64Data;
			if (First.IsValid() && First->TryGetStringField(TEXT("b64_json"), Base64Data) && !Base64Data.IsEmpty())
			{
				TArray<uint8> ImageBytes;
				if (!FBase64::Decode(Base64Data, ImageBytes))
				{
					OnComplete.ExecuteIfBound(false, TArray<uint8>(), TEXT("Failed to decode the generated image."));
					return;
				}
				OnComplete.ExecuteIfBound(true, ImageBytes, FString());
				return;
			}

			// Fallback: some responses return a URL instead of inline base64; download it.
			FString ImageUrl;
			if (First.IsValid() && First->TryGetStringField(TEXT("url"), ImageUrl) && !ImageUrl.IsEmpty())
			{
				TSharedRef<IHttpRequest, ESPMode::ThreadSafe> DownloadReq = FHttpModule::Get().CreateRequest();
				DownloadReq->SetURL(ImageUrl);
				DownloadReq->SetVerb(TEXT("GET"));
				DownloadReq->SetTimeout(120.0f);
				DownloadReq->SetActivityTimeout(120.0f);
				DownloadReq->OnProcessRequestComplete().BindLambda(
					[OnComplete](FHttpRequestPtr, FHttpResponsePtr DlResponse, bool bDlOk)
					{
						if (!bDlOk || !DlResponse.IsValid() || DlResponse->GetContentLength() <= 0)
						{
							OnComplete.ExecuteIfBound(false, TArray<uint8>(), TEXT("Failed to download the generated image."));
							return;
						}
						OnComplete.ExecuteIfBound(true, DlResponse->GetContent(), FString());
					});
				DownloadReq->ProcessRequest();
				return;
			}

			OnComplete.ExecuteIfBound(false, TArray<uint8>(), TEXT("The response did not contain any image data."));
		});

	Request->ProcessRequest();
}
