// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "ADSYarinVoice.h"
#include "ADSYarinVoiceSettings.h"
#include "Misc/AutomationTest.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"

namespace
{
	/** A curated voice list like the one the Yarin catalog carries. */
	TArray<FACFCatalogVoice> SampleVoices()
	{
		TArray<FACFCatalogVoice> Out;
		FACFCatalogVoice One;
		One.Id = TEXT("voice-1");
		One.Name = TEXT("Alice");
		One.Description = TEXT("Warm");
		One.PreviewUrl = TEXT("https://cdn.example.com/a.mp3");
		Out.Add(One);

		FACFCatalogVoice Two;
		Two.Id = TEXT("voice-2");
		Two.Name = TEXT("Bob");
		Two.Description = TEXT("Deep");
		Out.Add(Two);
		return Out;
	}

	/** A snapshot with voice models, entitled, 7,400 credits remaining. */
	FACFMediaCatalogSnapshot SampleSnapshot()
	{
		FACFMediaCatalogSnapshot Snapshot;
		Snapshot.State = EACFMediaCatalogState::Ready;
		Snapshot.bVoiceEntitled = true;
		Snapshot.RequiredPlan = TEXT("Hobbyist");
		Snapshot.bHasRemaining = true;
		Snapshot.RemainingCredits = 7400;
		FACFCatalogVoiceModel Expressive;
		Expressive.Id = TEXT("yarin-tts");
		Expressive.Name = TEXT("Expressive");
		Expressive.CreditsPer1kChars = 270;
		Snapshot.VoiceModels.Add(Expressive);
		FACFCatalogVoiceModel Fast;
		Fast.Id = TEXT("yarin-tts-turbo");
		Fast.Name = TEXT("Fast");
		Fast.CreditsPer1kChars = 90;
		Snapshot.VoiceModels.Add(Fast);
		Snapshot.Voices = SampleVoices();
		return Snapshot;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FADSVoiceSourceResolutionTest, "Ascent.DialogueEditor.YarinVoice.SourceResolution",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FADSVoiceSourceResolutionTest::RunTest(const FString& Parameters)
{
	// An explicit choice is honoured in every situation.
	TestTrue(TEXT("Explicit My own key stays own even when Yarin is ready"),
		ADSYarinVoice::ResolveVoiceSource(EADSVoiceBillingSource::OwnKey, false, true, true) == EADSResolvedVoiceSource::OwnKey);
	TestTrue(TEXT("Explicit Yarin credit stays Yarin even when the own key is saved"),
		ADSYarinVoice::ResolveVoiceSource(EADSVoiceBillingSource::Yarin, true, true, true) == EADSResolvedVoiceSource::Yarin);
	TestTrue(TEXT("Explicit Yarin credit is honoured even when signed out (the caller maps the failure)"),
		ADSYarinVoice::ResolveVoiceSource(EADSVoiceBillingSource::Yarin, false, false, false) == EADSResolvedVoiceSource::Yarin);

	// Auto: the own key when saved, else Yarin when connected and entitled, else the own key.
	TestTrue(TEXT("Auto with a saved own key is own even when Yarin is ready"),
		ADSYarinVoice::ResolveVoiceSource(EADSVoiceBillingSource::Auto, true, true, true) == EADSResolvedVoiceSource::OwnKey);
	TestTrue(TEXT("Auto without an own key, signed in and entitled, is Yarin"),
		ADSYarinVoice::ResolveVoiceSource(EADSVoiceBillingSource::Auto, false, true, true) == EADSResolvedVoiceSource::Yarin);
	TestTrue(TEXT("Auto without an own key, signed in but not entitled, is own"),
		ADSYarinVoice::ResolveVoiceSource(EADSVoiceBillingSource::Auto, false, true, false) == EADSResolvedVoiceSource::OwnKey);
	TestTrue(TEXT("Auto without an own key, entitled but signed out, is own"),
		ADSYarinVoice::ResolveVoiceSource(EADSVoiceBillingSource::Auto, false, false, true) == EADSResolvedVoiceSource::OwnKey);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FADSYarinGatewayRequestTest, "Ascent.DialogueEditor.YarinVoice.GatewayRequest",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FADSYarinGatewayRequestTest::RunTest(const FString& Parameters)
{
	const FString GatewayKey(TEXT("sk-gateway-virtual-key"));
	const FADSYarinVoiceGatewayRequest Request = ADSYarinVoice::BuildGatewaySpeechRequest(
		TEXT("https://yarin.example.com/litellm/v1/"), GatewayKey, TEXT("yarin-tts"),
		TEXT("voice-1"), TEXT("Hello there."));

	// URL: the gateway base (trailing slash trimmed) plus the speech endpoint. Not the own key endpoint.
	TestTrue(TEXT("The URL targets the gateway speech endpoint"),
		Request.Url == TEXT("https://yarin.example.com/litellm/v1/audio/speech"));

	// The gateway key travels ONLY as the Bearer authorization value.
	TestTrue(TEXT("Authorization carries the gateway key as Bearer"),
		Request.Authorization == TEXT("Bearer sk-gateway-virtual-key"));

	// The body is JSON with exactly the three flight fields.
	TSharedPtr<FJsonObject> Body;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Request.Body);
	TestTrue(TEXT("The request body is JSON"), FJsonSerializer::Deserialize(Reader, Body) && Body.IsValid());
	if (Body.IsValid())
	{
		FString Model, Input, Voice;
		Body->TryGetStringField(TEXT("model"), Model);
		Body->TryGetStringField(TEXT("input"), Input);
		Body->TryGetStringField(TEXT("voice"), Voice);
		TestEqual(TEXT("The body model is the chosen catalog model"), Model, FString(TEXT("yarin-tts")));
		TestEqual(TEXT("The body input is the node text"), Input, FString(TEXT("Hello there.")));
		TestEqual(TEXT("The body voice is the raw voice id"), Voice, FString(TEXT("voice-1")));
	}

	// The request carries a key in exactly one place: the Bearer authorization. The URL and the body
	// never name a key, and the body carries only the three flight fields.
	TSharedPtr<FJsonObject> BodyCheck;
	TSharedRef<TJsonReader<>> BodyReader = TJsonReaderFactory<>::Create(Request.Body);
	FJsonSerializer::Deserialize(BodyReader, BodyCheck);
	TestTrue(TEXT("The body carries only model, input and voice"),
		BodyCheck.IsValid() && BodyCheck->Values.Num() == 3);
	TestFalse(TEXT("The URL never carries the gateway key"), Request.Url.Contains(GatewayKey));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FADSYarinErrorMappingTest, "Ascent.DialogueEditor.YarinVoice.ErrorMapping",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FADSYarinErrorMappingTest::RunTest(const FString& Parameters)
{
	const FString Plan(TEXT("Hobbyist"));

	const FString Expired = ADSYarinVoice::MapGatewayHttpError(401, Plan);
	TestTrue(TEXT("401 asks to sign in again"), Expired.Contains(TEXT("Sign in to Yarin again")));

	const FString NotInPlan = ADSYarinVoice::MapGatewayHttpError(403, Plan);
	TestTrue(TEXT("403 says the plan does not include voice"), NotInPlan.Contains(TEXT("does not include voice")));
	TestTrue(TEXT("403 names the plan that includes it"), NotInPlan.Contains(Plan));

	const FString NotInPlanNoName = ADSYarinVoice::MapGatewayHttpError(403, FString());
	TestTrue(TEXT("403 without a plan name still says the plan does not include voice"),
		NotInPlanNoName.Contains(TEXT("does not include voice")));

	const FString Credit = ADSYarinVoice::MapGatewayHttpError(429, Plan);
	TestTrue(TEXT("429 says the credit is used up"), Credit.Contains(TEXT("credit is used up")));
	TestTrue(TEXT("429 carries the Add credit hint"), Credit.Contains(TEXT("Add credit")));

	const FString Trouble = ADSYarinVoice::MapGatewayHttpError(503, Plan);
	TestTrue(TEXT("5xx reports provider trouble"), Trouble.Contains(TEXT("having trouble")));
	TestTrue(TEXT("507 is provider trouble too"), ADSYarinVoice::MapGatewayHttpError(507, Plan).Contains(TEXT("having trouble")));

	const FString Unknown = ADSYarinVoice::MapGatewayHttpError(400, Plan);
	TestTrue(TEXT("An unmapped code falls to a plain HTTP line"), Unknown.Contains(TEXT("HTTP 400")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FADSYarinBatchEstimateTest, "Ascent.DialogueEditor.YarinVoice.BatchEstimate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FADSYarinBatchEstimateTest::RunTest(const FString& Parameters)
{
	// The estimate: characters times the model price per 1,000, rounded up to whole credits.
	TestEqual(TEXT("48000 characters at 270 per 1k is 12,960"), ADSYarinVoice::EstimateBatchCredits(48000, 270), static_cast<int64>(12960));
	TestEqual(TEXT("0 characters costs nothing"), ADSYarinVoice::EstimateBatchCredits(0, 270), static_cast<int64>(0));
	TestEqual(TEXT("A fraction of a credit rounds up to one"), ADSYarinVoice::EstimateBatchCredits(1, 270), static_cast<int64>(1));
	TestEqual(TEXT("Just past a whole credit rounds up"), ADSYarinVoice::EstimateBatchCredits(1001, 1), static_cast<int64>(2));
	TestEqual(TEXT("An exact whole credit stays exact"), ADSYarinVoice::EstimateBatchCredits(1000, 1), static_cast<int64>(1));

	// Threshold: below proceeds silently, equal proceeds (only exceeding asks), above asks.
	const int64 Threshold = 500;
	TestTrue(TEXT("Below the threshold starts silently"),
		ADSYarinVoice::DecideBatch(270, Threshold, true, 7400) == EADSYarinBatchDecision::Proceed);
	TestTrue(TEXT("At the threshold starts silently"),
		ADSYarinVoice::DecideBatch(500, Threshold, true, 7400) == EADSYarinBatchDecision::Proceed);
	TestTrue(TEXT("Above the threshold asks first"),
		ADSYarinVoice::DecideBatch(501, Threshold, true, 7400) == EADSYarinBatchDecision::Ask);
	TestTrue(TEXT("Above the threshold with no reported remaining still asks"),
		ADSYarinVoice::DecideBatch(1000, Threshold, false, 0) == EADSYarinBatchDecision::Ask);
	TestTrue(TEXT("Below the threshold with no reported remaining starts silently"),
		ADSYarinVoice::DecideBatch(200, Threshold, false, 0) == EADSYarinBatchDecision::Proceed);

	// Remaining credit: an estimate that exceeds it refuses the batch up front.
	TestTrue(TEXT("An estimate above the remaining credit refuses"),
		ADSYarinVoice::DecideBatch(7401, Threshold, true, 7400) == EADSYarinBatchDecision::Refuse);
	TestTrue(TEXT("An estimate exactly at the remaining credit is served"),
		ADSYarinVoice::DecideBatch(7400, Threshold, true, 7400) == EADSYarinBatchDecision::Ask ||
		ADSYarinVoice::DecideBatch(7400, Threshold, true, 7400) == EADSYarinBatchDecision::Proceed);

	// The modal wording carries the line count, the estimate and the balance.
	const FString Confirm = ADSYarinVoice::FormatBatchConfirmMessage(48, 620, SampleSnapshot());
	TestTrue(TEXT("The confirmation names the lines"), Confirm.Contains(TEXT("48 lines")));
	TestTrue(TEXT("The confirmation names the estimate"), Confirm.Contains(TEXT("about 620 credits")));
	TestTrue(TEXT("The confirmation names the plan"), Confirm.Contains(TEXT("Yarin plan")));
	TestTrue(TEXT("The confirmation names the balance"), Confirm.Contains(TEXT("7,400 credits left this month")));
	TestFalse(TEXT("The confirmation never names dollars"), Confirm.Contains(TEXT("$")));

	const FString Refuse = ADSYarinVoice::FormatBatchRefuseMessage(7401, SampleSnapshot());
	TestTrue(TEXT("The refusal says the credit does not cover the batch"), Refuse.Contains(TEXT("Not enough Yarin credits")));
	TestTrue(TEXT("The refusal names the estimate"), Refuse.Contains(TEXT("about 7,401 credits")));
	TestTrue(TEXT("The refusal carries the Add credit hint"), Refuse.Contains(TEXT("Add credit")));
	TestTrue(TEXT("The refusal offers the own key switch"), Refuse.Contains(TEXT("My own key")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FADSYarinVoiceNotInListTest, "Ascent.DialogueEditor.YarinVoice.VoiceNotInList",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FADSYarinVoiceNotInListTest::RunTest(const FString& Parameters)
{
	const TArray<FACFCatalogVoice> Voices = SampleVoices();

	TestTrue(TEXT("A curated voice id is in the list"), ADSYarinVoice::IsVoiceInCatalog(Voices, TEXT("voice-1")));
	TestFalse(TEXT("A cloned voice from the user's own account is not in the Yarin list"),
		ADSYarinVoice::IsVoiceInCatalog(Voices, TEXT("lUTamkMw7gOzZbFIwmq4")));
	TestFalse(TEXT("An empty id is not in the list"), ADSYarinVoice::IsVoiceInCatalog(Voices, FString()));

	const FACFCatalogVoiceModel* Model = ADSYarinVoice::FindVoiceModel(SampleSnapshot(), TEXT("yarin-tts-turbo"));
	TestTrue(TEXT("The chosen quality is found in the catalog"), Model != nullptr);
	TestTrue(TEXT("The found quality carries its price"), Model && Model->CreditsPer1kChars > 0);
	TestFalse(TEXT("An unknown quality id finds nothing"), ADSYarinVoice::FindVoiceModel(SampleSnapshot(), TEXT("unknown")) != nullptr);
	return true;
}
