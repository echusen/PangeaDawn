// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NanoBananaClient.h" // for FACFOnImageReady (shared image-ready delegate)

/**
 * Thin HTTP client for the GPT Image generation backend.
 * Returns PNG bytes decoded from the base64 payload in the response.
 */
class ASCENTAITOOLS_API FACFGptImageClient
{
public:
	/** Generates an image from a text prompt. */
	static void GenerateImage(const FString& Prompt, FACFOnImageReady OnComplete);
};
