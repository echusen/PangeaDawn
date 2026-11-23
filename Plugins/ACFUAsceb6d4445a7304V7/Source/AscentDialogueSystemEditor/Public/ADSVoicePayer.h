// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** One voice a payer offers for dialogue participants. */
struct FADSPayerVoice
{
	FString Name;
	FString Id;
};

/** What a payer decides about a batch that is about to start. */
enum class EADSBatchVerdict : uint8
{
	/** Start the batch. */
	Proceed,
	/** The payer cannot serve it; the reason is in the refusal text and is shown to the user. */
	Refused,
	/** The user was asked and said no; nothing more to show. */
	Declined
};

/**
 * An optional payer of voice generation other than the user's own key. A module that provides one registers it
 * at startup and the dialogue editor then routes generation to it while it IsPaying. The dialogue editor knows
 * nothing about what pays: no account, no catalog, no gateway. A plugin without any payer simply has the own key
 * path, exactly as before.
 */
class ASCENTDIALOGUESYSTEMEDITOR_API IADSVoicePayer
{
public:
	virtual ~IADSVoicePayer() = default;

	/**
	 * True when this payer pays for voice generation right now: the settings and the account agree. Cheap, it is
	 * asked per node and per refresh. A payer that is chosen but cannot serve still answers true and then fails
	 * the request with a clear message; generation never falls back to the own key behind the user's back.
	 */
	virtual bool IsPaying() const = 0;

	/**
	 * Generates one line. Called only while IsPaying. OnDone receives success, the audio bytes (valid for the call
	 * only) and, on failure, the plain reason. Asynchronous; the callback runs on the game thread.
	 * bInBatch is true inside a batch, where the payer settles its balance once at the end instead of per line.
	 */
	virtual void RequestSpeech(const FString& VoiceId, const FString& Text, bool bInBatch,
		TFunction<void(bool bSuccess, const TArray<uint8>& Audio, const FString& Message)> OnDone) = 0;

	/**
	 * Called before a batch starts, with the number of lines and the total characters to voice. The payer may
	 * ask the user (a modal) or refuse up front, for example when the batch would exceed the remaining credit.
	 */
	virtual EADSBatchVerdict ReviewBatch(int32 LineCount, int32 TotalChars, FString& OutRefusal) = 0;

	/** Called once after a batch that generated at least one line. */
	virtual void BatchFinished(int32 SuccessCount) = 0;

	/**
	 * The voices this payer offers. OutCacheLabel names the source in the voice cache; it is not a secret.
	 * Returns false with a plain reason in OutRefusal when the list cannot be served right now.
	 */
	virtual bool ListVoices(TArray<FADSPayerVoice>& OutVoices, FString& OutCacheLabel, FString& OutRefusal) = 0;
};

namespace ADSVoicePayers
{
	/** Registers a payer (module startup). One payer is supported; a second registration replaces the first. */
	ASCENTDIALOGUESYSTEMEDITOR_API void Register(const TSharedRef<IADSVoicePayer>& Payer);

	/** Removes the payer again (module shutdown). A payer that is not the registered one is ignored. */
	ASCENTDIALOGUESYSTEMEDITOR_API void Unregister(const TSharedRef<IADSVoicePayer>& Payer);

	/** The registered payer whether it is paying or not, else null. For diagnostics and for tests that swap it. */
	ASCENTDIALOGUESYSTEMEDITOR_API TSharedPtr<IADSVoicePayer> GetRegistered();

	/** The registered payer when it is paying right now, else null (the own key pays). */
	ASCENTDIALOGUESYSTEMEDITOR_API TSharedPtr<IADSVoicePayer> GetPaying();
}
