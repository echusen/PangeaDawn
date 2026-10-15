// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** Called when an image generation request completes. bSuccess + PNG bytes (or error message). */
DECLARE_DELEGATE_ThreeParams(FACFOnImageReady, bool /*bSuccess*/, const TArray<uint8>& /*PngBytes*/, const FString& /*Error*/);

/**
 * Thin HTTP client for the Google Gemini image generation model (a.k.a. "Nano Banana").
 * Uses the generateContent REST endpoint and decodes the inline base64 image from the response.
 */
class ASCENTAITOOLS_API FACFNanoBananaClient
{
public:
	/**
	 * Generates an image from a text prompt (and optionally a reference image to edit).
	 * @param Prompt              The text prompt.
	 * @param ReferenceImagePng   Optional PNG bytes used as input image (image editing). Can be empty.
	 * @param OnComplete          Delegate invoked on the game thread when finished.
	 */
	static void GenerateImage(const FString& Prompt, const TArray<uint8>& ReferenceImagePng, FACFOnImageReady OnComplete);
};
