// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

/**
 * Pure helpers shared by the Tripo and WaveSpeed backends: response parsing and request building.
 * No network, no settings, no globals, so they can be unit tested directly.
 */
namespace ACF3D
{
	enum class EPollState : uint8
	{
		/** Keep polling. Also used for transient failures (rate limit, 5xx, unparsable body). */
		Pending,
		Succeeded,
		/** Terminal. Error is set. */
		Failed
	};

	struct FPollResult
	{
		EPollState State = EPollState::Pending;
		/** 0 to 100, or -1 when the service reports no progress. */
		int32 Progress = -1;
		/** Set when State is Succeeded. */
		FString ModelUrl;
		/** Set when State is Failed. */
		FString Error;
	};

	/** Tripo v3 GET /tasks/{id}. HttpCode is the transport status. Json may be null (body was not JSON). */
	FPollResult ParseTripoTask(int32 HttpCode, const TSharedPtr<FJsonObject>& Json);

	/** WaveSpeed GET /predictions/{id}/result. HttpCode is the transport status. Json may be null. */
	FPollResult ParseWaveSpeedResult(int32 HttpCode, const TSharedPtr<FJsonObject>& Json);

	/**
	 * Picks the model file out of a WaveSpeed outputs array. Items may be URL strings or objects holding URLs.
	 * Prefers a .glb, then a .gltf. Returns empty when the outputs hold no such file.
	 */
	FString PickModelUrl(const TArray<TSharedPtr<FJsonValue>>& Outputs);

	/** Lowercase file extension of a URL path without the dot. Query and fragment are ignored. Empty when there is none. */
	FString ExtensionFromUrl(const FString& Url);

	/** Builds a multipart/form-data body holding one file part. */
	void BuildMultipartFile(const FString& Boundary, const FString& FieldName, const FString& FileName, const FString& ContentType, const TArray<uint8>& Bytes, TArray<uint8>& OutBody);

	/** WaveSpeed accepts 40,000 to 1,500,000 faces. */
	int32 ClampWaveSpeedFaceCount(int32 Requested);

	/** The service's own error text (message, then error, then suggestion), or a generic line naming the HTTP code. */
	FString ExtractServiceError(const TSharedPtr<FJsonObject>& Json, int32 HttpCode);
}
