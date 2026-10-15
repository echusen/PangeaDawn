// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "ThreeDJob.h"

#include "AscentAIToolsSettings.h"
#include "Dom/JsonObject.h"
#include "HAL/PlatformTime.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#define LOCTEXT_NAMESPACE "ACF3DJob"

FACF3DJob::~FACF3DJob()
{
	StopPolling();
}

void FACF3DJob::Run(FACF3DStatusDelegate InOnStatus, FACF3DFinishedDelegate InOnFinished)
{
	OnStatus = MoveTemp(InOnStatus);
	OnFinished = MoveTemp(InOnFinished);
	KeepAlive = AsShared();
	StartSeconds = FPlatformTime::Seconds();

	if (const UACFAIToolsSettings* Settings = UACFAIToolsSettings::Get())
	{
		RequestTimeout = Settings->MeshRequestTimeout;
		DownloadTimeout = Settings->MeshDownloadTimeout;
		PollInterval = FMath::Max(Settings->MeshyPollInterval, 1.0f);
	}

	Begin();
}

void FACF3DJob::SetStatus(const FText& Status) const
{
	OnStatus.ExecuteIfBound(Status);
}

double FACF3DJob::ElapsedSeconds() const
{
	return FPlatformTime::Seconds() - StartSeconds;
}

void FACF3DJob::Fail(const FString& Error)
{
	Finish(false, FACF3DMeshResult(), Error);
}

void FACF3DJob::Finish(bool bSuccess, const FACF3DMeshResult& Mesh, const FString& Error)
{
	if (bFinished)
	{
		return;
	}
	bFinished = true;
	StopPolling();

	// Keep the job alive until the delegate returned, then let it go.
	const TSharedPtr<FACF3DJob> Hold = MoveTemp(KeepAlive);
	FACF3DFinishedDelegate Callback = MoveTemp(OnFinished);
	Callback.ExecuteIfBound(bSuccess, Mesh, Error);
}

void FACF3DJob::Send(FCall&& Call, FOnJson OnDone)
{
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(Call.Url);
	Request->SetVerb(Call.Verb);
	if (Call.bAuthorize)
	{
		Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *ApiKey));
	}
	for (const TPair<FString, FString>& Header : Call.Headers)
	{
		Request->SetHeader(Header.Key, Header.Value);
	}
	if (Call.Body.Num() > 0)
	{
		Request->SetContent(MoveTemp(Call.Body));
	}
	Request->SetTimeout(RequestTimeout);
	Request->SetActivityTimeout(RequestTimeout);

	const TWeakPtr<FACF3DJob> Weak = AsShared();
	Request->OnProcessRequestComplete().BindLambda(
		[Weak, OnDone = MoveTemp(OnDone)](FHttpRequestPtr, FHttpResponsePtr Response, bool bConnected)
		{
			const TSharedPtr<FACF3DJob> Self = Weak.Pin();
			if (!Self.IsValid() || Self->bFinished)
			{
				return;
			}
			if (!bConnected || !Response.IsValid())
			{
				OnDone(0, nullptr);
				return;
			}
			TSharedPtr<FJsonObject> Json;
			const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());
			if (!FJsonSerializer::Deserialize(Reader, Json))
			{
				Json.Reset();
			}
			OnDone(Response->GetResponseCode(), Json);
		});
	Request->ProcessRequest();
}

void FACF3DJob::SendJson(const FString& Verb, const FString& Url, const TSharedPtr<FJsonObject>& Body, FOnJson OnDone)
{
	FCall Call;
	Call.Verb = Verb;
	Call.Url = Url;
	if (Body.IsValid())
	{
		Call.Headers.Add(TEXT("Content-Type"), TEXT("application/json"));
		FString Text;
		const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Text);
		FJsonSerializer::Serialize(Body.ToSharedRef(), Writer);
		const FTCHARToUTF8 Utf8(*Text);
		Call.Body.Append(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
	}
	Send(MoveTemp(Call), MoveTemp(OnDone));
}

void FACF3DJob::StartPolling(TFunction<void()> InPollOnce)
{
	StopPolling();
	PollOnce = MoveTemp(InPollOnce);
	bPollInFlight = false;

	const TWeakPtr<FACF3DJob> Weak = AsShared();
	PollHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateLambda([Weak](float)
		{
			const TSharedPtr<FACF3DJob> Self = Weak.Pin();
			return Self.IsValid() && Self->TickPoll();
		}),
		PollInterval);

	// The first poll must not wait a whole interval.
	TickPoll();
}

bool FACF3DJob::TickPoll()
{
	if (bFinished || !PollOnce)
	{
		return false;
	}
	if (ElapsedSeconds() > MaxJobSeconds)
	{
		Fail(TEXT("The 3D service did not finish in time. The job was abandoned after 20 minutes."));
		return false;
	}
	if (!bPollInFlight)
	{
		bPollInFlight = true;
		PollOnce();
	}
	return true;
}

void FACF3DJob::StopPolling()
{
	if (PollHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(PollHandle);
		PollHandle.Reset();
	}
	PollOnce = nullptr;
	bPollInFlight = false;
}

void FACF3DJob::PollJson(const FString& Url, FOnJson OnDone)
{
	FCall Call;
	Call.Url = Url;
	Send(MoveTemp(Call), [this, OnDone = MoveTemp(OnDone)](int32 HttpCode, const TSharedPtr<FJsonObject>& Json)
	{
		bPollInFlight = false;
		OnDone(HttpCode, Json);
	});
}

void FACF3DJob::Download(const FString& Url, const FString& Extension, bool bRigged)
{
	StopPolling();
	SetStatus(LOCTEXT("Downloading", "Downloading 3D mesh..."));

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(Url);
	Request->SetVerb(TEXT("GET"));
	Request->SetTimeout(DownloadTimeout);
	Request->SetActivityTimeout(DownloadTimeout);

	const TWeakPtr<FACF3DJob> Weak = AsShared();
	Request->OnProcessRequestComplete().BindLambda(
		[Weak, Extension, bRigged](FHttpRequestPtr, FHttpResponsePtr Response, bool bConnected)
		{
			const TSharedPtr<FACF3DJob> Self = Weak.Pin();
			if (!Self.IsValid() || Self->bFinished)
			{
				return;
			}
			if (!bConnected || !Response.IsValid() || !EHttpResponseCodes::IsOk(Response->GetResponseCode()) || Response->GetContent().Num() == 0)
			{
				Self->Fail(TEXT("The finished 3D model could not be downloaded."));
				return;
			}
			FACF3DMeshResult Mesh;
			Mesh.Bytes = Response->GetContent();
			Mesh.Extension = Extension.IsEmpty() ? TEXT("glb") : Extension;
			Mesh.bRigged = bRigged;
			Self->Finish(true, Mesh, FString());
		});
	Request->ProcessRequest();
}

#undef LOCTEXT_NAMESPACE
