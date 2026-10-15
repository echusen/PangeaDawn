// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "ThreeDBackend.h"
#include "ThreeDParsers.h"
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
