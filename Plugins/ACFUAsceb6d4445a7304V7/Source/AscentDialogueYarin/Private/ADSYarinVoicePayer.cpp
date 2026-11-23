// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "ADSYarinVoicePayer.h"

#include "ADSDialogueDeveloperSettings.h"
#include "ADSYarinVoice.h"
#include "ADSYarinVoiceSettings.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "MediaCatalog.h"
#include "MediaGateway.h"
#include "Misc/MessageDialog.h"

namespace
{
	/** What a failed request reports as audio: nothing. */
	const TArray<uint8>& NoAudio()
	{
		static const TArray<uint8> Empty;
		return Empty;
	}
}

bool FADSYarinVoicePayer::IsPaying() const
{
	const UADSYarinVoiceSettings* YarinSettings = GetDefault<UADSYarinVoiceSettings>();
	const UADSDialogueDeveloperSettings* DialogueSettings = GetDefault<UADSDialogueDeveloperSettings>();
	if (!YarinSettings || !DialogueSettings)
	{
		return false;
	}
	// A refcount on the shared snapshot, no copy of the catalog, however often this is asked.
	const TSharedRef<const FACFMediaCatalogSnapshot> Snapshot = FACFMediaCatalog::GetSnapshot();
	return ADSYarinVoice::ResolveVoiceSource(YarinSettings->GetVoiceBillingSource(),
		!DialogueSettings->GetTTSVoiceGenAPIKey().TrimStartAndEnd().IsEmpty(),
		FACFMediaGateway::IsPresent(), Snapshot->bVoiceEntitled) == EADSResolvedVoiceSource::Yarin;
}

void FADSYarinVoicePayer::RequestSpeech(const FString& VoiceId, const FString& Text, const bool bInBatch,
	TFunction<void(bool bSuccess, const TArray<uint8>& Audio, const FString& Message)> OnDone)
{
	const UADSYarinVoiceSettings* Settings = GetDefault<UADSYarinVoiceSettings>();
	// The handle keeps the snapshot alive for every pointer taken from it below (FindVoiceModel).
	const TSharedRef<const FACFMediaCatalogSnapshot> SnapshotHandle = FACFMediaCatalog::GetSnapshot();
	const FACFMediaCatalogSnapshot& Snapshot = *SnapshotHandle;

	if (!FACFMediaGateway::IsPresent())
	{
		OnDone(false, NoAudio(), TEXT("Sign in to Yarin to bill voice generation to your credit."));
		return;
	}
	if (!Snapshot.bVoiceEntitled)
	{
		OnDone(false, NoAudio(), ADSYarinVoice::MapGatewayHttpError(403, Snapshot.RequiredPlan));
		return;
	}
	if (!ADSYarinVoice::FindVoiceModel(Snapshot, Settings->GetYarinVoiceModelId()))
	{
		OnDone(false, NoAudio(), FString::Printf(
			TEXT("The Yarin catalog does not offer the model \"%s\" right now. Pick an offered model in the Generation settings, then generate again."),
			*Settings->GetYarinVoiceModelId()));
		return;
	}
	// A voice outside the curated Yarin list (typically the user's own cloned voice) only exists in the
	// user's own ElevenLabs account: refuse here, the request is never sent anywhere.
	if (!ADSYarinVoice::IsVoiceInCatalog(Snapshot.Voices, VoiceId))
	{
		OnDone(false, NoAudio(), TEXT("This participant voice is not available through Yarin: it only exists in your own ElevenLabs account. Pick a Yarin voice for this participant or switch voice generation to My own key."));
		return;
	}

	const FACFMediaGateway::FCredential Credential = FACFMediaGateway::GetCredential();
	const FADSYarinVoiceGatewayRequest RequestData = ADSYarinVoice::BuildGatewaySpeechRequest(
		Credential.BaseUrl, Credential.Key, Settings->GetYarinVoiceModelId(), VoiceId, Text);

	const TSharedRef<IHttpRequest> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(RequestData.Url);
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	// The key travels only here, never in a URL, a log line or a message.
	Request->SetHeader(TEXT("Authorization"), RequestData.Authorization);
	Request->SetContentAsString(RequestData.Body);

	// Only the plan name is needed by the completion handler, so only it is captured, not the whole snapshot.
	const FString RequiredPlan = Snapshot.RequiredPlan;
	Request->OnProcessRequestComplete().BindLambda(
		[RequiredPlan, bInBatch, OnDone](FHttpRequestPtr /*Request*/, FHttpResponsePtr Response, const bool bWasSuccessful)
		{
			if (!(bWasSuccessful && Response.IsValid() && Response->GetResponseCode() == 200))
			{
				const int32 Code = Response.IsValid() ? Response->GetResponseCode() : 0;
				OnDone(false, NoAudio(), Code == 0
					? FString(TEXT("Request failed (no response from the Yarin gateway)."))
					: ADSYarinVoice::MapGatewayHttpError(Code, RequiredPlan));
				return;
			}

			// The gateway has charged the credit by now, whatever happens to the audio afterwards: settle the
			// balance right away for a single line. A batch settles once at its end (BatchFinished).
			if (!bInBatch)
			{
				FACFMediaCatalog::NotifyJobFinished();
			}
			OnDone(true, Response->GetContent(), FString());
		});

	Request->ProcessRequest();
}

EADSBatchVerdict FADSYarinVoicePayer::ReviewBatch(const int32 LineCount, const int32 TotalChars, FString& OutRefusal)
{
	const UADSYarinVoiceSettings* Settings = GetDefault<UADSYarinVoiceSettings>();
	const TSharedRef<const FACFMediaCatalogSnapshot> SnapshotHandle = FACFMediaCatalog::GetSnapshot();
	const FACFMediaCatalogSnapshot& Snapshot = *SnapshotHandle;

	if (!FACFMediaGateway::IsPresent())
	{
		OutRefusal = TEXT("Sign in to Yarin to bill voice generation to your credit.");
		return EADSBatchVerdict::Refused;
	}
	if (!Snapshot.bVoiceEntitled)
	{
		OutRefusal = ADSYarinVoice::MapGatewayHttpError(403, Snapshot.RequiredPlan);
		return EADSBatchVerdict::Refused;
	}
	const FACFCatalogVoiceModel* Model = ADSYarinVoice::FindVoiceModel(Snapshot, Settings->GetYarinVoiceModelId());
	if (!Model)
	{
		OutRefusal = ADSYarinVoice::FormatBatchUnavailableMessage();
		return EADSBatchVerdict::Refused;
	}

	// Refused up front when the credit cannot cover the batch, confirmed with a modal above the settings
	// threshold, silent otherwise. The estimate is the catalog price, margin included.
	const int64 Estimate = ADSYarinVoice::EstimateBatchCredits(TotalChars, Model->CreditsPer1kChars);
	switch (ADSYarinVoice::DecideBatch(Estimate, Settings->GetYarinBatchConfirmCredits(), Snapshot.bHasRemaining, Snapshot.RemainingCredits))
	{
	case EADSYarinBatchDecision::Refuse:
		OutRefusal = ADSYarinVoice::FormatBatchRefuseMessage(Estimate, Snapshot);
		return EADSBatchVerdict::Refused;
	case EADSYarinBatchDecision::Ask:
		return FMessageDialog::Open(EAppMsgType::YesNo,
			FText::FromString(ADSYarinVoice::FormatBatchConfirmMessage(LineCount, Estimate, Snapshot))) == EAppReturnType::Yes
			? EADSBatchVerdict::Proceed : EADSBatchVerdict::Declined;
	case EADSYarinBatchDecision::Proceed:
	default:
		return EADSBatchVerdict::Proceed;
	}
}

void FADSYarinVoicePayer::BatchFinished(const int32 SuccessCount)
{
	// One balance refresh for the whole charged batch; the per line requests skipped it.
	if (SuccessCount > 0)
	{
		FACFMediaCatalog::NotifyJobFinished();
	}
}

bool FADSYarinVoicePayer::ListVoices(TArray<FADSPayerVoice>& OutVoices, FString& OutCacheLabel, FString& OutRefusal)
{
	if (!FACFMediaGateway::IsPresent())
	{
		OutRefusal = TEXT("Sign in to Yarin to load its voices.");
		return false;
	}

	const TSharedRef<const FACFMediaCatalogSnapshot> SnapshotHandle = FACFMediaCatalog::GetSnapshot();
	const FACFMediaCatalogSnapshot& Snapshot = *SnapshotHandle;
	if (Snapshot.State != EACFMediaCatalogState::Ready)
	{
		OutRefusal = TEXT("The Yarin voice catalog is not available right now. Try again in a moment.");
		return false;
	}
	if (!Snapshot.bVoiceEntitled)
	{
		OutRefusal = ADSYarinVoice::MapGatewayHttpError(403, Snapshot.RequiredPlan);
		return false;
	}

	OutVoices.Reset();
	for (const FACFCatalogVoice& Voice : Snapshot.Voices)
	{
		OutVoices.Add(FADSPayerVoice{ Voice.Name, Voice.Id });
	}
	// A label for the voice cache, never the gateway key: the cache lives in the runtime subsystem.
	OutCacheLabel = TEXT("yarin-credit");
	return true;
}
