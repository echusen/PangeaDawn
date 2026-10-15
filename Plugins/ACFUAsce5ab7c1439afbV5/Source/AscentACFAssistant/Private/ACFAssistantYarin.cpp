// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

// Yarin as a provider: sign in with a Yarin account in the browser (the same device-code flow the Yarin web companion uses), receive a
// per-user gateway key, list the gateway's chat models and run them on the Codex runtime through its `yarin` model provider.

#include "ACFAssistantClientInternal.h"
#include "ACFAssistantOwnPlugin.h"
#include "ACFAssistantSettings.h"
#include "ACFAssistantTools.h"
#include "AscentACFAssistant.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/DateTime.h"
#include "Misc/EngineVersion.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Serialization/JsonSerializer.h"
#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include "Windows/WindowsHWrapper.h"
#include <wincrypt.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

namespace
{
	using FObject = TSharedPtr<FJsonObject>;
	FObject Object() { return MakeShared<FJsonObject>(); }
	FString String(const FObject& Json, const TCHAR* Key)
	{
		FString Text;
		if (Json) { Json->TryGetStringField(Key, Text); }
		return Text;
	}
	FObject Child(const FObject& Json, const TCHAR* Key)
	{
		const FObject* Result = nullptr;
		return Json && Json->TryGetObjectField(Key, Result) ? *Result : Object();
	}
	TArray<TSharedPtr<FJsonValue>> Array(const FObject& Json, const TCHAR* Key)
	{
		const TArray<TSharedPtr<FJsonValue>>* Result = nullptr;
		return Json && Json->TryGetArrayField(Key, Result) ? *Result : TArray<TSharedPtr<FJsonValue>>();
	}
	FObject Parse(const FString& Text)
	{
		FObject Result;
		FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Result);
		return Result;
	}
	FObject AsObject(const TSharedPtr<FJsonValue>& Value) { return Value.IsValid() && Value->Type == EJson::Object ? Value->AsObject() : FObject(); }

	const TCHAR* DefaultYarinCloudUrl = TEXT("https://yarin.darktowerinteractive.com");
	// The id of the custom `[model_providers.*]` table in the Codex profile, and of `modelProvider` on every Yarin thread.
	const TCHAR* YarinCodexProvider = TEXT("yarin");
	// The gateway's own aliases, in the order the picker lists them. Anything else the gateway serves (a raw model) follows alphabetically.
	const TCHAR* const YarinAliasLadder[] = {TEXT("yarin-balanced"), TEXT("yarin-pro"), TEXT("yarin-fast"), TEXT("yarin-free")};
	// Picker ids of gateway models are "yarin/<gateway id>"; the Codex runtime only ever receives the gateway id.
	const TCHAR* YarinModelPrefix = TEXT("yarin/");

	bool IsSecureOrLoopbackUrl(const FString& Url)
	{
		if (Url.StartsWith(TEXT("https://"))) { return true; }
		// A prefix test alone would trust "http://localhost.evil.example": the host must end right after the loopback name.
		for (const FStringView Loopback : {TEXTVIEW("http://127.0.0.1"), TEXTVIEW("http://localhost")})
		{
			if (!Url.StartsWith(Loopback)) { continue; }
			const TCHAR Next = Url.Len() > Loopback.Len() ? Url[Loopback.Len()] : TEXT('\0');
			if (Next == TEXT('\0') || Next == TEXT(':') || Next == TEXT('/')) { return true; }
		}
		return false;
	}

	FString WithoutTrailingSlashes(FString Url)
	{
		while (Url.EndsWith(TEXT("/"))) { Url.LeftChopInline(1); }
		return Url;
	}

	// What the user reads when a Yarin server did not answer at all (status 0) or answered with an unexpected status.
	FString HttpFailureText(const TCHAR* Server, int32 Status)
	{
		return Status == 0 ? FString::Printf(TEXT("Yarin could not reach the Yarin %s. Check your connection and press Try again."), Server)
			: FString::Printf(TEXT("The Yarin %s answered HTTP %d. Press Try again in a moment."), Server, Status);
	}

	FString AccountLabelFrom(const FObject& Account)
	{
		FString Name = String(Account, TEXT("displayName"));
		if (Name.IsEmpty()) { Name = String(Account, TEXT("username")); }
		if (Name.IsEmpty()) { Name = String(Account, TEXT("discordUsername")); }
		if (Name.IsEmpty()) { Name = String(Account, TEXT("email")); }
		FString Plan = String(Account, TEXT("tierDisplayName"));
		if (Plan.IsEmpty()) { Plan = String(Account, TEXT("tierName")); }
		if (Name.IsEmpty()) { return Plan; }
		return Plan.IsEmpty() ? Name : Name + TEXT(" (") + Plan + TEXT(")");
	}
}

// ---------------------------------------------------------------------------------------------------------------- pure helpers

FString FAscentACFAssistant::YarinModelLabel(const FString& GatewayModelId)
{
	// yarin-balanced -> Yarin Balanced. A raw gateway model keeps its own name, because that is what the gateway bills it as.
	FString Tier = GatewayModelId;
	if (!Tier.RemoveFromStart(TEXT("yarin-")) || Tier.IsEmpty()) { return GatewayModelId; }
	Tier[0] = FChar::ToUpper(Tier[0]);
	return TEXT("Yarin ") + Tier;
}

TArray<TSharedPtr<FACFAssistantModel>> FAscentACFAssistant::ParseYarinModels(const FString& ModelsJson, const FString& ModelInfoJson)
{
	// An alias fans out to several deployments in /model/info: the smallest input window is the one a conversation can rely on, and the model is
	// a chat model when any deployment is (image generation and the like are not callable through the Responses chat route).
	struct FInfo
	{
		int64 Window = 0;
		bool bChat = false;
	};
	TMap<FString, FInfo> Info;
	for (const TSharedPtr<FJsonValue>& Value : Array(Parse(ModelInfoJson), TEXT("data")))
	{
		const FObject Entry = AsObject(Value);
		const FString Name = String(Entry, TEXT("model_name"));
		if (Name.IsEmpty()) { continue; }
		const FObject Meta = Child(Entry, TEXT("model_info"));
		FInfo& Known = Info.FindOrAdd(Name);
		const FString Mode = String(Meta, TEXT("mode"));
		Known.bChat |= Mode.IsEmpty() || Mode == TEXT("chat") || Mode == TEXT("responses");
		double Window = 0;
		Meta->TryGetNumberField(TEXT("max_input_tokens"), Window);
		if (Window > 0 && (Known.Window == 0 || static_cast<int64>(Window) < Known.Window)) { Known.Window = static_cast<int64>(Window); }
	}

	TArray<FString> Ids;
	for (const TSharedPtr<FJsonValue>& Value : Array(Parse(ModelsJson), TEXT("data")))
	{
		const FString Id = String(AsObject(Value), TEXT("id"));
		if (Id.IsEmpty() || Ids.Contains(Id)) { continue; }
		const FInfo* Known = Info.Find(Id);
		if (Known && !Known->bChat) { continue; }
		Ids.Add(Id);
	}
	auto Rank = [](const FString& Id)
	{
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(YarinAliasLadder); ++Index)
		{
			if (Id == YarinAliasLadder[Index]) { return Index; }
		}
		return static_cast<int32>(UE_ARRAY_COUNT(YarinAliasLadder));
	};
	Ids.Sort([&Rank](const FString& A, const FString& B)
	{
		const int32 RankA = Rank(A);
		const int32 RankB = Rank(B);
		return RankA != RankB ? RankA < RankB : A < B;
	});

	TArray<TSharedPtr<FACFAssistantModel>> Parsed;
	for (const FString& Id : Ids)
	{
		TSharedPtr<FACFAssistantModel> Model = MakeShared<FACFAssistantModel>();
		Model->Id = YarinModelPrefix + Id;
		Model->ApiId = Id;
		Model->Label = YarinModelLabel(Id);
		Model->Provider = EACFAssistantProvider::Yarin;
		if (const FInfo* Known = Info.Find(Id)) { Model->ContextWindowTokens = Known->Window; }
		Parsed.Add(Model);
	}
	if (!Parsed.IsEmpty()) { Parsed[0]->bDefault = true; }
	return Parsed;
}

bool FAscentACFAssistant::IsUsableGatewayUrl(const FString& Url)
{
	if (!IsSecureOrLoopbackUrl(Url)) { return false; }
	for (const TCHAR Ch : Url)
	{
		if (Ch <= TEXT(' ') || Ch > TEXT('~') || Ch == TEXT('"') || Ch == TEXT('\\')) { return false; }
	}
	return true;
}

bool FAscentACFAssistant::IsUsableGatewayKey(const FString& Key)
{
	constexpr int32 MaxKeyLength = 512;
	if (Key.IsEmpty() || Key.Len() > MaxKeyLength) { return false; }
	for (const TCHAR Ch : Key)
	{
		if (Ch <= TEXT(' ') || Ch > TEXT('~')) { return false; }
	}
	return true;
}

bool FAscentACFAssistant::IsCodexRuntimeInUse() const
{
	if (IsAnyBusy(EACFAssistantProvider::Codex) || Codex.State == EConnectionState::SigningIn) { return true; }
	// A title thread is deliberately not counted: its failure path never clears it, and losing an auto-generated title is not worth a stranded restart.
	return OneShotCompletions.ContainsByPredicate([](const TSharedPtr<FOneShotCompletion>& Shot)
	{
		return Shot.IsValid() && HarnessOf(Shot->Provider) == EACFAssistantProvider::Codex;
	});
}

bool FAscentACFAssistant::IsYarinSignInUrl(const FString& Url, const FString& CloudBaseUrl)
{
	const FString Base = WithoutTrailingSlashes(CloudBaseUrl.TrimStartAndEnd());
	return !Base.IsEmpty() && IsSecureOrLoopbackUrl(Base) && Url.StartsWith(Base + TEXT("/auth/device?"));
}

FString FAscentACFAssistant::GatewayKeyHash(const FString& Key)
{
	// Identifies which key a Codex process was started with without keeping the key itself around a second time.
	return Key.IsEmpty() ? FString() : FMD5::HashAnsiString(*Key);
}

bool FAscentACFAssistant::ProtectSecret(const FString& Plain, TArray<uint8>& OutProtected)
{
	OutProtected.Reset();
#if PLATFORM_WINDOWS
	const FTCHARToUTF8 Utf8(*Plain);

	DATA_BLOB In;
	In.cbData = static_cast<DWORD>(Utf8.Length());
	In.pbData = reinterpret_cast<BYTE*>(const_cast<ANSICHAR*>(Utf8.Get()));
	DATA_BLOB Out = {};
	if (!::CryptProtectData(&In, L"Yarin", nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &Out)) { return false; }
	OutProtected.Append(Out.pbData, static_cast<int32>(Out.cbData));
	::LocalFree(Out.pbData);
	return true;
#else
	return false;
#endif
}

bool FAscentACFAssistant::UnprotectSecret(const TArray<uint8>& Protected, FString& OutPlain)
{
	OutPlain.Reset();
#if PLATFORM_WINDOWS
	if (Protected.IsEmpty()) { return false; }
	DATA_BLOB In;
	In.cbData = static_cast<DWORD>(Protected.Num());
	In.pbData = const_cast<BYTE*>(Protected.GetData());
	DATA_BLOB Out = {};
	if (!::CryptUnprotectData(&In, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &Out)) { return false; }
	const FUTF8ToTCHAR Converted(reinterpret_cast<const ANSICHAR*>(Out.pbData), static_cast<int32>(Out.cbData));
	OutPlain = FString(Converted.Length(), Converted.Get());
	::LocalFree(Out.pbData);
	return true;
#else
	return false;
#endif
}

FString FAscentACFAssistant::YarinCloudUrl()
{
	const FString Url = WithoutTrailingSlashes(UACFAssistantSettings::Get().YarinCloudUrl.TrimStartAndEnd());
	return IsSecureOrLoopbackUrl(Url) ? Url : FString(DefaultYarinCloudUrl);
}

FString FAscentACFAssistant::YarinDeviceFingerprint()
{
	// One Yarin device slot per computer and Windows user, shared by every Unreal project. Distinct from the web companion's, which keys on its state folder.
	return FMD5::HashAnsiString(*FString::Printf(TEXT("ue-native|%s|%s"), FPlatformProcess::ComputerName(), FPlatformProcess::UserName()));
}

// ---------------------------------------------------------------------------------------------------------- Codex runtime wiring

FString FAscentACFAssistant::WireModelId(const FString& Id) const
{
	const TSharedPtr<FACFAssistantModel> Model = FindModel(Id);
	if (Model.IsValid() && !Model->ApiId.IsEmpty()) { return Model->ApiId; }
	// Not in the list right now (the gateway reconnecting): a Yarin id still must not reach the runtime with its picker prefix.
	FString Bare = Id;
	Bare.RemoveFromStart(YarinModelPrefix);
	return Bare;
}

void FAscentACFAssistant::ApplyYarinRoute(const TSharedPtr<FJsonObject>& Params, const FString& RoutedModelId) const
{
	Params->SetStringField(TEXT("modelProvider"), YarinCodexProvider);
	FObject Config = Object();
	// Codex sends its hosted web_search tool whenever the profile allows it. The gateway's models cannot run it, and LiteLLM answers the whole
	// request with "429 No deployments available", which Codex retries and then reports as a bare "Too Many Requests".
	Config->SetStringField(TEXT("web_search"), TEXT("disabled"));
	const TSharedPtr<FACFAssistantModel> Model = FindModel(RoutedModelId);
	// Codex has never heard of these models and would assume a default window, which the context ring would then show as the model's own.
	if (Model.IsValid() && Model->ContextWindowTokens > 0) { Config->SetNumberField(TEXT("model_context_window"), static_cast<double>(Model->ContextWindowTokens)); }
	Params->SetObjectField(TEXT("config"), Config);
}

// ------------------------------------------------------------------------------------------------------------- credential store

FString FAscentACFAssistant::YarinCredentialPath() const
{
	return FPaths::Combine(FPlatformProcess::UserSettingsDir(), TEXT("ACFAssistant/yarin_session.dat"));
}

void FAscentACFAssistant::LoadYarinCredential()
{
	YarinCredential = FYarinCredential();
	const FString Path = YarinCredentialPath();
	TArray<uint8> Bytes;
	FString Text;
	// Not signed in is the normal first-run state, so a missing file must not reach LoadFileToArray, which logs a warning for it.
	const int64 StoredSize = IFileManager::Get().FileSize(*Path);
	if (StoredSize <= 0 || StoredSize > 64 * 1024 || !FFileHelper::LoadFileToArray(Bytes, *Path)
		|| !UnprotectSecret(Bytes, Text))
	{
		return;
	}
	const FObject Json = Parse(Text);
	YarinCredential.DeviceToken = String(Json, TEXT("deviceToken"));
	YarinCredential.GatewayKey = String(Json, TEXT("gatewayKey"));
	YarinCredential.GatewayBaseUrl = String(Json, TEXT("gatewayBaseUrl"));
	YarinCredential.AccountLabel = String(Json, TEXT("accountLabel"));
	// Anything the loader would not trust on the wire is not trusted from disk either. The device token stays: only the gateway fields are re-minted.
	const bool bBadUrl = !YarinCredential.GatewayBaseUrl.IsEmpty() && !IsUsableGatewayUrl(YarinCredential.GatewayBaseUrl);
	const bool bBadKey = !YarinCredential.GatewayKey.IsEmpty() && !IsUsableGatewayKey(YarinCredential.GatewayKey);
	if (bBadUrl || bBadKey)
	{
		YarinCredential.GatewayKey.Reset();
		YarinCredential.GatewayBaseUrl.Reset();
	}
}

void FAscentACFAssistant::SaveYarinCredential() const
{
	FObject Json = Object();
	Json->SetStringField(TEXT("deviceToken"), YarinCredential.DeviceToken);
	Json->SetStringField(TEXT("gatewayKey"), YarinCredential.GatewayKey);
	Json->SetStringField(TEXT("gatewayBaseUrl"), YarinCredential.GatewayBaseUrl);
	Json->SetStringField(TEXT("accountLabel"), YarinCredential.AccountLabel);
	TArray<uint8> Protected;
	if (!ProtectSecret(ACFAssistantTools::Encode(Json), Protected))
	{
		UE_LOG(LogAscentACFAssistant, Warning, TEXT("Yarin could not protect its sign-in for storage; it will be asked for again next session."));
		return;
	}
	const FString Path = YarinCredentialPath();
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
	FFileHelper::SaveArrayToFile(Protected, *Path);
}

void FAscentACFAssistant::ForgetYarinCredential()
{
	YarinCredential = FYarinCredential();
	YarinModels.Reset();
	IFileManager::Get().Delete(*YarinCredentialPath(), false, true, true);
}

void FAscentACFAssistant::DropCodexGatewayKey()
{
	// The running Codex process still holds the old key in its environment and cannot be edited. Replace it when nothing depends on it; otherwise it
	// is replaced by the next restart, and no Yarin model is offered in the meantime.
	if (CodexGatewayKeyHash.IsEmpty() || IsCodexRuntimeInUse()) { return; }
	Disconnect(EACFAssistantProvider::Codex);
	Connect(EACFAssistantProvider::Codex, false);
}

// ------------------------------------------------------------------------------------------------------------------------- http

void FAscentACFAssistant::YarinHttp(const TCHAR* Verb, const FString& Url, const FString& BearerToken, const FString& Body,
	TFunction<void(int32, const FString&)> Done)
{
	const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(Url);
	Request->SetVerb(Verb);
	Request->SetTimeout(30.0f);
	Request->SetHeader(TEXT("Accept"), TEXT("application/json"));
	if (!BearerToken.IsEmpty()) { Request->SetHeader(TEXT("Authorization"), TEXT("Bearer ") + BearerToken); }
	if (!Body.IsEmpty())
	{
		Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
		Request->SetContentAsString(Body);
	}
	TWeakPtr<FAscentACFAssistant> WeakSelf = AsShared();
	Request->OnProcessRequestComplete().BindLambda([WeakSelf, Generation = Yarin.Generation, Done = MoveTemp(Done)](FHttpRequestPtr, FHttpResponsePtr Response, bool bConnected)
	{
		const TSharedPtr<FAscentACFAssistant> Self = WeakSelf.Pin();
		// A cancel, sign-out or newer attempt bumped the generation: this answer belongs to something the user already left.
		if (!Self.IsValid() || Self->Yarin.Generation != Generation) { return; }
		Done(bConnected && Response.IsValid() ? Response->GetResponseCode() : 0, Response.IsValid() ? Response->GetContentAsString() : FString());
	});
	Request->ProcessRequest();
}

// ------------------------------------------------------------------------------------------------------------------- connection

void FAscentACFAssistant::ConnectYarin()
{
#if PLATFORM_WINDOWS
	// A device token held in memory (a sign-in whose key mint failed) survives a retry; only a cold start reads the file.
	if (YarinCredential.DeviceToken.IsEmpty()) { LoadYarinCredential(); }
	if (YarinCredential.DeviceToken.IsEmpty())
	{
		const bool bSignIn = Yarin.bSignInIfSignedOut;
		EnterSignedOut(EACFAssistantProvider::Yarin, TEXT("Sign in with your Yarin account to use Yarin models."));
		if (bSignIn) { StartYarinLogin(); }
		return;
	}
	Yarin.Status = TEXT("Connecting to Yarin...");
	RefreshYarinGateway();
#else
	Fail(EACFAssistantProvider::Yarin, TEXT("Yarin chat is currently supported on Windows Editor builds."));
#endif
}

void FAscentACFAssistant::StartYarinLogin()
{
#if PLATFORM_WINDOWS
	Yarin.bSignInIfSignedOut = false;
	StopYarinLogin();
	Yarin.State = EConnectionState::SigningIn;
	Yarin.Failure.Reset();
	Yarin.Status = TEXT("Opening the Yarin sign-in page...");
	Changed.Broadcast();

	FObject Body = Object();
	Body->SetStringField(TEXT("fingerprint"), YarinDeviceFingerprint());
	Body->SetStringField(TEXT("deviceName"), TEXT("Yarin (Unreal Editor)"));
	Body->SetStringField(TEXT("platform"), TEXT("win32"));
	Body->SetStringField(TEXT("ueVersion"), FEngineVersion::Current().ToString(EVersionComponent::Patch));
	if (const TSharedPtr<IPlugin> Plugin = ACFAssistantPlugin::Owner())
	{
		Body->SetStringField(TEXT("pluginVersion"), Plugin->GetDescriptor().VersionName);
	}
	const FString Cloud = YarinCloudUrl();
	YarinHttp(TEXT("POST"), Cloud + TEXT("/api/device-auth"), FString(), ACFAssistantTools::Encode(Body), [this, Cloud](int32 Status, const FString& Text)
	{
		if (Yarin.State != EConnectionState::SigningIn) { return; }
		const FObject Json = Parse(Text);
		const FString Code = String(Json, TEXT("code"));
		const FString Url = String(Json, TEXT("verificationUrl"));
		if (Status != 200 || Code.IsEmpty() || !IsYarinSignInUrl(Url, Cloud))
		{
			EnterSignedOut(EACFAssistantProvider::Yarin,
				Status == 0 ? TEXT("Yarin could not reach the Yarin cloud. Check your connection and try again.")
				: Status == 429 ? TEXT("Too many sign-in attempts. Wait a minute and try again.")
				: TEXT("Yarin did not receive a secure sign-in link. Try again."));
			return;
		}
		YarinLoginCode = Code;
		double Interval = 5.0;
		Json->TryGetNumberField(TEXT("interval"), Interval);
		const float PollInterval = static_cast<float>(FMath::Clamp(Interval, 3.0, 15.0));
		// The code is good for 10 minutes; the cloud's own expiry wins when it sends one.
		YarinLoginDeadline = FPlatformTime::Seconds() + 10.0 * 60.0;
		FDateTime Expires;
		if (FDateTime::ParseIso8601(*String(Json, TEXT("expiresAt")), Expires))
		{
			const double Remaining = (Expires - FDateTime::UtcNow()).GetTotalSeconds();
			if (Remaining > 0.0 && Remaining < 30.0 * 60.0) { YarinLoginDeadline = FPlatformTime::Seconds() + Remaining; }
		}
		FPlatformProcess::LaunchURL(*Url, nullptr, nullptr);
		Yarin.Status = FString::Printf(TEXT("Finish signing in to Yarin in your browser (code %s). Yarin continues on its own."), *Code);
		YarinPollTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FAscentACFAssistant::PollYarinLogin), PollInterval);
		Changed.Broadcast();
	});
#else
	Fail(EACFAssistantProvider::Yarin, TEXT("Yarin chat is currently supported on Windows Editor builds."));
#endif
}

void FAscentACFAssistant::StopYarinLogin()
{
	FTSTicker::GetCoreTicker().RemoveTicker(YarinPollTicker);
	YarinPollTicker.Reset();
	YarinLoginCode.Reset();
	bYarinPollInFlight = false;
	// Any request still in flight belongs to the login that just ended.
	++Yarin.Generation;
}

bool FAscentACFAssistant::PollYarinLogin(float)
{
	if (Yarin.State != EConnectionState::SigningIn || YarinLoginCode.IsEmpty())
	{
		YarinPollTicker.Reset();
		return false;
	}
	if (FPlatformTime::Seconds() > YarinLoginDeadline)
	{
		StopYarinLogin();
		EnterSignedOut(EACFAssistantProvider::Yarin, TEXT("The Yarin sign-in timed out. Press Sign in with Yarin to try again."));
		return false;
	}
	if (bYarinPollInFlight) { return true; }
	bYarinPollInFlight = true;
	const FString Url = YarinCloudUrl() + TEXT("/api/device-auth-token?code=") + FGenericPlatformHttp::UrlEncode(YarinLoginCode);
	YarinHttp(TEXT("GET"), Url, FString(), FString(), [this](int32 Status, const FString& Text)
	{
		bYarinPollInFlight = false;
		if (Yarin.State != EConnectionState::SigningIn) { return; }
		// 202 is "still waiting". A network error or a 429/5xx is also just a missed poll: the code stays valid, so the next one asks again.
		if (Status == 404 || Status == 410 || Status == 409)
		{
			StopYarinLogin();
			EnterSignedOut(EACFAssistantProvider::Yarin, Status == 410 ? TEXT("The Yarin sign-in expired. Press Sign in with Yarin to try again.")
				: TEXT("That sign-in code was already used or is no longer valid. Press Sign in with Yarin to try again."));
			return;
		}
		if (Status != 200) { return; }
		const FObject Json = Parse(Text);
		const FString Token = String(Json, TEXT("deviceToken"));
		if (String(Json, TEXT("status")) != TEXT("authorized") || Token.IsEmpty()) { return; }
		const FString Label = AccountLabelFrom(Child(Json, TEXT("account")));
		StopYarinLogin();
		YarinCredential = FYarinCredential();
		YarinCredential.DeviceToken = Token;
		YarinCredential.AccountLabel = Label;
		// Saved before the key mint: a failed mint then retries with this token, in this session and after an Editor restart.
		SaveYarinCredential();
		CompleteYarinLogin();
	});
	return true;
}

void FAscentACFAssistant::CompleteYarinLogin()
{
	Yarin.State = EConnectionState::Connecting;
	Yarin.Status = TEXT("Signed in. Loading Yarin models...");
	Changed.Broadcast();
	RefreshYarinGateway();
}

void FAscentACFAssistant::RefreshYarinGateway()
{
	// Answers to anything issued before this refresh are no longer wanted.
	++Yarin.Generation;
	YarinModels.Reset();
	YarinHttp(TEXT("POST"), YarinCloudUrl() + TEXT("/api/model-gateway/key"), YarinCredential.DeviceToken, TEXT("{}"), [this](int32 Status, const FString& Text)
	{
		if (Status == 401 || Status == 403)
		{
			// Revoked, expired (90 days) or the device was removed from the account: the only way back is a new browser sign-in.
			ForgetYarinCredential();
			EnterSignedOut(EACFAssistantProvider::Yarin, TEXT("Your Yarin sign-in expired or was removed. Press Sign in with Yarin."));
			DropCodexGatewayKey();
			return;
		}
		if (Status != 200)
		{
			Fail(EACFAssistantProvider::Yarin, HttpFailureText(TEXT("cloud"), Status));
			return;
		}
		const FObject Json = Parse(Text);
		const FString Key = String(Json, TEXT("key"));
		const FString BaseUrl = WithoutTrailingSlashes(String(Json, TEXT("baseUrl")));
		if (!IsUsableGatewayKey(Key) || !IsUsableGatewayUrl(BaseUrl))
		{
			Fail(EACFAssistantProvider::Yarin, TEXT("The Yarin cloud did not return a usable gateway key. Press Try again."));
			return;
		}
		YarinCredential.GatewayKey = Key;
		YarinCredential.GatewayBaseUrl = BaseUrl;
		const FString Label = AccountLabelFrom(Child(Json, TEXT("account")));
		if (!Label.IsEmpty()) { YarinCredential.AccountLabel = Label; }
		SaveYarinCredential();
		LoadYarinCatalog();
	});
}

void FAscentACFAssistant::LoadYarinCatalog()
{
	const FString Base = YarinCredential.GatewayBaseUrl;
	const FString Key = YarinCredential.GatewayKey;
	YarinHttp(TEXT("GET"), Base + TEXT("/models"), Key, FString(), [this, Base, Key](int32 Status, const FString& ModelsText)
	{
		if (Status != 200)
		{
			Fail(EACFAssistantProvider::Yarin, Status == 401 || Status == 403
				? FString(TEXT("The Yarin gateway rejected this account's key. Press Try again; if it keeps happening, the Yarin service needs attention."))
				: HttpFailureText(TEXT("gateway"), Status));
			return;
		}
		// /model/info adds each model's real context window and mode. Without it the models are still usable, so its failure is not one.
		YarinHttp(TEXT("GET"), Base + TEXT("/model/info"), Key, FString(), [this, ModelsText](int32 InfoStatus, const FString& InfoText)
		{
			YarinModels = ParseYarinModels(ModelsText, InfoStatus == 200 ? InfoText : FString());
			if (YarinModels.IsEmpty())
			{
				Fail(EACFAssistantProvider::Yarin, TEXT("The Yarin gateway offers no chat models for this account."));
				return;
			}
			Yarin.Status = TEXT("Starting Yarin's runtime...");
			// Yarin's models run on the Codex runtime. Signed-out ChatGPT does not matter, a stopped runtime does.
			if (!Codex.bInitialized && Codex.State != EConnectionState::Connecting) { Connect(EACFAssistantProvider::Codex, false); }
			FinishYarinConnect();
			Changed.Broadcast();
		});
	});
}

void FAscentACFAssistant::FinishYarinConnect()
{
	if (YarinModels.IsEmpty() || !YarinCredential.IsComplete()) { return; }
	if (Yarin.State != EConnectionState::Connecting && Yarin.State != EConnectionState::Ready) { return; }
	if (!Codex.bInitialized)
	{
		if (Codex.State == EConnectionState::Failed)
		{
			Fail(EACFAssistantProvider::Yarin, TEXT("Yarin's models run on the Codex runtime, which is not available: ") + Codex.Failure);
		}
		// Otherwise Codex is still connecting; its initialize handshake calls back here.
		return;
	}
	if (GatewayKeyHash(YarinCredential.GatewayKey) != CodexGatewayKeyHash)
	{
		// The runtime was started without this account's key (it signed in later) or with another one, and a process cannot be given a new environment.
		// Restarting it would also kill a ChatGPT sign-in in progress and fail the one-shots running on it, so that waits for the user.
		if (IsCodexRuntimeInUse())
		{
			Fail(EACFAssistantProvider::Yarin, Codex.State == EConnectionState::SigningIn
				? TEXT("Finish or cancel the ChatGPT sign-in, then press Try again.")
				: TEXT("Yarin needs to restart its runtime to use this account. Wait for the running work to finish, then press Try again."));
			return;
		}
		Disconnect(EACFAssistantProvider::Codex);
		Connect(EACFAssistantProvider::Codex, false);
		return;
	}
	if (Yarin.IsReady()) { return; }
	Yarin.bAuthenticated = true;
	Yarin.bInitialized = true;
	Yarin.State = EConnectionState::Ready;
	Yarin.Failure.Reset();
	Yarin.AccountLabel = YarinCredential.AccountLabel;
	Yarin.Status = TEXT("Connected. Yarin is ready.");
	TArray<TSharedPtr<FACFAssistantModel>> Offered = YarinModels;
	SetProviderModels(EACFAssistantProvider::Yarin, MoveTemp(Offered));
	Changed.Broadcast();
}

void FAscentACFAssistant::SignOutYarin()
{
	StopYarinLogin();
	ForgetYarinCredential();
	EnterSignedOut(EACFAssistantProvider::Yarin, TEXT("Signed out of Yarin on this computer."));
	DropCodexGatewayKey();
}
