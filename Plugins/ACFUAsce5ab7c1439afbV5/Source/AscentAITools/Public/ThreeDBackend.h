// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AscentAIToolsSettings.h"

/** Progress text from a running 3D job (for example "Tripo: generating 42%"). */
DECLARE_DELEGATE_OneParam(FACF3DStatusDelegate, const FText& /*Status*/);

/** Downloaded mesh bytes plus format metadata for the importer. */
struct FACF3DMeshResult
{
	TArray<uint8> Bytes;
	/** File extension without a leading dot, for example "glb" or "fbx". */
	FString Extension = TEXT("glb");
	/** True when Bytes are an auto-rigged character (FBX). */
	bool bRigged = false;
};

/** Finished delegate: success flag, downloaded mesh, error message. Runs exactly once per job. */
DECLARE_DELEGATE_ThreeParams(FACF3DFinishedDelegate, bool /*bSuccess*/, const FACF3DMeshResult& /*Mesh*/, const FString& /*Error*/);

/** One generation request. Set Prompt for text-to-3D, PngBytes for image-to-3D. */
struct FACF3DRequest
{
	FString Prompt;
	TArray<uint8> PngBytes;
	/** Auto-rig the result onto a humanoid skeleton. Only honoured when the backend reports bRigging. */
	bool bRig = false;
	/** Character height in meters for rigging. Zero or less uses the value from Project Settings. */
	float HeightMeters = 0.0f;
};

/** What a backend can do. UI and tools read this instead of switching on the provider. */
struct FACF3DCapabilities
{
	bool bTextTo3D = false;
	bool bImageTo3D = false;
	bool bRigging = false;
	/** Meshy art style (realistic / sculpture). */
	bool bArtStyle = false;
	/** Triangle or quad choice. */
	bool bTopology = false;
	/** Separate refine pass after the preview mesh. */
	bool bRefine = false;
};

/**
 * One 3D generation service. A backend starts an asynchronous job and returns at once. The job keeps
 * itself alive, reports progress through the status delegate and runs the finished delegate exactly once.
 */
class ASCENTAITOOLS_API IACF3DBackend
{
public:
	virtual ~IACF3DBackend() = default;

	virtual EACF3DProvider GetProvider() const = 0;
	virtual FText GetDisplayName() const = 0;
	virtual FACF3DCapabilities GetCapabilities() const = 0;

	virtual void TextTo3D(FACF3DRequest Request, FACF3DStatusDelegate OnStatus, FACF3DFinishedDelegate OnFinished) const = 0;
	virtual void ImageTo3D(FACF3DRequest Request, FACF3DStatusDelegate OnStatus, FACF3DFinishedDelegate OnFinished) const = 0;
};

namespace ACF3D
{
	/** The backend for a provider. Never null: every EACF3DProvider value has one. */
	ASCENTAITOOLS_API const IACF3DBackend& GetBackend(EACF3DProvider Provider);

	/** The backend selected in Project Settings. */
	ASCENTAITOOLS_API const IACF3DBackend& GetActiveBackend();
}
