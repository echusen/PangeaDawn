// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ADSVoicePayer.h"

/**
 * Pays for dialogue voice generation from the signed-in Yarin credit through the gateway. Registered with the
 * dialogue editor by the module startup. Prices, plan access, qualities and the curated voice list all come from
 * the server catalog; nothing here knows a price.
 *
 * Whether it pays is the one billing rule of the voice card (ADSYarinVoice::ResolveVoiceSource). A chosen Yarin
 * source that cannot serve fails with a plain message and never hands the request to the own key.
 */
class FADSYarinVoicePayer : public IADSVoicePayer
{
public:
	virtual bool IsPaying() const override;
	virtual void RequestSpeech(const FString& VoiceId, const FString& Text, bool bInBatch,
		TFunction<void(bool bSuccess, const TArray<uint8>& Audio, const FString& Message)> OnDone) override;
	virtual EADSBatchVerdict ReviewBatch(int32 LineCount, int32 TotalChars, FString& OutRefusal) override;
	virtual void BatchFinished(int32 SuccessCount) override;
	virtual bool ListVoices(TArray<FADSPayerVoice>& OutVoices, FString& OutCacheLabel, FString& OutRefusal) override;
};
