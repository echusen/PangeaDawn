// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ThreeDBackend.h"
#include "Interfaces/IHttpRequest.h"
#include "Containers/Ticker.h"

/**
 * HTTP client for the Meshy 3D generation API. Handles the full asynchronous
 * lifecycle: create task -> poll progress -> (optional) refine -> (optional) rig -> download.
 * The instance keeps itself alive (via an internal shared reference) for the
 * duration of the operation, so callers may discard the returned pointer.
 */
class ASCENTAITOOLS_API FACFMeshyClient : public TSharedFromThis<FACFMeshyClient>
{
public:
	/** Generate a mesh from a text prompt. When bRig is true, auto-rigs onto a humanoid skeleton after texturing. */
	static TSharedPtr<FACFMeshyClient> TextTo3D(const FString& Prompt, bool bRig, float HeightMeters, FACF3DStatusDelegate OnStatus, FACF3DFinishedDelegate OnFinished);

	/** Generate a mesh from a (PNG) image. When bRig is true, auto-rigs onto a humanoid skeleton after texturing. */
	static TSharedPtr<FACFMeshyClient> ImageTo3D(const TArray<uint8>& PngBytes, bool bRig, float HeightMeters, FACF3DStatusDelegate OnStatus, FACF3DFinishedDelegate OnFinished);

	~FACFMeshyClient();

private:
	void CreateTask(const FString& JsonBody);
	void OnTaskCreated(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnected);
	void StartPolling();
	bool Poll(float DeltaTime);
	void OnPollResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnected);
	void StartRefine();
	void StartRigging();
	void OnRiggingTaskCreated(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnected);
	void DownloadModel(const FString& Url);
	void OnModelDownloaded(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnected);
	void Complete(bool bSuccess, const FACF3DMeshResult& Mesh, const FString& Error);
	void CompleteError(const FString& Error);

	void SetStatus(const FText& Text);

	FString ApiKey;
	/** Base endpoint for the current generation or rigging task, e.g. https://api.meshy.ai/openapi/v2/text-to-3d */
	FString Endpoint;
	FString TaskId;
	bool bIsImageTo3D = false;
	bool bRefinePending = false;
	bool bRigPending = false;
	bool bWantRig = false;
	float HeightMeters = 1.7f;
	float PollInterval = 5.0f;

	FACF3DStatusDelegate OnStatusDelegate;
	FACF3DFinishedDelegate OnFinishedDelegate;

	FTSTicker::FDelegateHandle PollHandle;

	/** Self-reference that keeps the client alive while the task runs. */
	TSharedPtr<FACFMeshyClient> KeepAlive;
};
