// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "ADSYarinVoice.h"

#include "ADSYarinVoiceSettings.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

// File local helpers.
namespace
{
FString AdsWithoutTrailingSlash(FString Url)
{
	while (Url.EndsWith(TEXT("/")))
	{
		Url.LeftChopInline(1);
	}
	return Url;
}
} // namespace

EADSResolvedVoiceSource ADSYarinVoice::ResolveVoiceSource(
	const EADSVoiceBillingSource ExplicitSetting, const bool bHasOwnKey, const bool bYarinConnected, const bool bVoiceEntitled)
{
	switch (ExplicitSetting)
	{
	case EADSVoiceBillingSource::Yarin:
		return EADSResolvedVoiceSource::Yarin;
	case EADSVoiceBillingSource::OwnKey:
		return EADSResolvedVoiceSource::OwnKey;
	case EADSVoiceBillingSource::Auto:
	default:
		if (bHasOwnKey)
		{
			return EADSResolvedVoiceSource::OwnKey;
		}
		return (bYarinConnected && bVoiceEntitled) ? EADSResolvedVoiceSource::Yarin : EADSResolvedVoiceSource::OwnKey;
	}
}

FADSYarinVoiceGatewayRequest ADSYarinVoice::BuildGatewaySpeechRequest(
	const FString& GatewayBaseUrl, const FString& GatewayKey, const FString& ModelId,
	const FString& VoiceId, const FString& Text)
{
	FADSYarinVoiceGatewayRequest Out;
	Out.Url = AdsWithoutTrailingSlash(GatewayBaseUrl) + TEXT("/audio/speech");
	Out.Authorization = TEXT("Bearer ") + GatewayKey;

	const TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("model"), ModelId);
	Body->SetStringField(TEXT("input"), Text);
	Body->SetStringField(TEXT("voice"), VoiceId);
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out.Body);
	FJsonSerializer::Serialize(Body.ToSharedRef(), Writer);

	return Out;
}

FString ADSYarinVoice::MapGatewayHttpError(const int32 Code, const FString& RequiredPlan)
{
	if (Code == 401)
	{
		return TEXT("Your Yarin sign in has expired or the gateway key is no longer valid. Sign in to Yarin again.");
	}
	if (Code == 403)
	{
		if (!RequiredPlan.IsEmpty())
		{
			return FString::Printf(TEXT("Your Yarin plan does not include voice generation. It is included from %s."), *RequiredPlan);
		}
		return TEXT("Your Yarin plan does not include voice generation. Upgrade your plan to use it.");
	}
	if (Code == 429)
	{
		return TEXT("Your Yarin credit is used up. Add credit in the Yarin panel or switch this generation to My own key.");
	}
	if (Code >= 500 && Code <= 599)
	{
		return TEXT("The Yarin voice service is having trouble right now. Try again in a moment.");
	}
	return FString::Printf(TEXT("Voice generation failed (HTTP %d)."), Code);
}

int64 ADSYarinVoice::EstimateBatchCredits(const int32 TotalChars, const int64 CreditsPer1kChars)
{
	const int64 Thousandths = FMath::Max<int64>(TotalChars, 0) * FMath::Max<int64>(CreditsPer1kChars, 0);
	return (Thousandths + 999) / 1000;
}

EADSYarinBatchDecision ADSYarinVoice::DecideBatch(
	const int64 EstimateCredits, const int64 ThresholdCredits, const bool bHasRemaining, const int64 RemainingCredits)
{
	if (bHasRemaining && EstimateCredits > RemainingCredits)
	{
		return EADSYarinBatchDecision::Refuse;
	}
	if (EstimateCredits > ThresholdCredits)
	{
		return EADSYarinBatchDecision::Ask;
	}
	return EADSYarinBatchDecision::Proceed;
}

FString ADSYarinVoice::FormatBatchConfirmMessage(const int32 LineCount, const int64 EstimateCredits, const FACFMediaCatalogSnapshot& Snapshot)
{
	if (Snapshot.bHasRemaining)
	{
		return FString::Printf(TEXT("Generate voice for %d lines? %s from your Yarin plan, %s."),
			LineCount, *FACFMediaCatalog::FormatAboutCredits(EstimateCredits), *FACFMediaCatalog::FormatCreditsLeftThisMonth(Snapshot.RemainingCredits));
	}
	return FString::Printf(TEXT("Generate voice for %d lines? %s from your Yarin plan."),
		LineCount, *FACFMediaCatalog::FormatAboutCredits(EstimateCredits));
}

FString ADSYarinVoice::FormatBatchRefuseMessage(const int64 EstimateCredits, const FACFMediaCatalogSnapshot& Snapshot)
{
	if (Snapshot.bHasRemaining)
	{
		return FString::Printf(TEXT("Not enough Yarin credits left for this batch: it would cost %s, %s. Add credit in the Yarin panel or switch voice generation to My own key."),
			*FACFMediaCatalog::FormatAboutCredits(EstimateCredits), *FACFMediaCatalog::FormatCreditsLeftThisMonth(Snapshot.RemainingCredits));
	}
	return FString::Printf(TEXT("Not enough Yarin credits left for this batch: it would cost %s. Add credit in the Yarin panel or switch voice generation to My own key."),
		*FACFMediaCatalog::FormatAboutCredits(EstimateCredits));
}

FString ADSYarinVoice::FormatBatchUnavailableMessage()
{
	return TEXT("The Yarin voice catalog is not available right now: sign in to Yarin, make sure your plan includes voice, then try again.");
}

bool ADSYarinVoice::IsVoiceInCatalog(const TArray<FACFCatalogVoice>& Voices, const FString& VoiceId)
{
	for (const FACFCatalogVoice& Voice : Voices)
	{
		if (Voice.Id == VoiceId)
		{
			return true;
		}
	}
	return false;
}

const FACFCatalogVoiceModel* ADSYarinVoice::FindVoiceModel(const FACFMediaCatalogSnapshot& Snapshot, const FString& ModelId)
{
	for (const FACFCatalogVoiceModel& Model : Snapshot.VoiceModels)
	{
		if (Model.Id == ModelId)
		{
			return &Model;
		}
	}
	return nullptr;
}
