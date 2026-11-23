// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** Called when a sound effect generation completes. bSuccess + the encoded audio bytes (or error message). */
DECLARE_DELEGATE_ThreeParams(FACFOnSoundEffectReady, bool /*bSuccess*/, const TArray<uint8>& /*AudioBytes*/, const FString& /*Error*/);

/** One text-to-sound-effects request. Everything except the prompt is optional. */
struct ASCENTAITOOLS_API FACFSoundEffectRequest
{
	/** What the sound is: action, setting, atmosphere. SFX v3 follows a described scene better than a sound category. */
	FString Prompt;

	/** Length in seconds. 0 or less lets the model pick it from the prompt; a set value is clamped to 0.5 - 30. */
	float DurationSeconds = 0.0f;

	/** How strictly the model follows the prompt. Below 0 sends the API default (0.3); a set value is clamped to 0 - 1. */
	float PromptInfluence = -1.0f;

	/** Generates a sound that loops seamlessly, for ambiences and textures. */
	bool bLoop = false;
};

/**
 * Thin HTTP client for the ElevenLabs text-to-sound-effects endpoint (SFX v3).
 * Same account and API key as the dialogue voice generation: the key is resolved by
 * UACFAIToolsSettings::GetSfxApiKey, so a project that already voices dialogue needs no second key.
 */
class ASCENTAITOOLS_API FACFElevenLabsSfxClient
{
public:
	/**
	 * Generates one sound effect from a prompt. Asynchronous; OnComplete runs on the game thread.
	 * The audio comes back encoded in the configured output format (mp3 by default), ready to import.
	 */
	static void GenerateSoundEffect(const FACFSoundEffectRequest& Request, FACFOnSoundEffectReady OnComplete);

	/** File extension the configured output format decodes to ("mp3" or "wav"), for the import temp file. */
	static FString GetOutputFileExtension();
};
