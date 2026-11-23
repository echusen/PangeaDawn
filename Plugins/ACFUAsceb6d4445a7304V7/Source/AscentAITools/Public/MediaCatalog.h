// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * The one price list of the Yarin media service (GET {media root}/catalog, Bearer account key). Every amount
 * is in whole Yarin credits as the service computes them, margin included, so the UI never rescales it.
 *
 * The structures mirror the media3d catalog contract field for field. When a feature is not in the account's
 * plan the service answers with entitled:false, the plan that includes it, and EMPTY arrays: no names and no
 * prices leak to a plan that cannot use them.
 */

/** Liveness of the cached snapshot. */
enum class EACFMediaCatalogState : uint8
{
	/** No Yarin credential is stored: nothing is fetched and nothing is shown. */
	NotConnected,
	/** A fetch is in flight and no usable catalog exists yet. */
	Loading,
	/** A catalog body was parsed and is cached. */
	Ready,
	/** The last attempt failed; ErrorText says why in plain words. No stale prices are kept. */
	Failed
};

/** One billable generation shape of one provider, for example text-to-3D with textures. */
struct FACFCatalogLine
{
	/** Contract id: "text-mesh", "text-textured", "image-mesh", "image-textured" or "rig". */
	FString Id;
	/** Human label from the catalog ("Text to 3D with textures"). */
	FString Label;
	/** Margin inclusive credits one provider task of this line costs the user's plan. */
	int64 Credits = 0;
};

/** One 3D provider as the catalog offers it. */
struct FACFCatalogProvider
{
	/** Catalog id: "meshy", "tripo" or "wavespeed". */
	FString Id;
	FString Name;
	/**
	 * False when the service has no key for the provider or pricing disables it. The provider still appears
	 * with empty lines, so the UI can say it is not available right now.
	 */
	bool bAvailable = false;
	TArray<FACFCatalogLine> Lines;
};

/** One Yarin voice model (an ElevenLabs version behind a catalog id). */
struct FACFCatalogVoiceModel
{
	/** Catalog id: "yarin-tts", "yarin-tts-v3" or "yarin-tts-turbo". */
	FString Id;
	/** Plain name: "Expressive", "Studio" or "Fast". */
	FString Name;
	/** What sits behind the id, for example "ElevenLabs v4". */
	FString Detail;
	/** Margin inclusive credits per 1000 characters. */
	int64 CreditsPer1kChars = 0;
};

/** One curated Yarin voice. */
struct FACFCatalogVoice
{
	FString Id;
	FString Name;
	FString Description;
	/** Public preview url, empty when the catalog offers none. */
	FString PreviewUrl;
};

/** One parsed catalog body. Immutable content: refreshes publish a new one. */
struct FACFMediaCatalogSnapshot
{
	EACFMediaCatalogState State = EACFMediaCatalogState::NotConnected;
	/** Plain message of the last failure. Only set when State is Failed. */
	FString ErrorText;
	/** Platform seconds of the last completed fetch; 0 when nothing was ever fetched. */
	double FetchedAtSeconds = 0.0;

	// ---- The plan block ----

	/** The account's access tier, for example "tier-hobbyist". Empty when the account has none. */
	FString Tier;
	/** True when the account has a monthly cap (CapCredits is then valid). */
	bool bHasCap = false;
	int64 CapCredits = 0;
	/** Spent this month. */
	int64 SpendCredits = 0;
	/** True when the service reports a remaining amount (RemainingCredits is then valid). */
	bool bHasRemaining = false;
	int64 RemainingCredits = 0;
	/** True when billing refused the account (blocked). */
	bool bBlocked = false;

	// ---- Feature entitlement ----

	bool bThreeDEntitled = false;
	bool bVoiceEntitled = false;
	/** The plan that includes the features, shown even when they are not entitled ("Hobbyist"). */
	FString RequiredPlan;

	TArray<FACFCatalogProvider> ThreeDProviders;
	TArray<FACFCatalogVoiceModel> VoiceModels;
	TArray<FACFCatalogVoice> Voices;
};

bool operator==(const FACFCatalogLine& A, const FACFCatalogLine& B);
bool operator==(const FACFCatalogProvider& A, const FACFCatalogProvider& B);
bool operator==(const FACFCatalogVoiceModel& A, const FACFCatalogVoiceModel& B);
bool operator==(const FACFCatalogVoice& A, const FACFCatalogVoice& B);
bool operator==(const FACFMediaCatalogSnapshot& A, const FACFMediaCatalogSnapshot& B);

/**
 * The cached price list. Everything is static and thread safe: the snapshot is fetched asynchronously when
 * the Yarin credential is set, when a card opens with a stale one, or after a charged job, and is published
 * as a whole. There are no timers: subscribers act on OnChanged.
 *
 * Never logs and never stores the key outside the gateway holder it reads it from.
 */
class ASCENTAITOOLS_API FACFMediaCatalog
{
public:
	DECLARE_MULTICAST_DELEGATE(FACFMediaCatalogChanged);

	using FSnapshot = FACFMediaCatalogSnapshot;

	/**
	 * The last published snapshot. It is immutable and shared: reading it costs a reference count, never a copy,
	 * so it is cheap enough for Slate bindings that run every frame. A new fetch publishes a NEW snapshot and
	 * leaves this one untouched, so a pointer into it (FindLine, FindVoiceModel) stays valid exactly as long as the
	 * caller keeps the returned handle. Hold the handle in a local for as long as you use such a pointer; never
	 * take a pointer from a temporary.
	 */
	static TSharedRef<const FSnapshot> GetSnapshot();

	/**
	 * Fetches GET {media root}/catalog with the account key. Concurrent refreshes coalesce into the fetch
	 * already running; without bForce a Ready snapshot younger than the cache time makes this a no-op. Safe
	 * to call from any thread; subscribers fire on the game thread.
	 */
	static void Refresh(bool bForce = false);

	/** Broadcast on the game thread every time the published snapshot changes. */
	static FACFMediaCatalogChanged& OnChanged();

	/** Callers invoke after a job the gateway charged, so the balance refetches past the cache time. */
	static void NotifyJobFinished();

	/** Called by the gateway holder on Set and Clear: a clear publishes NotConnected at once, a set refreshes. */
	static void HandleGatewayChanged();

	/**
	 * Pure: folds one catalog body into OutSnapshot per the contract. Tolerates unknown extra fields and
	 * absent optional fields; a body that is not a JSON object fails with a plain OutError. Never reads
	 * globals and never touches the network, so tests drive it directly.
	 */
	static bool ParseCatalogJson(const FString& Body, FSnapshot& OutSnapshot, FString& OutError);

	/** "7,400 credits", "1 credit"; a negative amount reads as zero. */
	static FString FormatCredits(int64 Credits);

	/** "about 675 credits", "about 1 credit"; a negative amount reads as zero. */
	static FString FormatAboutCredits(int64 Credits);

	/** "7,400 credits left"; the chip and the balance line share this formatter and a negative amount reads as zero. */
	static FString FormatCreditsLeft(int64 Credits);

	/** "7,400 credits left this month"; a negative remaining amount reads as zero. */
	static FString FormatCreditsLeftThisMonth(int64 Credits);

	/** The line of one provider in the snapshot, or null when the provider or the line is absent. */
	static const FACFCatalogLine* FindLine(const FSnapshot& Snapshot, const FString& ProviderId, const FString& LineId);

	/** Test seam: replaces the HTTP fetch (url, bearer key, completion where code 0 means the network failed). Passing nullptr restores real HTTP. */
	static void SetFetchOverride(TUniqueFunction<void(const FString& Url, const FString& Key,
		TFunction<void(int32, const FString&)> Done)> InOverride);
};

/** State of the Yarin chip in the assistant panel header, produced by ComputeYarinChipState below. */
struct FACFYarinChipState
{
	/** The Yarin account is signed in (the media gateway holds a credential). */
	bool bConnected = false;
	/** The account's remaining credit is shown after the name. Only when connected and the catalog is Ready. */
	bool bShowsBalance = false;
	/** "Yarin  7,400 credits left", "Yarin" while Loading, Failed or without a remaining amount, "Connect Yarin" when signed out. */
	FString Text;
};

/**
 * Pure: the whole text and balance flag of the header chip from one Yarin connection bit and one snapshot, so
 * the panel carries no chip logic of its own. An amount appears only through a connected account, whatever the
 * plan includes: the user's remaining credit is theirs whether or not the plan has 3D or voice in it.
 */
ASCENTAITOOLS_API FACFYarinChipState ComputeYarinChipState(const bool bConnected, const FACFMediaCatalogSnapshot& Snapshot);
