// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "MediaCatalog.h"
#include "MediaGateway.h"
#include "GenerationSections.h"
#include "ThreeDGenerationSection.h"
#include "AscentAIToolsSettings.h"
#include "Misc/AutomationTest.h"

namespace
{
	/** The catalog of a hobbyist account entitled to everything, with margin inclusive prices. */
	FString EntitledCatalogBody()
	{
		return TEXT(R"---({
	"version": 1,
	"plan": { "tier": "tier-hobbyist", "capUsd": 10.0, "spendUsd": 2.6, "remainingUsd": 7.4, "capCredits": 10000, "spendCredits": 2600, "remainingCredits": 7400, "blocked": false },
	"requiredPlan": "Hobbyist",
	"threeD": { "entitled": true, "providers": [
		{ "id": "meshy", "name": "Meshy", "available": true, "lines": [
			{ "id": "text-mesh", "label": "Text to 3D", "usd": 0.30, "credits": 300 },
			{ "id": "text-textured", "label": "Text to 3D with textures", "usd": 0.60, "credits": 600 },
			{ "id": "image-mesh", "label": "Image to 3D", "usd": 0.30, "credits": 300 },
			{ "id": "image-textured", "label": "Image to 3D with textures", "usd": 0.60, "credits": 600 },
			{ "id": "rig", "label": "Auto rigging", "usd": 0.15, "credits": 150 } ] },
		{ "id": "tripo", "name": "Tripo", "available": true, "lines": [
			{ "id": "text-mesh", "label": "Text to 3D", "usd": 0.25, "credits": 250 } ] },
		{ "id": "wavespeed", "name": "WaveSpeed", "available": false, "lines": [] } ] },
	"voice": { "entitled": true,
		"models": [
			{ "id": "yarin-tts", "name": "Expressive", "detail": "ElevenLabs v4", "usdPer1kChars": 0.27, "creditsPer1kChars": 270 },
			{ "id": "yarin-tts-v3", "name": "Studio", "detail": "ElevenLabs v3", "usdPer1kChars": 0.27, "creditsPer1kChars": 270 } ],
		"voices": [
			{ "id": "voice-1", "name": "Alice", "description": "Warm", "previewUrl": "https://cdn.example.com/a.mp3" },
			{ "id": "voice-2", "name": "Bob", "description": "Deep", "previewUrl": null } ] }
})---");
	}

	/** The catalog of an account whose plan includes none of it: empty arrays, no prices, the plan named. */
	FString LockedCatalogBody()
	{
		return TEXT(R"---({
	"version": 1,
	"plan": { "tier": null, "capUsd": null, "spendUsd": 0, "remainingUsd": null, "capCredits": null, "spendCredits": 0, "remainingCredits": null, "blocked": false },
	"requiredPlan": "Hobbyist",
	"threeD": { "entitled": false, "providers": [] },
	"voice": { "entitled": false, "models": [], "voices": [] }
})---");
	}

	/** Parses a body as a ready snapshot, failing the test when the parse fails. */
	FACFMediaCatalogSnapshot ReadySnapshotOf(FAutomationTestBase& Test, const FString& Body)
	{
		FACFMediaCatalogSnapshot Snapshot;
		FString Error;
		const bool bParsed = FACFMediaCatalog::ParseCatalogJson(Body, Snapshot, Error);
		Test.TestTrue(TEXT("The catalog body parses"), bParsed);
		Test.TestTrue(TEXT("A parsed body carries no parse error"), Error.IsEmpty());
		Snapshot.State = EACFMediaCatalogState::Ready;
		return Snapshot;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAscentMediaCatalogParseTest, "Ascent.AITools.MediaCatalog.Parse",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAscentMediaCatalogParseTest::RunTest(const FString& Parameters)
{
	// An entitled catalog parses into providers, lines, voice models and voices.
	FACFMediaCatalogSnapshot Snapshot;
	FString Error;
	TestTrue(TEXT("The entitled catalog body parses"), FACFMediaCatalog::ParseCatalogJson(EntitledCatalogBody(), Snapshot, Error));
	TestTrue(TEXT("No parse error for a valid body"), Error.IsEmpty());
	TestTrue(TEXT("3D is entitled"), Snapshot.bThreeDEntitled);
	TestTrue(TEXT("Voice is entitled"), Snapshot.bVoiceEntitled);
	TestEqual(TEXT("The plan tier is read"), Snapshot.Tier, FString(TEXT("tier-hobbyist")));
	TestTrue(TEXT("The cap is reported"), Snapshot.bHasCap);
	TestEqual(TEXT("The cap is read in credits"), Snapshot.CapCredits, static_cast<int64>(10000));
	TestEqual(TEXT("The spend is read in credits"), Snapshot.SpendCredits, static_cast<int64>(2600));
	TestTrue(TEXT("The remaining amount is reported"), Snapshot.bHasRemaining);
	TestEqual(TEXT("The remaining amount is read in credits"), Snapshot.RemainingCredits, static_cast<int64>(7400));
	TestFalse(TEXT("The account is not blocked"), Snapshot.bBlocked);
	TestEqual(TEXT("The required plan is read"), Snapshot.RequiredPlan, FString(TEXT("Hobbyist")));
	TestEqual(TEXT("Three providers are listed"), Snapshot.ThreeDProviders.Num(), 3);
	TestEqual(TEXT("Meshy keeps its five lines"), Snapshot.ThreeDProviders[0].Lines.Num(), 5);
	TestTrue(TEXT("Meshy is available"), Snapshot.ThreeDProviders[0].bAvailable);
	TestTrue(TEXT("WaveSpeed is listed but unavailable"), !Snapshot.ThreeDProviders[2].bAvailable);
	TestEqual(TEXT("A line price is read in credits as sent"), Snapshot.ThreeDProviders[0].Lines[0].Credits, static_cast<int64>(300));
	TestEqual(TEXT("The voice models are listed"), Snapshot.VoiceModels.Num(), 2);
	TestEqual(TEXT("A voice model price is read in credits"), Snapshot.VoiceModels[0].CreditsPer1kChars, static_cast<int64>(270));
	TestEqual(TEXT("The voices are listed"), Snapshot.Voices.Num(), 2);
	TestEqual(TEXT("A voice preview url is kept"), Snapshot.Voices[0].PreviewUrl, FString(TEXT("https://cdn.example.com/a.mp3")));
	TestTrue(TEXT("An explicit null preview url reads as none"), Snapshot.Voices[1].PreviewUrl.IsEmpty());

	// FindLine resolves provider and line by id and misses cleanly.
	const FACFCatalogLine* MeshyTextMesh = FACFMediaCatalog::FindLine(Snapshot, TEXT("meshy"), TEXT("text-mesh"));
	TestNotNull(TEXT("Meshy's text-to-3D line is found"), MeshyTextMesh);
	if (MeshyTextMesh)
	{
		TestEqual(TEXT("The found line carries its price"), MeshyTextMesh->Credits, static_cast<int64>(300));
	}
	TestNotNull(TEXT("Tripo's text-to-3D line is found"), FACFMediaCatalog::FindLine(Snapshot, TEXT("tripo"), TEXT("text-mesh")));
	TestTrue(TEXT("An unknown line misses"), FACFMediaCatalog::FindLine(Snapshot, TEXT("tripo"), TEXT("rig")) == nullptr);
	TestTrue(TEXT("An unknown provider misses"), FACFMediaCatalog::FindLine(Snapshot, TEXT("sketchfab"), TEXT("text-mesh")) == nullptr);

	// A locked plan keeps the required plan name but no prices and no names.
	FACFMediaCatalogSnapshot Locked;
	FString LockedError;
	TestTrue(TEXT("The locked catalog body parses"), FACFMediaCatalog::ParseCatalogJson(LockedCatalogBody(), Locked, LockedError));
	TestTrue(TEXT("No parse error for a locked body"), LockedError.IsEmpty());
	TestFalse(TEXT("3D is not entitled"), Locked.bThreeDEntitled);
	TestFalse(TEXT("Voice is not entitled"), Locked.bVoiceEntitled);
	TestEqual(TEXT("The locked body still names the required plan"), Locked.RequiredPlan, FString(TEXT("Hobbyist")));
	TestTrue(TEXT("A locked body lists no providers"), Locked.ThreeDProviders.IsEmpty());
	TestTrue(TEXT("A locked body lists no voice models"), Locked.VoiceModels.IsEmpty());
	TestTrue(TEXT("A locked body lists no voices"), Locked.Voices.IsEmpty());
	TestFalse(TEXT("A null cap reads as no cap"), Locked.bHasCap);
	TestFalse(TEXT("A null remaining reads as none"), Locked.bHasRemaining);
	TestEqual(TEXT("A null cap leaves no credit amount"), Locked.CapCredits, static_cast<int64>(0));
	TestEqual(TEXT("A null remaining leaves no credit amount"), Locked.RemainingCredits, static_cast<int64>(0));

	FACFMediaCatalogSnapshot UsdOnly;
	FString UsdOnlyError;
	TestTrue(TEXT("A body with dollar fields only parses"), FACFMediaCatalog::ParseCatalogJson(
		TEXT("{\"version\":1,\"plan\":{\"capUsd\":10,\"remainingUsd\":7.4}}"), UsdOnly, UsdOnlyError));
	TestFalse(TEXT("Dollar fields never stand in for a credit cap"), UsdOnly.bHasCap);
	TestFalse(TEXT("Dollar fields never stand in for credits left"), UsdOnly.bHasRemaining);

	// Missing fields keep their defaults and the parse still succeeds.
	FACFMediaCatalogSnapshot Sparse;
	FString SparseError;
	TestTrue(TEXT("A body without any known field parses"), FACFMediaCatalog::ParseCatalogJson(TEXT("{\"version\":1}"), Sparse, SparseError));
	TestTrue(TEXT("No parse error for a sparse body"), SparseError.IsEmpty());
	TestFalse(TEXT("A sparse body entitles nothing"), Sparse.bThreeDEntitled || Sparse.bVoiceEntitled);
	TestTrue(TEXT("A sparse body lists nothing"), Sparse.ThreeDProviders.IsEmpty() && Sparse.VoiceModels.IsEmpty());

	// A body that is not a JSON object fails with a plain message and empties the snapshot.
	for (const TCHAR* Malformed : { TEXT("not json"), TEXT("[]"), TEXT("42"), TEXT("") })
	{
		FACFMediaCatalogSnapshot Broken;
		FString BrokenError;
		Broken.ThreeDProviders.Add(FACFCatalogProvider());
		TestFalse(FString::Printf(TEXT("A malformed body (%s) fails"), Malformed),
			FACFMediaCatalog::ParseCatalogJson(Malformed, Broken, BrokenError));
		TestFalse(TEXT("A malformed body explains itself"), BrokenError.IsEmpty());
		TestTrue(TEXT("A malformed body empties the snapshot"), Broken.ThreeDProviders.IsEmpty());
	}

	// Unknown extra fields are tolerated, including inside known objects.
	FACFMediaCatalogSnapshot Extra;
	FString ExtraError;
	TestTrue(TEXT("Unknown extra fields are tolerated"), FACFMediaCatalog::ParseCatalogJson(
		TEXT("{\"version\":1,\"future\":\"yes\",\"threeD\":{\"entitled\":true,\"providers\":[{\"id\":\"meshy\",\"name\":\"Meshy\",\"available\":true,\"lines\":[],\"later\":2}]},\"voice\":{\"entitled\":false}}"),
		Extra, ExtraError));
	TestTrue(TEXT("No parse error for extra fields"), ExtraError.IsEmpty());
	TestTrue(TEXT("Known fields still parse next to extra ones"), Extra.bThreeDEntitled && Extra.ThreeDProviders.Num() == 1);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAscentMediaCatalogStateTest, "Ascent.AITools.MediaCatalog.States",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAscentMediaCatalogStateTest::RunTest(const FString& Parameters)
{
	// The fetch is intercepted: requests queue here and complete when the test says so.
	TArray<FString> FetchedUrls;
	TArray<TFunction<void(int32, const FString&)>> Pending;
	FACFMediaCatalog::SetFetchOverride([&FetchedUrls, &Pending](const FString& Url, const FString& /*Key*/,
		TFunction<void(int32, const FString&)> Done)
	{
		FetchedUrls.Add(Url);
		Pending.Add(MoveTemp(Done));
	});

	// Start signed out, whatever another test left behind.
	FACFMediaGateway::Clear();
	FACFMediaCatalogSnapshot Snapshot = *FACFMediaCatalog::GetSnapshot();
	TestTrue(TEXT("Signed out is NotConnected"), Snapshot.State == EACFMediaCatalogState::NotConnected);
	TestTrue(TEXT("Signed out lists no prices"), Snapshot.ThreeDProviders.IsEmpty());

	// Sign in: one fetch is issued at once, against the media catalog, and the card shows loading.
	const FString FirstKey(TEXT("sk-test-first"));
	TestTrue(TEXT("A loopback gateway url is accepted"), FACFMediaGateway::Set(TEXT("http://127.0.0.1/litellm/v1"), FirstKey));
	TestEqual(TEXT("One fetch issued on sign in"), FetchedUrls.Num(), 1);
	TestEqual(TEXT("The fetch targets the media catalog"), FetchedUrls[0], FACFMediaGateway::GetMediaRoot() + TEXT("/catalog"));
	Snapshot = *FACFMediaCatalog::GetSnapshot();
	TestTrue(TEXT("While the fetch runs the snapshot is Loading"), Snapshot.State == EACFMediaCatalogState::Loading);
	TestTrue(TEXT("Loading lists no prices"), Snapshot.ThreeDProviders.IsEmpty());

	// The catalog answers: Ready with everything in it.
	Pending[0](200, EntitledCatalogBody());
	Snapshot = *FACFMediaCatalog::GetSnapshot();
	TestTrue(TEXT("A served catalog is Ready"), Snapshot.State == EACFMediaCatalogState::Ready);
	TestTrue(TEXT("The served catalog is entitled"), Snapshot.bThreeDEntitled);
	TestEqual(TEXT("The served balance is read"), Snapshot.RemainingCredits, static_cast<int64>(7400));

	// The cached snapshot is reused inside the cache time: a plain refresh issues nothing.
	FACFMediaCatalog::Refresh(false);
	TestEqual(TEXT("A fresh catalog is not refetched"), FetchedUrls.Num(), 1);

	// A charged job refreshes past the cache time.
	FACFMediaCatalog::NotifyJobFinished();
	TestEqual(TEXT("A charged job forces a refetch"), FetchedUrls.Num(), 2);
	Pending[1](200, EntitledCatalogBody());

	// A new sign-in replaces the account: the old prices vanish and the card shows loading again.
	TestTrue(TEXT("A different key is a new sign in"),
		FACFMediaGateway::Set(TEXT("http://127.0.0.1/litellm/v1"), TEXT("sk-test-second")));
	TestEqual(TEXT("A new sign in refetches"), FetchedUrls.Num(), 3);
	Snapshot = *FACFMediaCatalog::GetSnapshot();
	TestTrue(TEXT("A new sign in is Loading"), Snapshot.State == EACFMediaCatalogState::Loading);
	TestTrue(TEXT("A new sign in hides the old prices"), Snapshot.ThreeDProviders.IsEmpty());

	// A rejected key fails plainly and never leaves stale prices behind.
	Pending[2](401, TEXT("{\"error\":{\"code\":\"invalid_key\"}}"));
	Snapshot = *FACFMediaCatalog::GetSnapshot();
	TestTrue(TEXT("A rejected key is Failed"), Snapshot.State == EACFMediaCatalogState::Failed);
	TestTrue(TEXT("A rejected key tells the user to sign in again"), Snapshot.ErrorText.Contains(TEXT("Sign in again")));
	TestTrue(TEXT("A rejected key lists no prices"), Snapshot.ThreeDProviders.IsEmpty());

	// An unreachable service fails plainly too.
	TestTrue(TEXT("A third key refreshes"), FACFMediaGateway::Set(TEXT("http://127.0.0.1/litellm/v1"), TEXT("sk-test-third")));
	TestEqual(TEXT("A third sign in refetches"), FetchedUrls.Num(), 4);
	Pending[3](0, FString());
	Snapshot = *FACFMediaCatalog::GetSnapshot();
	TestTrue(TEXT("An unreachable service is Failed"), Snapshot.State == EACFMediaCatalogState::Failed);
	TestTrue(TEXT("An unreachable service tells the user"), !Snapshot.ErrorText.IsEmpty());

	// Sign out: the prices vanish at once and the failure is gone.
	FACFMediaGateway::Clear();
	Snapshot = *FACFMediaCatalog::GetSnapshot();
	TestTrue(TEXT("Sign out is NotConnected"), Snapshot.State == EACFMediaCatalogState::NotConnected);
	TestTrue(TEXT("Sign out lists no prices"), Snapshot.ThreeDProviders.IsEmpty());
	TestTrue(TEXT("Sign out clears the failure"), Snapshot.ErrorText.IsEmpty());

	// An answer that was already in flight when the user signed out must not bring the prices back.
	TestTrue(TEXT("A fourth key refreshes"), FACFMediaGateway::Set(TEXT("http://127.0.0.1/litellm/v1"), TEXT("sk-test-fourth")));
	TestEqual(TEXT("A fourth sign in refetches"), FetchedUrls.Num(), 5);
	FACFMediaGateway::Clear();
	Pending[4](200, EntitledCatalogBody());
	Snapshot = *FACFMediaCatalog::GetSnapshot();
	TestTrue(TEXT("A late answer after sign out leaves the card signed out"), Snapshot.State == EACFMediaCatalogState::NotConnected);
	TestTrue(TEXT("A late answer after sign out lists no prices"), Snapshot.ThreeDProviders.IsEmpty());

	// Restore real HTTP and the signed-out state for whatever runs next.
	FACFMediaCatalog::SetFetchOverride(nullptr);
	FACFMediaGateway::Clear();

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAscentMediaCatalogCardStateTest, "Ascent.AITools.MediaCatalog.CardState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAscentMediaCatalogCardStateTest::RunTest(const FString& Parameters)
{
	const FACFMediaCatalogSnapshot Entitled = ReadySnapshotOf(*this, EntitledCatalogBody());
	const FACFMediaCatalogSnapshot Locked = ReadySnapshotOf(*this, LockedCatalogBody());
	const FThreeDGenerationSection Section;

	// (a) Connected and entitled: the Yarin option is enabled and shows balance and prices.
	{
		const FACFGenerationCardState State = ComputeGenerationCardState(Section.ReadEntitlement(Entitled), Entitled,
			/*bYarinConnected*/ true, EACF3DKeySource::Yarin, /*bHasOwnKey*/ true);
		TestTrue(TEXT("Connected and entitled enables the Yarin option"), State.bYarinEnabled && !State.bYarinLocked);
		TestTrue(TEXT("Connected and entitled shows the balance"), State.bShowBalance);
		TestTrue(TEXT("Connected and entitled shows prices"), State.bShowPrices);
		TestFalse(TEXT("Connected and entitled needs no fallback"), State.bShowFallbackMessage);
		TestEqual(TEXT("An explicit Yarin choice is honoured"), State.EffectiveSource, EACFGenerationSource::Yarin);
	}

	// (b) Connected but not entitled: locked with the plan named, no prices, no balance.
	{
		const FACFGenerationCardState State = ComputeGenerationCardState(Section.ReadEntitlement(Locked), Locked,
			true, EACF3DKeySource::Yarin, false);
		TestTrue(TEXT("Not entitled locks the Yarin option"), State.bYarinLocked && !State.bYarinEnabled);
		TestTrue(TEXT("Not entitled names the plan that includes it"), State.LockReason.Contains(TEXT("Included from Hobbyist")));
		TestFalse(TEXT("Not entitled shows no prices"), State.bShowPrices);
		TestFalse(TEXT("Not entitled shows no balance"), State.bShowBalance);
	}

	// (c) Not connected: dimmed with a padlock that says where to sign in, no prices, no balance; the own key side stays usable.
	{
		const FACFGenerationCardState State = ComputeGenerationCardState(Section.ReadEntitlement(Entitled), Entitled,
			/*bYarinConnected*/ false, EACF3DKeySource::Yarin, true);
		TestTrue(TEXT("Not connected locks the Yarin option"), State.bYarinLocked && !State.bYarinEnabled);
		TestFalse(TEXT("Not connected shows no prices"), State.bShowPrices);
		TestFalse(TEXT("Not connected shows no balance"), State.bShowBalance);
		TestTrue(TEXT("Not connected says where to sign in"), State.LockReason.Contains(TEXT("Sign in to Yarin")));
		// The explicit own key choice keeps working signed out.
		const FACFGenerationCardState OwnKeyState = ComputeGenerationCardState(Section.ReadEntitlement(Entitled), Entitled,
			false, EACF3DKeySource::OwnKey, true);
		TestEqual(TEXT("The own key side stays usable when signed out"), OwnKeyState.EffectiveSource, EACFGenerationSource::OwnKey);
		TestFalse(TEXT("A working own key choice needs no fallback"), OwnKeyState.bShowFallbackMessage);
	}

	// (d) Auto is never a choice of its own.
	{
		const auto AutoState = [&Section](const FACFMediaCatalogSnapshot& Snapshot, const bool bConnected, const bool bOwnKey)
		{
			return ComputeGenerationCardState(Section.ReadEntitlement(Snapshot), Snapshot, bConnected,
				EACF3DKeySource::Auto, bOwnKey);
		};
		TestEqual(TEXT("Auto prefers a saved own key"), AutoState(Entitled, true, true).EffectiveSource, EACFGenerationSource::OwnKey);
		TestEqual(TEXT("Auto takes the Yarin credit when entitled and no own key is saved"),
			AutoState(Entitled, true, false).EffectiveSource, EACFGenerationSource::Yarin);
		TestEqual(TEXT("Auto falls back to the own key side when signed out and no own key is saved"),
			AutoState(Entitled, false, false).EffectiveSource, EACFGenerationSource::OwnKey);
	}

	// (e) A chosen Yarin source that cannot serve never falls back silently.
	{
		// Signed out with Yarin chosen.
		const FACFGenerationCardState SignedOut = ComputeGenerationCardState(Section.ReadEntitlement(Entitled), Entitled,
			false, EACF3DKeySource::Yarin, true);
		TestTrue(TEXT("A chosen Yarin source signed out shows the fallback message"), SignedOut.bShowFallbackMessage);
		TestTrue(TEXT("Signed out names the way back"), SignedOut.FallbackReason.Contains(TEXT("Sign in to Yarin")));
		TestEqual(TEXT("The effective source stays Yarin"), SignedOut.EffectiveSource, EACFGenerationSource::Yarin);

		// Not entitled with Yarin chosen.
		const FACFGenerationCardState NotEntitled = ComputeGenerationCardState(Section.ReadEntitlement(Locked), Locked,
			true, EACF3DKeySource::Yarin, false);
		TestTrue(TEXT("A chosen Yarin source without entitlement shows the fallback message"), NotEntitled.bShowFallbackMessage);
		TestTrue(TEXT("Not entitled names the plan"), NotEntitled.FallbackReason.Contains(TEXT("Hobbyist")));

		// Blocked billing.
		FACFMediaCatalogSnapshot Blocked = Entitled;
		Blocked.bBlocked = true;
		const FACFGenerationCardState BlockedState = ComputeGenerationCardState(Section.ReadEntitlement(Blocked), Blocked,
			true, EACF3DKeySource::Yarin, false);
		TestTrue(TEXT("A blocked account shows the fallback message"), BlockedState.bShowFallbackMessage);
		TestTrue(TEXT("A blocked account names billing"), BlockedState.FallbackReason.Contains(TEXT("bill")));

		// Remaining credit below the cheapest line: 100 left against a cheapest line of 150.
		FACFMediaCatalogSnapshot NearlySpent = Entitled;
		NearlySpent.RemainingCredits = 100;
		const FACFGenerationCardState Spent = ComputeGenerationCardState(Section.ReadEntitlement(NearlySpent), NearlySpent,
			true, EACF3DKeySource::Yarin, false);
		TestTrue(TEXT("Remaining credit below the cheapest line shows the fallback message"), Spent.bShowFallbackMessage);
		TestTrue(TEXT("The out of credit message names the balance"), Spent.FallbackReason.Contains(TEXT("100 credits left this month")));
		TestTrue(TEXT("The out of credit message names the cheapest option"), Spent.FallbackReason.Contains(TEXT("about 150 credits")));

		// Exactly at it: no fallback.
		FACFMediaCatalogSnapshot Affordable = Entitled;
		Affordable.RemainingCredits = 150;
		TestFalse(TEXT("Remaining credit at the cheapest line still serves"),
			ComputeGenerationCardState(Section.ReadEntitlement(Affordable), Affordable, true, EACF3DKeySource::Yarin, false).bShowFallbackMessage);

		// The Auto resolution follows rule (d): own key effective, so no fallback message even when spent.
		TestFalse(TEXT("Auto resolved to the own key needs no fallback"),
			ComputeGenerationCardState(Section.ReadEntitlement(NearlySpent), NearlySpent, true, EACF3DKeySource::Auto, true).bShowFallbackMessage);
		// But Auto resolved to Yarin without an own key does.
		TestTrue(TEXT("Auto resolved to Yarin warns before the credit runs out"),
			ComputeGenerationCardState(Section.ReadEntitlement(NearlySpent), NearlySpent, true, EACF3DKeySource::Auto, false).bShowFallbackMessage);
	}

	// (f) A failed catalog shows the retry link and no prices.
	{
		FACFMediaCatalogSnapshot Failed = Entitled;
		Failed.State = EACFMediaCatalogState::Failed;
		Failed.ErrorText = TEXT("Yarin could not reach the price service.");
		Failed.ThreeDProviders.Reset();
		Failed.bHasRemaining = false;
		const FACFGenerationCardState State = ComputeGenerationCardState(Section.ReadEntitlement(Failed), Failed,
			true, EACF3DKeySource::Yarin, false);
		TestTrue(TEXT("A failed catalog offers the retry link"), State.bShowRetryLink);
		TestFalse(TEXT("A failed catalog shows no prices"), State.bShowPrices);
		TestFalse(TEXT("A failed catalog shows no balance"), State.bShowBalance);
	}

	// The RECOMMENDED badge marks the effective default only.
	{
		TestTrue(TEXT("Auto without an own key and entitled marks Yarin recommended"),
			ComputeGenerationCardState(Section.ReadEntitlement(Entitled), Entitled, true, EACF3DKeySource::Auto, false).bYarinRecommended);
		TestFalse(TEXT("An own key saved under Auto takes the badge away"),
			ComputeGenerationCardState(Section.ReadEntitlement(Entitled), Entitled, true, EACF3DKeySource::Auto, true).bYarinRecommended);
		TestFalse(TEXT("An explicit choice carries no badge"),
			ComputeGenerationCardState(Section.ReadEntitlement(Entitled), Entitled, true, EACF3DKeySource::Yarin, false).bYarinRecommended);

		// Low but affordable: the warning band is above the floor and below one more generation.
		FACFMediaCatalogSnapshot AlmostSpent = Entitled;
		AlmostSpent.RemainingCredits = 200;
		const FACFGenerationCardState Low = ComputeGenerationCardState(Section.ReadEntitlement(AlmostSpent), AlmostSpent,
			true, EACF3DKeySource::Yarin, false);
		TestTrue(TEXT("Low but affordable credit warns"), Low.bShowLowCreditWarning);
		FACFMediaCatalogSnapshot Plenty = Entitled;
		Plenty.RemainingCredits = 5000;
		TestFalse(TEXT("Comfortable credit does not warn"),
			ComputeGenerationCardState(Section.ReadEntitlement(Plenty), Plenty, true, EACF3DKeySource::Yarin, false).bShowLowCreditWarning);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAscentMediaCatalogFormatTest, "Ascent.AITools.MediaCatalog.Format",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAscentMediaCatalogFormatTest::RunTest(const FString& Parameters)
{
	// Thousands carry separators; exactly one credit is singular; negatives clamp to zero.
	TestEqual(TEXT("Zero credits read plural"), FACFMediaCatalog::FormatCredits(0), FString(TEXT("0 credits")));
	TestEqual(TEXT("One credit reads singular"), FACFMediaCatalog::FormatCredits(1), FString(TEXT("1 credit")));
	TestEqual(TEXT("A negative amount reads as zero"), FACFMediaCatalog::FormatCredits(-5), FString(TEXT("0 credits")));
	TestEqual(TEXT("Three digits carry no separator"), FACFMediaCatalog::FormatCredits(999), FString(TEXT("999 credits")));
	TestEqual(TEXT("A thousand carries a separator"), FACFMediaCatalog::FormatCredits(1000), FString(TEXT("1,000 credits")));
	TestEqual(TEXT("A million carries two separators"), FACFMediaCatalog::FormatCredits(1234567), FString(TEXT("1,234,567 credits")));

	// A price reads as an approximation.
	TestEqual(TEXT("A price reads about its credits"), FACFMediaCatalog::FormatAboutCredits(675), FString(TEXT("about 675 credits")));
	TestEqual(TEXT("A one credit price reads singular"), FACFMediaCatalog::FormatAboutCredits(1), FString(TEXT("about 1 credit")));
	TestEqual(TEXT("A large price carries a separator"), FACFMediaCatalog::FormatAboutCredits(12960), FString(TEXT("about 12,960 credits")));
	TestEqual(TEXT("A negative price reads as zero"), FACFMediaCatalog::FormatAboutCredits(-1), FString(TEXT("about 0 credits")));

	// The balance speaks of the month.
	TestEqual(TEXT("A balance reads with the month"), FACFMediaCatalog::FormatCreditsLeftThisMonth(7400), FString(TEXT("7,400 credits left this month")));
	TestEqual(TEXT("A one credit balance reads singular"), FACFMediaCatalog::FormatCreditsLeftThisMonth(1), FString(TEXT("1 credit left this month")));
	TestEqual(TEXT("A spent balance reads as zero"), FACFMediaCatalog::FormatCreditsLeftThisMonth(0), FString(TEXT("0 credits left this month")));
	TestEqual(TEXT("A negative balance reads as zero"), FACFMediaCatalog::FormatCreditsLeftThisMonth(-1), FString(TEXT("0 credits left this month")));

	// The chip shares the amount part without the month.
	TestEqual(TEXT("The chip amount ends before the month"), FACFMediaCatalog::FormatCreditsLeft(7400), FString(TEXT("7,400 credits left")));
	TestEqual(TEXT("The chip amount reads zero when spent"), FACFMediaCatalog::FormatCreditsLeft(0), FString(TEXT("0 credits left")));
	TestEqual(TEXT("A negative chip amount reads as zero"), FACFMediaCatalog::FormatCreditsLeft(-2), FString(TEXT("0 credits left")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAscentMediaCatalogChipTest, "Ascent.AITools.MediaCatalog.YarinChip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAscentMediaCatalogChipTest::RunTest(const FString& Parameters)
{
	const FACFMediaCatalogSnapshot Ready = ReadySnapshotOf(*this, EntitledCatalogBody());

	// Signed out: the chip is a call to action and carries no amount, whatever the last snapshot held.
	const FACFYarinChipState SignedOut = ComputeYarinChipState(false, Ready);
	TestFalse(TEXT("Signed out is not connected"), SignedOut.bConnected);
	TestFalse(TEXT("Signed out shows no balance"), SignedOut.bShowsBalance);
	TestEqual(TEXT("Signed out offers to connect"), SignedOut.Text, FString(TEXT("Connect Yarin")));

	// The snapshot must not smuggle an amount into a signed-out chip.
	FACFMediaCatalogSnapshot ReadyButDisconnected = Ready;
	ReadyButDisconnected.State = EACFMediaCatalogState::Ready;
	TestFalse(TEXT("A ready snapshot still hides the amount signed out"),
		ComputeYarinChipState(false, ReadyButDisconnected).Text.Contains(TEXT("credit")));
	ReadyButDisconnected.State = EACFMediaCatalogState::Loading;
	TestFalse(TEXT("A loading snapshot still hides the amount signed out"),
		ComputeYarinChipState(false, ReadyButDisconnected).Text.Contains(TEXT("credit")));

	// Connected with a Ready catalog: the remaining credit shows, also for a plan without 3D or voice.
	const FACFYarinChipState Connected = ComputeYarinChipState(true, Ready);
	TestTrue(TEXT("Connected marks the chip connected"), Connected.bConnected);
	TestTrue(TEXT("Connected shows the balance"), Connected.bShowsBalance);
	TestEqual(TEXT("Connected names the credit left"), Connected.Text, FString(TEXT("Yarin  7,400 credits left")));
	TestFalse(TEXT("The chip never names dollars"), Connected.Text.Contains(TEXT("$")));

	// A Ready snapshot without a remaining amount shows the plain name: there is no amount to name.
	FACFMediaCatalogSnapshot NoRemaining = Ready;
	NoRemaining.bHasRemaining = false;
	const FACFYarinChipState NameOnly = ComputeYarinChipState(true, NoRemaining);
	TestTrue(TEXT("Connected without a remaining amount still means connected"), NameOnly.bConnected);
	TestFalse(TEXT("Connected without a remaining amount hides the balance"), NameOnly.bShowsBalance);
	TestEqual(TEXT("Connected without a remaining amount reads the name only"), NameOnly.Text, FString(TEXT("Yarin")));

	// A Loading or Failed catalog also reads the plain name; no stale and no failed amount is shown.
	const auto ChipWithState = [](const EACFMediaCatalogState State)
	{
		FACFMediaCatalogSnapshot Snapshot;
		Snapshot.State = State;
		return ComputeYarinChipState(true, Snapshot);
	};
	TestEqual(TEXT("A loading catalog shows the plain name"), ChipWithState(EACFMediaCatalogState::Loading).Text, FString(TEXT("Yarin")));
	TestFalse(TEXT("A loading catalog shows no amount"), ChipWithState(EACFMediaCatalogState::Loading).Text.Contains(TEXT("credit")));
	TestEqual(TEXT("A failed catalog shows the plain name"), ChipWithState(EACFMediaCatalogState::Failed).Text, FString(TEXT("Yarin")));
	TestEqual(TEXT("A not connected snapshot signed in still awaits its first fetch"),
		ChipWithState(EACFMediaCatalogState::NotConnected).Text, FString(TEXT("Yarin")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAscentMediaCatalogApprovalNoteTest, "Ascent.AITools.MediaCatalog.ApprovalNote",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAscentMediaCatalogApprovalNoteTest::RunTest(const FString& Parameters)
{
	const FACFMediaCatalogSnapshot Entitled = ReadySnapshotOf(*this, EntitledCatalogBody());
	const FACFMediaCatalogSnapshot Locked = ReadySnapshotOf(*this, LockedCatalogBody());
	const FThreeDGenerationSection Section;
	const FACFGenerationEntitlement Entitlement = Section.ReadEntitlement(Entitled);
	const FACFGenerationEntitlement LockedEntitlement = Section.ReadEntitlement(Locked);

	const auto Note = [](const EACF3DProvider Provider, const bool bImage, const bool bTextures,
		const FACFMediaCatalogSnapshot& Snapshot, const bool bConnected, const EACF3DKeySource Setting,
		const bool bOwnKey, const FACFGenerationEntitlement& SectionEntitlement)
	{
		return ComputeMeshApprovalNote(Provider, bImage, bTextures, Snapshot, bConnected, Setting, bOwnKey, SectionEntitlement);
	};

	// Yarin credit and a Ready entitled catalog name the provider, the texture setting and the line price.
	TestEqual(TEXT("A Tripo text mesh reads the provider, the texture setting and the line"),
		Note(EACF3DProvider::Tripo, false, false, Entitled, true, EACF3DKeySource::Yarin, true, Entitlement),
		FString(TEXT("Tripo, untextured, about 250 credits from your Yarin plan.")));
	TestTrue(TEXT("A textured call with no matching line names no price"), Note(EACF3DProvider::Tripo, false, true,
		Entitled, true, EACF3DKeySource::Yarin, true, Entitlement).IsEmpty());
	TestEqual(TEXT("A Meshy image mesh reads the image line"),
		Note(EACF3DProvider::Meshy, true, false, Entitled, true, EACF3DKeySource::Yarin, false, Entitlement),
		FString(TEXT("Meshy, untextured, about 300 credits from your Yarin plan.")));
	TestEqual(TEXT("A Meshy textured image reads the textured image line"),
		Note(EACF3DProvider::Meshy, true, true, Entitled, true, EACF3DKeySource::Yarin, false, Entitlement),
		FString(TEXT("Meshy, textured, about 600 credits from your Yarin plan.")));
	// The catalog's textured text line is already the whole Meshy job (preview and texture pass), so it is never added to the mesh line.
	TestEqual(TEXT("A Meshy textured text mesh reads the textured line once and does not add the mesh line"),
		Note(EACF3DProvider::Meshy, false, true, Entitled, true, EACF3DKeySource::Yarin, false, Entitlement),
		FString(TEXT("Meshy, textured, about 600 credits from your Yarin plan.")));
	TestEqual(TEXT("A Meshy untextured text mesh reads the mesh line"),
		Note(EACF3DProvider::Meshy, false, false, Entitled, true, EACF3DKeySource::Yarin, false, Entitlement),
		FString(TEXT("Meshy, untextured, about 300 credits from your Yarin plan.")));

	// The own key source names the key and carries no price, even with a populated catalog.
	const FString OwnKeyNote = Note(EACF3DProvider::Meshy, false, true, Entitled, true, EACF3DKeySource::OwnKey, true, Entitlement);
	TestEqual(TEXT("The own key source names the provider's own key"), OwnKeyNote, FString(TEXT("Uses your own Meshy key.")));
	TestFalse(TEXT("The own key note carries no amount"), OwnKeyNote.Contains(TEXT("credit")));
	// Auto with a saved own key resolves to the own key side and reads the same.
	TestEqual(TEXT("Auto with a saved own key reads the own key note"),
		Note(EACF3DProvider::Tripo, true, true, Entitled, true, EACF3DKeySource::Auto, true, Entitlement),
		FString(TEXT("Uses your own Tripo key.")));
	// Auto without an own key resolves to Yarin when connected and entitled.
	TestEqual(TEXT("Auto without an own key reads the Yarin price"),
		Note(EACF3DProvider::Tripo, false, false, Entitled, true, EACF3DKeySource::Auto, false, Entitlement),
		FString(TEXT("Tripo, untextured, about 250 credits from your Yarin plan.")));

	// A chosen Yarin source that cannot serve carries the card's plain refusal, never an amount.
	const FString SignedOutNote = Note(EACF3DProvider::Tripo, false, false, Entitled, false, EACF3DKeySource::Yarin, true, Entitlement);
	TestTrue(TEXT("Signed out with Yarin chosen refuses in the card's words"), SignedOutNote.Contains(TEXT("Sign in to Yarin")));
	TestFalse(TEXT("The signed out refusal carries no amount"), SignedOutNote.Contains(TEXT("credits")));
	const FString NotEntitledNote = Note(EACF3DProvider::Meshy, false, false, Locked, true, EACF3DKeySource::Yarin, false, LockedEntitlement);
	TestTrue(TEXT("Not entitled with Yarin chosen names the plan"), NotEntitledNote.Contains(TEXT("Hobbyist")));
	TestFalse(TEXT("The not entitled refusal carries no amount"), NotEntitledNote.Contains(TEXT("credits")));

	// Almost out of credit: the card's refusal is the note, with what is left in it.
	FACFMediaCatalogSnapshot NearlySpent = ReadySnapshotOf(*this, EntitledCatalogBody());
	NearlySpent.RemainingCredits = 100;
	const FString OutOfCreditNote = Note(EACF3DProvider::Tripo, false, false, NearlySpent, true, EACF3DKeySource::Yarin, false, Entitlement);
	TestTrue(TEXT("Out of credit refuses through the card's message"), OutOfCreditNote.Contains(TEXT("100 credits left this month")));

	// A catalog that is not Ready shows no price at all while Yarin is the effective source.
	FACFMediaCatalogSnapshot Loading = ReadySnapshotOf(*this, EntitledCatalogBody());
	Loading.State = EACFMediaCatalogState::Loading;
	FACFMediaCatalogSnapshot Failed = ReadySnapshotOf(*this, EntitledCatalogBody());
	Failed.State = EACFMediaCatalogState::Failed;
	TestTrue(TEXT("A loading catalog prices nothing on the card"),
		Note(EACF3DProvider::Meshy, false, true, Loading, true, EACF3DKeySource::Yarin, false, Entitlement).IsEmpty());
	TestTrue(TEXT("A failed catalog prices nothing on the card"),
		Note(EACF3DProvider::Meshy, false, true, Failed, true, EACF3DKeySource::Yarin, false, Entitlement).IsEmpty());

	return true;
}
