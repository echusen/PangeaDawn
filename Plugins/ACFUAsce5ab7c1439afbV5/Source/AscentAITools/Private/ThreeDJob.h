// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ThreeDBackend.h"
#include "Containers/Ticker.h"

class FJsonObject;

/**
 * Shared lifecycle for the HTTP based 3D backends: self keeping lifetime, JSON requests, a poll loop that never
 * overlaps itself, the model download, and exactly one finish.
 *
 * A concrete job configures itself in its constructor, implements Begin() and calls Run(). HTTP callbacks reach the
 * job through a weak pointer, so a job that already finished ignores late responses, and subclass lambdas passed to
 * Send / PollJson may safely capture `this`.
 */
class FACF3DJob : public TSharedFromThis<FACF3DJob>
{
public:
	virtual ~FACF3DJob();

	/** Takes over the job's lifetime and starts it. The job frees itself once the finished delegate has run. */
	void Run(FACF3DStatusDelegate InOnStatus, FACF3DFinishedDelegate InOnFinished);

protected:
	/** Called once by Run(). Issue the first request. */
	virtual void Begin() = 0;

	struct FCall
	{
		FString Verb = TEXT("GET");
		FString Url;
		TMap<FString, FString> Headers;
		TArray<uint8> Body;
		/** Send the Authorization header. Off for storage uploads and CDN downloads: the API key goes to the API host only. */
		bool bAuthorize = true;
	};

	/** HttpCode is 0 when the service could not be reached. Json is null when the body was not JSON. */
	using FOnJson = TFunction<void(int32 HttpCode, const TSharedPtr<FJsonObject>& Json)>;

	void Send(FCall&& Call, FOnJson OnDone);
	void SendJson(const FString& Verb, const FString& Url, const TSharedPtr<FJsonObject>& Body, FOnJson OnDone);

	/** Polls immediately, then every PollInterval seconds. PollOnce must end in PollJson so overlapping polls are impossible. */
	void StartPolling(TFunction<void()> PollOnce);
	void StopPolling();
	void PollJson(const FString& Url, FOnJson OnDone);

	/** Downloads the finished model and finishes the job with it. */
	void Download(const FString& Url, const FString& Extension, bool bRigged);

	void SetStatus(const FText& Status) const;
	void Fail(const FString& Error);
	double ElapsedSeconds() const;

	/** Bearer key of the provider. Set by the subclass constructor. */
	FString ApiKey;

private:
	void Finish(bool bSuccess, const FACF3DMeshResult& Mesh, const FString& Error);
	bool TickPoll();

	/** Jobs are abandoned after this long so a stuck service cannot keep the UI busy forever. */
	static constexpr double MaxJobSeconds = 1200.0;

	FACF3DStatusDelegate OnStatus;
	FACF3DFinishedDelegate OnFinished;
	TFunction<void()> PollOnce;
	FTSTicker::FDelegateHandle PollHandle;
	TSharedPtr<FACF3DJob> KeepAlive;

	float RequestTimeout = 60.0f;
	float DownloadTimeout = 300.0f;
	float PollInterval = 5.0f;
	double StartSeconds = 0.0;
	bool bFinished = false;
	bool bPollInFlight = false;
};
