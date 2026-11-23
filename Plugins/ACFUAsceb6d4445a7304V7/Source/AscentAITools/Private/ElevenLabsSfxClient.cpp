// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "ElevenLabsSfxClient.h"
#include "AscentAIToolsSettings.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

/** Endpoint from the settings, or the ElevenLabs one when the setting was emptied. */
static FString ResolveSfxEndpoint()
{
	const UACFAIToolsSettings* Settings = UACFAIToolsSettings::Get();
	FString Endpoint = Settings ? Settings->SfxEndpoint.TrimStartAndEnd() : FString();
	if (Endpoint.IsEmpty())
	{
		Endpoint = TEXT("https://api.elevenlabs.io/v1/sound-generation");
	}
	return Endpoint;
}

static FString ResolveSfxOutputFormat()
{
	const UACFAIToolsSettings* Settings = UACFAIToolsSettings::Get();
	const FString Format = Settings ? Settings->SfxOutputFormat.TrimStartAndEnd() : FString();
	return Format.IsEmpty() ? FString(TEXT("mp3_44100_128")) : Format;
}

/** A failed generation answers with the API's own message when it sent one: "Unknown model" is the SFX v3 access error. */
static FString ReadApiError(const FString& ResponseBody, int32 ResponseCode)
{
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);
	if (FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid())
	{
		const TSharedPtr<FJsonObject>* Detail = nullptr;
		FString Message;
		if (Root->TryGetObjectField(TEXT("detail"), Detail) && (*Detail)->TryGetStringField(TEXT("message"), Message) && !Message.IsEmpty())
		{
			return Message;
		}
		if (Root->TryGetStringField(TEXT("detail"), Message) && !Message.IsEmpty())
		{
			return Message;
		}
		if (Root->TryGetStringField(TEXT("message"), Message) && !Message.IsEmpty())
		{
			return Message;
		}
	}
	return FString::Printf(TEXT("The sound effects service returned HTTP %d."), ResponseCode);
}

FString FACFElevenLabsSfxClient::GetOutputFileExtension()
{
	const FString Format = ResolveSfxOutputFormat().ToLower();
	if (Format.StartsWith(TEXT("wav")) || Format.StartsWith(TEXT("pcm")))
	{
		return TEXT("wav");
	}
	return TEXT("mp3");
}

void FACFElevenLabsSfxClient::GenerateSoundEffect(const FACFSoundEffectRequest& Request, FACFOnSoundEffectReady OnComplete)
{
	const FString Prompt = Request.Prompt.TrimStartAndEnd();
	if (Prompt.IsEmpty())
	{
		OnComplete.ExecuteIfBound(false, TArray<uint8>(), TEXT("Describe the sound before generating it."));
		return;
	}

	const FString ApiKey = UACFAIToolsSettings::GetSfxApiKey();
	if (ApiKey.IsEmpty())
	{
		OnComplete.ExecuteIfBound(false, TArray<uint8>(),
			TEXT("The ElevenLabs API key is not set. Add it in Project Settings > Plugins > Ascent AI Tools > Sound Effects, ")
			TEXT("or set the Voice API Key in Ascent Dialogue Settings: sound effects share the voice key."));
		return;
	}

	const UACFAIToolsSettings* Settings = UACFAIToolsSettings::Get();

	const TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
	Payload->SetStringField(TEXT("text"), Prompt);
	Payload->SetStringField(TEXT("model_id"), UACFAIToolsSettings::GetSfxModel());
	Payload->SetBoolField(TEXT("loop"), Request.bLoop);
	if (Request.DurationSeconds > 0.0f)
	{
		// Outside 0.5 - 30 the API rejects the request; clamping keeps a rounded "2 seconds" ask working.
		Payload->SetNumberField(TEXT("duration_seconds"), FMath::Clamp(Request.DurationSeconds, 0.5f, 30.0f));
	}
	if (Request.PromptInfluence >= 0.0f)
	{
		Payload->SetNumberField(TEXT("prompt_influence"), FMath::Clamp(Request.PromptInfluence, 0.0f, 1.0f));
	}

	FString Body;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Body);
	FJsonSerializer::Serialize(Payload, Writer);

	const FString Endpoint = ResolveSfxEndpoint();
	const FString Url = FString::Printf(TEXT("%s%soutput_format=%s"),
		*Endpoint, Endpoint.Contains(TEXT("?")) ? TEXT("&") : TEXT("?"), *ResolveSfxOutputFormat());

	const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = FHttpModule::Get().CreateRequest();
	HttpRequest->SetURL(Url);
	HttpRequest->SetVerb(TEXT("POST"));
	HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	HttpRequest->SetHeader(TEXT("xi-api-key"), ApiKey);
	HttpRequest->SetContentAsString(Body);
	const float Timeout = Settings ? Settings->SfxRequestTimeout : 180.0f;
	HttpRequest->SetTimeout(Timeout);
	// Nothing comes back while the sound is being generated, so the idle timeout has to match.
	HttpRequest->SetActivityTimeout(Timeout);

	HttpRequest->OnProcessRequestComplete().BindLambda(
		[OnComplete](FHttpRequestPtr /*Req*/, FHttpResponsePtr Response, bool bConnectedSuccessfully)
		{
			if (!bConnectedSuccessfully || !Response.IsValid())
			{
				OnComplete.ExecuteIfBound(false, TArray<uint8>(), TEXT("Could not reach the sound effects service. Check your internet connection."));
				return;
			}

			const int32 ResponseCode = Response->GetResponseCode();
			if (ResponseCode < 200 || ResponseCode > 299)
			{
				OnComplete.ExecuteIfBound(false, TArray<uint8>(), ReadApiError(Response->GetContentAsString(), ResponseCode));
				return;
			}

			const TArray<uint8>& Audio = Response->GetContent();
			if (Audio.Num() == 0)
			{
				OnComplete.ExecuteIfBound(false, TArray<uint8>(), TEXT("The sound effects service returned no audio."));
				return;
			}

			OnComplete.ExecuteIfBound(true, Audio, FString());
		});

	HttpRequest->ProcessRequest();
}
