// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "ADSYarinVoiceSettings.generated.h"

/** Whose key pays for dialogue voice generation. */
UENUM()
enum class EADSVoiceBillingSource : uint8
{
	/** The own ElevenLabs key when it is saved, otherwise the Yarin credit when Yarin is signed in and the plan includes voice. */
	Auto UMETA(DisplayName = "Auto"),
	/** Always the user's own ElevenLabs key. Never touches Yarin. */
	OwnKey UMETA(DisplayName = "My own key"),
	/** Always the signed-in Yarin account's credit. */
	Yarin UMETA(DisplayName = "Yarin credit")
};

/**
 * Editor settings of voice generation billed to the Yarin credit. They live here, in the module that provides the
 * Yarin payer, so the runtime dialogue module and a dialogue plugin without Yarin carry no trace of them. The
 * Generation settings window edits the same values.
 */
UCLASS(config = Plugins, DefaultConfig, meta = (DisplayName = "Ascent Dialogue Voice Billing"))
class ASCENTDIALOGUEYARIN_API UADSYarinVoiceSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }

	/**
	 * Whose key pays for voice generation: Auto prefers the saved own key, otherwise the signed-in Yarin
	 * credit when the plan includes voice.
	 */
	UPROPERTY(EditAnywhere, config, Category = "Voice Generation", meta = (DisplayName = "Voice Source"))
	EADSVoiceBillingSource VoiceBillingSource = EADSVoiceBillingSource::Auto;

	/** Yarin voice model id used when generation is billed to the Yarin credit, for example yarin-tts. */
	UPROPERTY(EditAnywhere, config, Category = "Voice Generation", meta = (DisplayName = "Yarin Voice Model"))
	FString YarinVoiceModelId = TEXT("yarin-tts");

	/**
	 * A Yarin batch estimated above this many credits asks for confirmation before it starts. Batches at or
	 * below it start silently. An estimate above the remaining credit is refused up front regardless.
	 */
	UPROPERTY(EditAnywhere, config, Category = "Voice Generation", meta = (DisplayName = "Yarin Batch Confirmation Threshold (credits)", ClampMin = "0"))
	int64 YarinBatchConfirmCredits = 500;

	EADSVoiceBillingSource GetVoiceBillingSource() const { return VoiceBillingSource; }
	FString GetYarinVoiceModelId() const { return YarinVoiceModelId; }
	int64 GetYarinBatchConfirmCredits() const { return YarinBatchConfirmCredits; }

	/** Setters used by the Generation settings window. */
	void SetVoiceBillingSource(const EADSVoiceBillingSource InSource) { VoiceBillingSource = InSource; }
	void SetYarinVoiceModelId(const FString& InModelId) { YarinVoiceModelId = InModelId; }
};
