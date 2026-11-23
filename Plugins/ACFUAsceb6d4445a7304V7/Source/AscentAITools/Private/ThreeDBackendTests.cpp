// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "ThreeDBackend.h"
#include "ThreeDParsers.h"
#include "MediaGateway.h"
#include "AscentAIToolsSettings.h"
#include "Containers/StringConv.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/AutomationTest.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAscentAITools3DParseTest, "Ascent.AITools.3D.Parse",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
	/** Parses a JSON document into its object, or returns null when malformed. */
	TSharedPtr<FJsonObject> ParseJsonObject(const FString& Text)
	{
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
		TSharedPtr<FJsonValue> Parsed;
		if (FJsonSerializer::Deserialize(Reader, Parsed) && Parsed.IsValid() && Parsed->Type == EJson::Object)
		{
			return Parsed->AsObject();
		}
		return nullptr;
	}

	/** Builds {"code":0,"data":{...}} or {"code":N,"status":"error","message":...} bodies. */
	FString TripoBody(const FString& DataObject)
	{
		return TEXT("{\"code\":0,\"data\":") + DataObject + TEXT("}");
	}
}

bool FAscentAITools3DParseTest::RunTest(const FString& Parameters)
{
	// Tripo running and queued keep polling and report progress.
	ACF3D::FPollResult Running = ACF3D::ParseTripoTask(200, ParseJsonObject(TripoBody(
		TEXT("{\"status\":\"running\",\"progress\":42}"))));
	TestTrue(TEXT("A running Tripo task keeps polling"), Running.State == ACF3D::EPollState::Pending);
	TestEqual(TEXT("A running Tripo task reports its progress"), Running.Progress, 42);

	TestTrue(TEXT("A queued Tripo task keeps polling"),
		ACF3D::ParseTripoTask(200, ParseJsonObject(TripoBody(TEXT("{\"status\":\"queued\"}")))).State == ACF3D::EPollState::Pending);

	// Success carries the model URL at 100 percent.
	const FString ModelUrlData = TEXT("{\"status\":\"success\",\"output\":{\"model_url\":\"https://cdn.tripo3d.ai/model.glb\"}}");
	ACF3D::FPollResult Success = ACF3D::ParseTripoTask(200, ParseJsonObject(TripoBody(ModelUrlData)));
	TestTrue(TEXT("A successful Tripo task is done"), Success.State == ACF3D::EPollState::Succeeded);
	TestEqual(TEXT("A successful Tripo task carries its model URL"),
		Success.ModelUrl, FString(TEXT("https://cdn.tripo3d.ai/model.glb")));
	TestEqual(TEXT("A successful Tripo task is complete"), Success.Progress, 100);

	// Success without a model URL cannot be imported, so it is a failure.
	TestTrue(TEXT("A Tripo success without a model URL fails"),
		ACF3D::ParseTripoTask(200, ParseJsonObject(TripoBody(TEXT("{\"status\":\"success\"}")))).State == ACF3D::EPollState::Failed);
	TestTrue(TEXT("A Tripo success without a model URL explains why"),
		!ACF3D::ParseTripoTask(200, ParseJsonObject(TripoBody(TEXT("{\"status\":\"success\"}")))).Error.IsEmpty());

	// Failures carry the service's own message when it has one.
	ACF3D::FPollResult Failed = ACF3D::ParseTripoTask(200,
		ParseJsonObject(TripoBody(TEXT("{\"status\":\"failed\",\"error_message\":\"bad input\"}"))));
	TestTrue(TEXT("A failed Tripo task is terminal"), Failed.State == ACF3D::EPollState::Failed);
	TestTrue(TEXT("A failed Tripo task keeps the service error"), Failed.Error.Contains(TEXT("bad input")));

	for (const TCHAR* Status : { TEXT("failed"), TEXT("cancelled"), TEXT("banned"), TEXT("expired") })
	{
		const FString Body = TripoBody(FString::Printf(TEXT("{\"status\":\"%s\"}"), Status));
		ACF3D::FPollResult Result = ACF3D::ParseTripoTask(200, ParseJsonObject(Body));
		TestTrue(FString::Printf(TEXT("A %s Tripo task fails with an error"), Status),
			Result.State == ACF3D::EPollState::Failed && !Result.Error.IsEmpty());
	}

	// Transport errors of Tripo's own shape are surfaced as failures.
	ACF3D::FPollResult Unauthorized = ACF3D::ParseTripoTask(401,
		ParseJsonObject(TEXT("{\"code\":2,\"status\":\"error\",\"message\":\"Invalid API key\"}")));
	TestTrue(TEXT("A Tripo 401 fails"), Unauthorized.State == ACF3D::EPollState::Failed);
	TestTrue(TEXT("A Tripo 401 keeps the service error"), Unauthorized.Error.Contains(TEXT("Invalid API key")));

	// A 200 body whose code is not 0 carries the failure message.
	ACF3D::FPollResult Rejected = ACF3D::ParseTripoTask(200,
		ParseJsonObject(TEXT("{\"code\":100,\"message\":\"quota exhausted\"}")));
	TestTrue(TEXT("A Tripo body with a nonzero code fails"), Rejected.State == ACF3D::EPollState::Failed);
	TestTrue(TEXT("A Tripo body with a nonzero code keeps the message"), Rejected.Error.Contains(TEXT("quota exhausted")));

	// Transient conditions keep polling: 429, 503 and a non JSON body.
	TestTrue(TEXT("A Tripo 429 keeps polling"),
		ACF3D::ParseTripoTask(429, nullptr).State == ACF3D::EPollState::Pending);
	TestTrue(TEXT("A Tripo 503 keeps polling"),
		ACF3D::ParseTripoTask(503, nullptr).State == ACF3D::EPollState::Pending);
	TestTrue(TEXT("A Tripo task with a non JSON body keeps polling"),
		ACF3D::ParseTripoTask(200, nullptr).State == ACF3D::EPollState::Pending);

	// WaveSpeed processing states keep polling and a completed result carries the glb.
	for (const TCHAR* Status : { TEXT("created"), TEXT("processing") })
	{
		const FString Body = FString::Printf(TEXT("{\"code\":200,\"message\":\"success\",\"data\":{\"status\":\"%s\"}}"), Status);
		TestTrue(FString::Printf(TEXT("A WaveSpeed %s result keeps polling"), Status),
			ACF3D::ParseWaveSpeedResult(200, ParseJsonObject(Body)).State == ACF3D::EPollState::Pending);
	}

	ACF3D::FPollResult WaveDone = ACF3D::ParseWaveSpeedResult(200, ParseJsonObject(
		TEXT("{\"code\":200,\"message\":\"success\",\"data\":{\"status\":\"completed\",\"outputs\":[\"https://cdn/x/model.glb\"]}}")));
	TestTrue(TEXT("A completed WaveSpeed result is done"), WaveDone.State == ACF3D::EPollState::Succeeded);
	TestEqual(TEXT("A completed WaveSpeed result carries its model URL"),
		WaveDone.ModelUrl, FString(TEXT("https://cdn/x/model.glb")));

	// A completion without a model file cannot be imported, so it is a failure.
	ACF3D::FPollResult WaveNoModel = ACF3D::ParseWaveSpeedResult(200, ParseJsonObject(
		TEXT("{\"code\":200,\"message\":\"success\",\"data\":{\"status\":\"completed\",\"outputs\":[\"https://cdn/x/preview.png\"]}}")));
	TestTrue(TEXT("A WaveSpeed completion without a glb or gltf fails"), WaveNoModel.State == ACF3D::EPollState::Failed);
	TestTrue(TEXT("A WaveSpeed completion without a glb or gltf explains why"), !WaveNoModel.Error.IsEmpty());

	// WaveSpeed failures carry the service error, and cancelled styles are terminal.
	ACF3D::FPollResult WaveFailed = ACF3D::ParseWaveSpeedResult(200, ParseJsonObject(
		TEXT("{\"code\":200,\"message\":\"success\",\"data\":{\"status\":\"failed\",\"error\":\"quota exceeded\"}}")));
	TestTrue(TEXT("A failed WaveSpeed result is terminal"), WaveFailed.State == ACF3D::EPollState::Failed);
	TestTrue(TEXT("A failed WaveSpeed result keeps the service error"), WaveFailed.Error.Contains(TEXT("quota exceeded")));

	for (const TCHAR* Status : { TEXT("cancelled"), TEXT("timeout"), TEXT("deleted") })
	{
		const FString Body = FString::Printf(TEXT("{\"code\":200,\"message\":\"success\",\"data\":{\"status\":\"%s\"}}"), Status);
		ACF3D::FPollResult Result = ACF3D::ParseWaveSpeedResult(200, ParseJsonObject(Body));
		TestTrue(FString::Printf(TEXT("A WaveSpeed %s result fails with an error"), Status),
			Result.State == ACF3D::EPollState::Failed && !Result.Error.IsEmpty());
	}

	// Transport errors of WaveSpeed's own shape are surfaced as failures.
	const TSharedPtr<FJsonObject> WaveUnauthorized = ParseJsonObject(TEXT("{\"code\":401,\"message\":\"Invalid API key\"}"));
	ACF3D::FPollResult WaveDenied = ACF3D::ParseWaveSpeedResult(401, WaveUnauthorized);
	TestTrue(TEXT("A WaveSpeed 401 fails"), WaveDenied.State == ACF3D::EPollState::Failed);
	TestTrue(TEXT("A WaveSpeed 401 keeps the service error"), WaveDenied.Error.Contains(TEXT("Invalid API key")));

	// Transient conditions keep polling: 502 and a non JSON body.
	TestTrue(TEXT("A WaveSpeed 502 keeps polling"),
		ACF3D::ParseWaveSpeedResult(502, nullptr).State == ACF3D::EPollState::Pending);
	TestTrue(TEXT("A WaveSpeed result with a non JSON body keeps polling"),
		ACF3D::ParseWaveSpeedResult(200, nullptr).State == ACF3D::EPollState::Pending);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAscentAITools3DUrlTest, "Ascent.AITools.3D.Urls",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAscentAITools3DUrlTest::RunTest(const FString& Parameters)
{
	// A glb beats a gltf, a gltf beats anything else, and the order does not matter.
	TArray<TSharedPtr<FJsonValue>> GltfFirst;
	GltfFirst.Add(MakeShared<FJsonValueString>(TEXT("https://x/a.gltf")));
	GltfFirst.Add(MakeShared<FJsonValueString>(TEXT("https://x/b.png")));
	TestEqual(TEXT("A gltf beats other formats regardless of order"),
		ACF3D::PickModelUrl(GltfFirst), FString(TEXT("https://x/a.gltf")));

	TArray<TSharedPtr<FJsonValue>> GlbLast;
	GlbLast.Add(MakeShared<FJsonValueString>(TEXT("https://x/a.gltf")));
	GlbLast.Add(MakeShared<FJsonValueString>(TEXT("https://x/b.png")));
	GlbLast.Add(MakeShared<FJsonValueString>(TEXT("https://x/c.glb")));
	TestEqual(TEXT("A glb beats a gltf regardless of order"),
		ACF3D::PickModelUrl(GlbLast), FString(TEXT("https://x/c.glb")));

	// URLs nested inside objects are read too.
	TArray<TSharedPtr<FJsonValue>> Nested;
	TSharedPtr<FJsonObject> NestedObject = MakeShared<FJsonObject>();
	NestedObject->SetStringField(TEXT("url"), TEXT("https://x/nested.glb"));
	Nested.Add(MakeShared<FJsonValueObject>(NestedObject));
	TestEqual(TEXT("A URL nested inside an object is found"),
		ACF3D::PickModelUrl(Nested), FString(TEXT("https://x/nested.glb")));

	// The query string does not count as part of the extension.
	TArray<TSharedPtr<FJsonValue>> Queried;
	Queried.Add(MakeShared<FJsonValueString>(TEXT("https://x/m.glb?sig=1")));
	TestEqual(TEXT("A query string does not hide a glb"), ACF3D::PickModelUrl(Queried), FString(TEXT("https://x/m.glb?sig=1")));

	// No model file anywhere means an empty pick.
	TArray<TSharedPtr<FJsonValue>> NoModel;
	NoModel.Add(MakeShared<FJsonValueString>(TEXT("https://x/a.png")));
	NoModel.Add(MakeShared<FJsonValueString>(TEXT("https://x/b.jpg")));
	TestTrue(TEXT("Outputs without a glb or gltf pick nothing"), ACF3D::PickModelUrl(NoModel).IsEmpty());
	TestTrue(TEXT("No outputs pick nothing"), ACF3D::PickModelUrl({}).IsEmpty());

	// A lone item is still picked.
	TArray<TSharedPtr<FJsonValue>> LoneGlb;
	LoneGlb.Add(MakeShared<FJsonValueString>(TEXT("https://x/only.glb")));
	TestEqual(TEXT("A lone glb is picked"), ACF3D::PickModelUrl(LoneGlb), FString(TEXT("https://x/only.glb")));

	// Extension resolution ignores the query and fragment: it lowercases and needs a leaf extension.
	TestEqual(TEXT("An uppercase extension is lowercased"),
		ACF3D::ExtensionFromUrl(TEXT("https://x/y/model.GLB?sig=1#a")), FString(TEXT("glb")));
	TestEqual(TEXT("A leaf without an extension gives none"),
		ACF3D::ExtensionFromUrl(TEXT("https://x/y/noext")), FString());
	TestEqual(TEXT("Multi dot leaves report the last extension"),
		ACF3D::ExtensionFromUrl(TEXT("https://x/y/a.tar.gz")), FString(TEXT("gz")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAscentAITools3DMultipartTest, "Ascent.AITools.3D.MultipartBody",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAscentAITools3DMultipartTest::RunTest(const FString& Parameters)
{
	const FString Boundary = TEXT("Ascent3DBoundary");
	const FString FieldName = TEXT("file");
	const FString FileName = TEXT("image.png");
	const FString ContentType = TEXT("image/png");

	// 0x00, 0xFF, 0x0D, 0x0A prove the payload keeps binary bytes, including CR and LF.
	TArray<uint8> Payload;
	Payload.Add(0x00);
	Payload.Add(0xFF);
	Payload.Add(0x0D);
	Payload.Add(0x0A);
	Payload.Add(0x41);
	TArray<uint8> OutBody;
	ACF3D::BuildMultipartFile(Boundary, FieldName, FileName, ContentType, Payload, OutBody);

	TArray<uint8> Expected;
	auto AppendAscii = [&Expected](const TCHAR* Text)
	{
		auto Converted = StringCast<ANSICHAR>(Text);
		Expected.Append(reinterpret_cast<const uint8*>(Converted.Get()), Converted.Length());
	};
	AppendAscii(TEXT("--Ascent3DBoundary\r\n"));
	AppendAscii(TEXT("Content-Disposition: form-data; name=\"file\"; filename=\"image.png\"\r\n"));
	AppendAscii(TEXT("Content-Type: image/png\r\n\r\n"));
	Expected.Append(Payload);
	AppendAscii(TEXT("\r\n--Ascent3DBoundary--\r\n"));

	TestEqual(TEXT("The multipart body has the expected size"), OutBody.Num(), Expected.Num());
	bool bMatch = true;
	for (int32 Index = 0; bMatch && Index < FMath::Min(OutBody.Num(), Expected.Num()); Index++)
	{
		if (OutBody[Index] != Expected[Index])
		{
			AddError(FString::Printf(TEXT("The multipart body differs at byte %d: got 0x%02X, expected 0x%02X"),
				Index, OutBody[Index], Expected[Index]));
			bMatch = false;
		}
	}
	TestTrue(TEXT("The multipart body matches byte for byte"), bMatch);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAscentAITools3DBoundsTest, "Ascent.AITools.3D.BoundsAndErrors",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAscentAITools3DBoundsTest::RunTest(const FString& Parameters)
{
	// WaveSpeed accepts 40,000 to 1,500,000 faces and clamps into that range.
	auto Clamped = [](int32 Requested, int32 Expected)
	{
		return ACF3D::ClampWaveSpeedFaceCount(Requested) == Expected;
	};
	TestTrue(TEXT("A WaveSpeed target below the floor is clamped up"), Clamped(30000, 40000));
	TestTrue(TEXT("A WaveSpeed target at the floor is kept"), Clamped(40000, 40000));
	TestTrue(TEXT("A WaveSpeed target inside the range is kept"), Clamped(500000, 500000));
	TestTrue(TEXT("A WaveSpeed target above the ceiling is clamped down"), Clamped(2000000, 1500000));
	TestTrue(TEXT("A WaveSpeed target of zero is clamped up to the floor"), Clamped(0, 40000));
	TestTrue(TEXT("A negative WaveSpeed target is clamped up to the floor"), Clamped(-5, 40000));

	// ExtractServiceError walks message, error, suggestion, then falls back to the HTTP code.
	TSharedPtr<FJsonObject> Message = ParseJsonObject(TEXT("{\"message\":\"m1\"}"));
	TestEqual(TEXT("A message field is preferred"), ACF3D::ExtractServiceError(Message, 400), FString(TEXT("m1")));

	TSharedPtr<FJsonObject> ErrorField = ParseJsonObject(TEXT("{\"error\":\"e1\"}"));
	TestEqual(TEXT("An error field is used when there is no message"),
		ACF3D::ExtractServiceError(ErrorField, 400), FString(TEXT("e1")));

	TSharedPtr<FJsonObject> Suggestion = ParseJsonObject(TEXT("{\"suggestion\":\"s1\"}"));
	TestTrue(TEXT("A suggestion is kept as the error"), ACF3D::ExtractServiceError(Suggestion, 400).Contains(TEXT("s1")));

	TSharedPtr<FJsonObject> Empty = MakeShared<FJsonObject>();
	TestTrue(TEXT("An empty body names the HTTP code"),
		ACF3D::ExtractServiceError(Empty, 500).Contains(TEXT("500")));
	TSharedPtr<FJsonObject> NoJson = nullptr;
	TestTrue(TEXT("A non JSON body names the code"),
		ACF3D::ExtractServiceError(NoJson, 503).Contains(TEXT("503")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAscentAITools3DRegistryTest, "Ascent.AITools.3D.BackendRegistry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAscentAITools3DRegistryTest::RunTest(const FString& Parameters)
{
	UEnum* ProviderDef = StaticEnum<EACF3DProvider>();
	TArray<FString> DisplayNames;

	for (int32 Index = 0; Index < ProviderDef->NumEnums(); Index++)
	{
		if (ProviderDef->GetNameStringByIndex(Index).Contains(TEXT("MAX")))
		{
			continue;
		}
		const EACF3DProvider Provider = static_cast<EACF3DProvider>(ProviderDef->GetValueByIndex(Index));
		const IACF3DBackend& Backend = ACF3D::GetBackend(Provider);

		TestTrue(TEXT("GetBackend returns the provider that was asked for"), Backend.GetProvider() == Provider);
		const FString DisplayName = Backend.GetDisplayName().ToString();
		TestFalse(TEXT("Every backend names itself"), DisplayName.IsEmpty());
		TestFalse(TEXT("Backend display names are unique"), DisplayName.IsEmpty() || DisplayNames.Contains(DisplayName));
		DisplayNames.Add(DisplayName);

		const FACF3DCapabilities Caps = Backend.GetCapabilities();
		if (Provider == EACF3DProvider::Meshy)
		{
			TestTrue(TEXT("Meshy does text to 3D"), Caps.bTextTo3D);
			TestTrue(TEXT("Meshy does image to 3D"), Caps.bImageTo3D);
			TestTrue(TEXT("Meshy does rigging"), Caps.bRigging);
			TestTrue(TEXT("Meshy has art style"), Caps.bArtStyle);
			TestTrue(TEXT("Meshy has topology"), Caps.bTopology);
			TestTrue(TEXT("Meshy has refine"), Caps.bRefine);
		}
		else if (Provider == EACF3DProvider::Tripo)
		{
			TestTrue(TEXT("Tripo does text to 3D"), Caps.bTextTo3D);
			TestTrue(TEXT("Tripo does image to 3D"), Caps.bImageTo3D);
			TestTrue(TEXT("Tripo does rigging"), Caps.bRigging);
			TestFalse(TEXT("Tripo has no art style"), Caps.bArtStyle);
			TestFalse(TEXT("Tripo has no topology"), Caps.bTopology);
			TestFalse(TEXT("Tripo has no refine"), Caps.bRefine);
		}
		else if (Provider == EACF3DProvider::WaveSpeed)
		{
			TestTrue(TEXT("WaveSpeed does text to 3D"), Caps.bTextTo3D);
			TestTrue(TEXT("WaveSpeed does image to 3D"), Caps.bImageTo3D);
			TestFalse(TEXT("WaveSpeed has no rigging"), Caps.bRigging);
			TestFalse(TEXT("WaveSpeed has no art style"), Caps.bArtStyle);
			TestFalse(TEXT("WaveSpeed has no topology"), Caps.bTopology);
			TestFalse(TEXT("WaveSpeed has no refine"), Caps.bRefine);
		}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAscentAITools3DKeyRoutingTest, "Ascent.AITools.3D.KeyRouting",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAscentAITools3DKeyRoutingTest::RunTest(const FString& Parameters)
{
	UACFAIToolsSettings* Settings = NewObject<UACFAIToolsSettings>(GetTransientPackage());
	Settings->ThreeDApiKey = TEXT("m");
	Settings->TripoApiKey = TEXT("t");
	Settings->WaveSpeedApiKey = TEXT("w");

	// Each provider reads its own key.
	TestEqual(TEXT("The Meshy key comes from the Meshy field"),
		Settings->GetThreeDApiKey(EACF3DProvider::Meshy), FString(TEXT("m")));
	TestEqual(TEXT("The Tripo key comes from the Tripo field"),
		Settings->GetThreeDApiKey(EACF3DProvider::Tripo), FString(TEXT("t")));
	TestEqual(TEXT("The WaveSpeed key comes from the WaveSpeed field"),
		Settings->GetThreeDApiKey(EACF3DProvider::WaveSpeed), FString(TEXT("w")));

	// Writing the active key touches only that provider's field.
	Settings->ThreeDProvider = EACF3DProvider::Tripo;
	Settings->SetActiveThreeDApiKey(TEXT("t2"));
	TestEqual(TEXT("The active key write lands on the Tripo field"),
		Settings->TripoApiKey, FString(TEXT("t2")));
	TestEqual(TEXT("The Meshy key is untouched by an active write"),
		Settings->ThreeDApiKey, FString(TEXT("m")));
	TestEqual(TEXT("The WaveSpeed key is untouched by an active write"),
		Settings->WaveSpeedApiKey, FString(TEXT("w")));
	TestEqual(TEXT("The active key read follows the active provider"),
		Settings->GetActiveThreeDApiKey(), FString(TEXT("t2")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAscentAITools3DEndpointSourceTest, "Ascent.AITools.3D.EndpointSource",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAscentAITools3DEndpointSourceTest::RunTest(const FString& Parameters)
{
	const FString GatewayUrl = TEXT("https://yarin.example.invalid/litellm/v1");
	const FString GatewayKey = TEXT("sk-yarin-test-key");

	auto MakeSettings = [this]()
	{
		UACFAIToolsSettings* Settings = NewObject<UACFAIToolsSettings>(GetTransientPackage());
		Settings->ThreeDKeySource = EACF3DKeySource::Auto;
		Settings->ThreeDApiKey = TEXT("meshy-own-key");
		Settings->TripoApiKey = TEXT("tripo-own-key");
		Settings->WaveSpeedApiKey = TEXT("wavespeed-own-key");
		Settings->MeshyBaseUrl = TEXT("https://api.meshy.ai");
		Settings->TripoBaseUrl = TEXT("https://openapi.tripo3d.ai/v3");
		Settings->WaveSpeedBaseUrl = TEXT("https://api.wavespeed.ai/api/v3");
		return Settings;
	};

	// Auto prefers the own key and keeps it on the provider's own host.
	{
		UACFAIToolsSettings* Settings = MakeSettings();
		const ACF3D::FACF3DEndpoints Resolved = ACF3D::ResolveEndpoints(EACF3DProvider::Tripo, nullptr, Settings);
		TestFalse(TEXT("An own key under Auto is not billed to Yarin"), Resolved.bBilledToYarin);
		TestEqual(TEXT("An own key under Auto keeps the provider base URL"),
			Resolved.BaseUrl, FString(TEXT("https://openapi.tripo3d.ai/v3")));
		TestEqual(TEXT("An own key under Auto keeps the provider key"), Resolved.ApiKey, FString(TEXT("tripo-own-key")));
	}

	// Auto falls back to the Yarin credit when the own key is empty and Yarin is signed in.
	{
		UACFAIToolsSettings* Settings = MakeSettings();
		Settings->TripoApiKey = FString();
		TestTrue(TEXT("A usable Yarin credential is stored"), FACFMediaGateway::Set(GatewayUrl, GatewayKey));
		const ACF3D::FACF3DEndpoints Resolved = ACF3D::ResolveEndpoints(EACF3DProvider::Tripo, nullptr, Settings);
		TestTrue(TEXT("Auto without an own key bills the Yarin credit"), Resolved.bBilledToYarin);
		TestEqual(TEXT("Auto without an own key routes through the Yarin gateway"),
			Resolved.BaseUrl, FString(TEXT("https://yarin.example.invalid/media3d/tripo/v3")));
		TestEqual(TEXT("Auto without an own key uses the Yarin key"), Resolved.ApiKey, GatewayKey);
	}

	// Auto with neither an own key nor a Yarin sign-in explains what is missing.
	{
		UACFAIToolsSettings* Settings = MakeSettings();
		Settings->TripoApiKey = FString();
		FACFMediaGateway::Clear();
		FString Error;
		const ACF3D::FACF3DEndpoints Resolved = ACF3D::ResolveEndpoints(EACF3DProvider::Tripo, &Error, Settings);
		TestTrue(TEXT("Auto with nothing available resolves no key"), Resolved.ApiKey.IsEmpty());
		TestFalse(TEXT("Auto with nothing available explains the choice"), Error.IsEmpty());
		TestTrue(TEXT("Auto with nothing available mentions signing in"), Error.Contains(TEXT("Sign in to Yarin")));
		TestTrue(TEXT("Auto with nothing available names the provider"), Error.Contains(TEXT("Tripo")));
	}

	// Own API Key never uses Yarin, even when Yarin is signed in.
	{
		UACFAIToolsSettings* Settings = MakeSettings();
		Settings->ThreeDKeySource = EACF3DKeySource::OwnKey;
		Settings->TripoApiKey = TEXT("tripo-own-key");
		TestTrue(TEXT("A usable Yarin credential is stored for the OwnKey check"), FACFMediaGateway::Set(GatewayUrl, GatewayKey));
		const ACF3D::FACF3DEndpoints Resolved = ACF3D::ResolveEndpoints(EACF3DProvider::Tripo, nullptr, Settings);
		TestFalse(TEXT("Own API Key never bills the Yarin credit"), Resolved.bBilledToYarin);
		TestEqual(TEXT("Own API Key keeps the provider key"), Resolved.ApiKey, FString(TEXT("tripo-own-key")));
		TestEqual(TEXT("Own API Key keeps the provider base URL"),
			Resolved.BaseUrl, FString(TEXT("https://openapi.tripo3d.ai/v3")));
	}

	// Own API Key without a key fails instead of falling back to Yarin.
	{
		UACFAIToolsSettings* Settings = MakeSettings();
		Settings->ThreeDKeySource = EACF3DKeySource::OwnKey;
		Settings->TripoApiKey = FString();
		FACFMediaGateway::Clear();
		FString Error;
		const ACF3D::FACF3DEndpoints Resolved = ACF3D::ResolveEndpoints(EACF3DProvider::Tripo, &Error, Settings);
		TestTrue(TEXT("Own API Key without a key resolves no key"), Resolved.ApiKey.IsEmpty());
		TestFalse(TEXT("Own API Key without a key explains the choice"), Error.IsEmpty());
		TestTrue(TEXT("Own API Key without a key names the key"), Error.Contains(TEXT("API key")));
	}

	// Yarin Credit always uses Yarin and errors clearly when signed out.
	{
		UACFAIToolsSettings* Settings = MakeSettings();
		Settings->ThreeDKeySource = EACF3DKeySource::Yarin;
		FACFMediaGateway::Clear();
		FString Error;
		const ACF3D::FACF3DEndpoints Resolved = ACF3D::ResolveEndpoints(EACF3DProvider::Tripo, &Error, Settings);
		TestTrue(TEXT("Yarin Credit without a sign-in resolves no key"), Resolved.ApiKey.IsEmpty());
		TestTrue(TEXT("Yarin Credit without a sign-in asks to sign in"), Error.Contains(TEXT("Sign in to Yarin")));
	}

	// Yarin Credit with a sign-in bills the credit for every provider.
	{
		UACFAIToolsSettings* Settings = MakeSettings();
		Settings->ThreeDKeySource = EACF3DKeySource::Yarin;
		TestTrue(TEXT("A usable Yarin credential is stored for the Yarin check"), FACFMediaGateway::Set(GatewayUrl, GatewayKey));
		for (const EACF3DProvider Provider : { EACF3DProvider::Meshy, EACF3DProvider::Tripo, EACF3DProvider::WaveSpeed })
		{
			const ACF3D::FACF3DEndpoints Resolved = ACF3D::ResolveEndpoints(Provider, nullptr, Settings);
			TestTrue(TEXT("Yarin Credit bills the Yarin credit"), Resolved.bBilledToYarin);
			TestEqual(TEXT("Yarin Credit uses the Yarin key"), Resolved.ApiKey, GatewayKey);
		}
	}

	// A credential the validation refuses behaves like signed out.
	{
		UACFAIToolsSettings* Settings = MakeSettings();
		Settings->ThreeDKeySource = EACF3DKeySource::Yarin;
		Settings->TripoApiKey = FString();
		FACFMediaGateway::Clear();
		TestFalse(TEXT("An insecure remote gateway URL is refused"),
			FACFMediaGateway::Set(TEXT("http://evil.example.invalid/litellm/v1"), GatewayKey));
		TestFalse(TEXT("A refused credential leaves nothing behind"), FACFMediaGateway::IsPresent());
		FString Error;
		ACF3D::ResolveEndpoints(EACF3DProvider::Tripo, &Error, Settings);
		TestTrue(TEXT("A refused credential asks to sign in"), Error.Contains(TEXT("Sign in to Yarin")));

		TestFalse(TEXT("A key with control characters is refused"),
			FACFMediaGateway::Set(GatewayUrl, TEXT("sk bad\r\nEVIL: 1")));
		TestFalse(TEXT("A refused key leaves nothing behind"), FACFMediaGateway::IsPresent());
	}

	// Clearing the holder is the sign-out: presence and the derived root go away.
	{
		TestTrue(TEXT("A usable Yarin credential is stored for the clear check"), FACFMediaGateway::Set(GatewayUrl, GatewayKey));
		TestTrue(TEXT("A stored credential is present"), FACFMediaGateway::IsPresent());
		FACFMediaGateway::Clear();
		TestFalse(TEXT("A cleared credential is not present"), FACFMediaGateway::IsPresent());
		TestTrue(TEXT("A cleared credential has no media root"), FACFMediaGateway::GetMediaRoot().IsEmpty());
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAscentAITools3DGatewayUrlsTest, "Ascent.AITools.3D.GatewayUrls",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAscentAITools3DGatewayUrlsTest::RunTest(const FString& Parameters)
{
	const FString GatewayUrl = TEXT("https://yarin.example.invalid/litellm/v1");
	const FString GatewayKey = TEXT("sk-yarin-test-key");
	const FString MediaRoot = TEXT("https://yarin.example.invalid/media3d");

	auto MakeSettings = [this]()
	{
		UACFAIToolsSettings* Settings = NewObject<UACFAIToolsSettings>(GetTransientPackage());
		Settings->ThreeDKeySource = EACF3DKeySource::Yarin;
		Settings->MeshyBaseUrl = TEXT("https://api.meshy.ai");
		Settings->TripoBaseUrl = TEXT("https://openapi.tripo3d.ai/v3");
		Settings->WaveSpeedBaseUrl = TEXT("https://api.wavespeed.ai/api/v3");
		return Settings;
	};

	// The media root is the gateway origin plus media3d.
	TestTrue(TEXT("A usable Yarin credential is stored for the URL tests"), FACFMediaGateway::Set(GatewayUrl, GatewayKey));
	TestEqual(TEXT("The media root is the gateway origin plus media3d"), FACFMediaGateway::GetMediaRoot(), MediaRoot);

	// A loopback gateway keeps its host and port.
	TestTrue(TEXT("A loopback gateway URL is accepted"), FACFMediaGateway::Set(TEXT("http://localhost:8080/litellm/v1"), GatewayKey));
	TestEqual(TEXT("A loopback gateway keeps its host and port"),
		FACFMediaGateway::GetMediaRoot(), FString(TEXT("http://localhost:8080/media3d")));
	TestTrue(TEXT("The remote gateway URL is restored"), FACFMediaGateway::Set(GatewayUrl, GatewayKey));

	// Each provider keeps its native path below the gateway route.
	{
		UACFAIToolsSettings* Settings = MakeSettings();
		TestEqual(TEXT("Tripo keeps its native path"),
			ACF3D::ResolveEndpoints(EACF3DProvider::Tripo, nullptr, Settings).BaseUrl,
			FString(TEXT("https://yarin.example.invalid/media3d/tripo/v3")));
		TestEqual(TEXT("WaveSpeed keeps its native path"),
			ACF3D::ResolveEndpoints(EACF3DProvider::WaveSpeed, nullptr, Settings).BaseUrl,
			FString(TEXT("https://yarin.example.invalid/media3d/wavespeed/api/v3")));
		TestEqual(TEXT("Meshy has no native path"),
			ACF3D::ResolveEndpoints(EACF3DProvider::Meshy, nullptr, Settings).BaseUrl,
			FString(TEXT("https://yarin.example.invalid/media3d/meshy")));

		// A different region host keeps its whole native path, and a trailing slash does not double.
		Settings->TripoBaseUrl = TEXT("https://openapi.tripo3d.com/v2/openapi");
		TestEqual(TEXT("A custom Tripo host keeps its whole native path"),
			ACF3D::ResolveEndpoints(EACF3DProvider::Tripo, nullptr, Settings).BaseUrl,
			FString(TEXT("https://yarin.example.invalid/media3d/tripo/v2/openapi")));
		Settings->TripoBaseUrl = TEXT("https://openapi.tripo3d.ai/v3/");
		TestEqual(TEXT("A trailing slash does not double"),
			ACF3D::ResolveEndpoints(EACF3DProvider::Tripo, nullptr, Settings).BaseUrl,
			FString(TEXT("https://yarin.example.invalid/media3d/tripo/v3")));
	}

	// Keys never cross to the wrong host: the gateway key goes to the gateway only, the provider key to the provider only.
	{
		UACFAIToolsSettings* Settings = MakeSettings();
		Settings->ThreeDKeySource = EACF3DKeySource::Yarin;
		const TPair<EACF3DProvider, FString> ProviderHosts[] = {
			{ EACF3DProvider::Meshy, TEXT("api.meshy.ai") },
			{ EACF3DProvider::Tripo, TEXT("openapi.tripo3d.ai") },
			{ EACF3DProvider::WaveSpeed, TEXT("api.wavespeed.ai") }
		};
		for (const TPair<EACF3DProvider, FString>& Host : ProviderHosts)
		{
			const ACF3D::FACF3DEndpoints Billed = ACF3D::ResolveEndpoints(Host.Key, nullptr, Settings);
			TestTrue(FString::Printf(TEXT("The billed %s route stays on the gateway"), *Host.Value),
				Billed.BaseUrl.StartsWith(MediaRoot + TEXT("/")));
			TestTrue(FString::Printf(TEXT("The billed %s route avoids the provider host"), *Host.Value),
				!Billed.BaseUrl.Contains(*Host.Value));
			TestEqual(FString::Printf(TEXT("The billed %s route carries the gateway key"), *Host.Value),
				Billed.ApiKey, GatewayKey);
		}

		Settings->ThreeDKeySource = EACF3DKeySource::OwnKey;
		const FString OwnKeys[] = { TEXT("meshy-own-key"), TEXT("tripo-own-key"), TEXT("wavespeed-own-key") };
		const FString OwnBases[] = { Settings->MeshyBaseUrl, Settings->TripoBaseUrl, Settings->WaveSpeedBaseUrl };
		int32 Index = 0;
		for (const TPair<EACF3DProvider, FString>& Host : ProviderHosts)
		{
			Settings->TripoApiKey = TEXT("tripo-own-key");
			Settings->ThreeDApiKey = TEXT("meshy-own-key");
			Settings->WaveSpeedApiKey = TEXT("wavespeed-own-key");
			const ACF3D::FACF3DEndpoints Own = ACF3D::ResolveEndpoints(Host.Key, nullptr, Settings);
			TestEqual(FString::Printf(TEXT("The own %s route stays on the provider host"), *Host.Value),
				Own.BaseUrl, OwnBases[Index]);
			TestTrue(FString::Printf(TEXT("The own %s route avoids the gateway"), *Host.Value),
				!Own.BaseUrl.Contains(TEXT("yarin.example.invalid")));
			TestFalse(FString::Printf(TEXT("The own %s route is not billed"), *Host.Value), Own.bBilledToYarin);
			Index++;
		}
	}

	FACFMediaGateway::Clear();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAscentAITools3DGatewayErrorsTest, "Ascent.AITools.3D.GatewayErrors",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAscentAITools3DGatewayErrorsTest::RunTest(const FString& Parameters)
{
	// HTTP 402: the monthly credit cannot cover the job, with the remaining and required amounts.
	const TSharedPtr<FJsonObject> Insufficient = ParseJsonObject(TEXT(
		"{\"error\":{\"code\":\"insufficient_credit\",\"message\":\"monthly limit\",\"remainingUsd\":1.25,\"requiredUsd\":4.5,\"remainingCredits\":1250,\"requiredCredits\":4500}}"));
	const FString Credit = ACF3D::MapGatewayError(402, Insufficient);
	TestTrue(TEXT("A 402 explains the monthly Yarin credit"), Credit.Contains(TEXT("Yarin credit does not cover this job")));
	TestTrue(TEXT("A 402 shows the required amount"), Credit.Contains(TEXT("needs 4,500 credits")));
	TestTrue(TEXT("A 402 shows the remaining amount"), Credit.Contains(TEXT("1,250 credits left this month")));
	TestFalse(TEXT("A 402 never names dollars"), Credit.Contains(TEXT("$")));

	// HTTP 401 asks for a fresh sign-in.
	const TSharedPtr<FJsonObject> InvalidKey = ParseJsonObject(TEXT("{\"error\":{\"code\":\"invalid_key\",\"message\":\"bad key\"}}"));
	TestTrue(TEXT("A 401 asks to sign in again"), ACF3D::MapGatewayError(401, InvalidKey).Contains(TEXT("Sign in to Yarin again")));

	// A 402 without the amounts still explains the credit.
	const TSharedPtr<FJsonObject> NoAmounts = ParseJsonObject(TEXT("{\"error\":{\"code\":\"insufficient_credit\"}}"));
	TestTrue(TEXT("A 402 without amounts still explains the credit"),
		ACF3D::MapGatewayError(402, NoAmounts).Contains(TEXT("Yarin credit does not cover this job")));

	// A provider's own errors are not the gateway's shape and stay untouched.
	const TSharedPtr<FJsonObject> ProviderAuth = ParseJsonObject(TEXT("{\"code\":2,\"message\":\"Invalid API key\"}"));
	TestTrue(TEXT("A provider's own 401 is not remapped"), ACF3D::MapGatewayError(401, ProviderAuth).IsEmpty());
	TestTrue(TEXT("Other codes are not remapped"), ACF3D::MapGatewayError(500, InvalidKey).IsEmpty());
	const TSharedPtr<FJsonObject> NoJson = nullptr;
	TestTrue(TEXT("A missing body is not remapped"), ACF3D::MapGatewayError(402, NoJson).IsEmpty());

	// The poll classification turns a gateway refusal into a terminal failure that carries the mapped text.
	const ACF3D::FPollResult TripoDenied = ACF3D::ParseTripoTask(401,
		ParseJsonObject(TEXT("{\"error\":{\"code\":\"invalid_key\"}}")));
	TestTrue(TEXT("A gateway 401 fails a Tripo poll"), TripoDenied.State == ACF3D::EPollState::Failed);
	TestTrue(TEXT("A gateway 401 asks to sign in again"), TripoDenied.Error.Contains(TEXT("Sign in to Yarin again")));

	const ACF3D::FPollResult WaveCharged = ACF3D::ParseWaveSpeedResult(402,
		ParseJsonObject(TEXT("{\"error\":{\"code\":\"insufficient_credit\",\"remainingUsd\":0.001,\"requiredUsd\":2,\"remainingCredits\":1,\"requiredCredits\":2000}}")));
	TestTrue(TEXT("A gateway 402 fails a WaveSpeed poll"), WaveCharged.State == ACF3D::EPollState::Failed);
	TestTrue(TEXT("A gateway 402 carries the amounts"), WaveCharged.Error.Contains(TEXT("2,000 credits")) && WaveCharged.Error.Contains(TEXT("1 credit left")));

	return true;
}
