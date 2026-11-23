// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ADSYarinVoiceSettings.h"
#include "MediaCatalog.h"

/** The source a voice generation actually uses after the billing rule. */
enum class EADSResolvedVoiceSource : uint8
{
	/** The user's own ElevenLabs key. */
	OwnKey,
	/** The signed-in Yarin account's credit through the gateway. */
	Yarin
};

/** What a Yarin batch does with its estimate. */
enum class EADSYarinBatchDecision : uint8
{
	/** At or below the confirmation threshold: start without asking. */
	Proceed,
	/** Above the threshold: ask the modal confirmation first. */
	Ask,
	/** Above the remaining credit: refuse up front, the batch would fail midway. */
	Refuse
};

/** One built gateway speech request. The key is carried ONLY in the Authorization header value. */
struct FADSYarinVoiceGatewayRequest
{
	/** POST target: the gateway base plus /audio/speech. */
	FString Url;
	/** "Bearer <gateway key>", dropped into the Authorization header. Never logged. */
	FString Authorization;
	/** JSON body: model, input, voice. */
	FString Body;
};

/**
 * Pure helpers of the Yarin voice path, shared by the generation section, the Yarin payer and the
 * automation tests. Nothing here reads globals or touches the network.
 */
namespace ADSYarinVoice
{
	/**
	 * The billing rule of the voice card, applied to generation: an explicit choice is honoured (even when
	 * it cannot serve, callers map the failure); Auto means the own key when one is saved, else the Yarin
	 * credit when Yarin is connected and the plan includes voice, else the own key.
	 */
	EADSResolvedVoiceSource ResolveVoiceSource(
		EADSVoiceBillingSource ExplicitSetting, bool bHasOwnKey, bool bYarinConnected, bool bVoiceEntitled);

	/** Pure builder of POST {gateway base}/audio/speech: JSON model, input, voice; Bearer authorization. */
	FADSYarinVoiceGatewayRequest BuildGatewaySpeechRequest(
		const FString& GatewayBaseUrl, const FString& GatewayKey, const FString& ModelId,
		const FString& VoiceId, const FString& Text);

	/**
	 * Plain notification wording of a gateway failure. 401 asks to sign in again, 403 names the plan that
	 * includes voice, 429 says the credit is used up with the Add credit hint, 5xx reports provider
	 * trouble. Anything else falls to a plain HTTP line.
	 */
	FString MapGatewayHttpError(int32 Code, const FString& RequiredPlan);

	/** Margin inclusive credits totalChars cost at creditsPer1kChars, rounded up to whole credits. */
	int64 EstimateBatchCredits(int32 TotalChars, int64 CreditsPer1kChars);

	/**
	 * The threshold rule: an estimate above the remaining credit (when the catalog reports one) refuses the
	 * batch up front; an estimate above the confirmation threshold asks; at or below the threshold it
	 * proceeds silently.
	 */
	EADSYarinBatchDecision DecideBatch(
		int64 EstimateCredits, int64 ThresholdCredits, bool bHasRemaining, int64 RemainingCredits);

	/** The modal wording when a batch is above the threshold ("Generate voice for 48 lines? about ..."). */
	FString FormatBatchConfirmMessage(int32 LineCount, int64 EstimateCredits, const FACFMediaCatalogSnapshot& Snapshot);

	/** The plain refusal wording when a batch exceeds the remaining credit. */
	FString FormatBatchRefuseMessage(int64 EstimateCredits, const FACFMediaCatalogSnapshot& Snapshot);

	/** The plain refusal wording when the Yarin voice catalog cannot serve a batch (not signed in, not entitled, model missing). */
	FString FormatBatchUnavailableMessage();

	/** True when VoiceId is one of the curated Yarin voices of the snapshot. */
	bool IsVoiceInCatalog(const TArray<FACFCatalogVoice>& Voices, const FString& VoiceId);

	/** The voice model with the id, or null when the snapshot has none. */
	const FACFCatalogVoiceModel* FindVoiceModel(const FACFMediaCatalogSnapshot& Snapshot, const FString& ModelId);
}
