// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "ACFAssistantClientInternal.h"
#include "ACFAssistantMesh.h"
#include "ACFAssistantOwnPlugin.h"
#include "ACFAssistantTools.h"
#include "ACFAssistantToolGate.h"
#include "ACFAssistantToolPolicy.h"
#include "ACFAssistantSettings.h"
#include "AscentACFAssistant.h"
#include "AscentEpicMCP.h"
#include "HAL/PlatformProcess.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Serialization/JsonSerializer.h"
#include "HAL/PlatformFileManager.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "Interfaces/IPluginManager.h"
#include <initializer_list>
#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include "Windows/WindowsHWrapper.h"
#include "Windows/HideWindowsPlatformTypes.h"
#endif

namespace
{
	TSharedPtr<FAscentACFAssistant> GSharedAssistant;
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
	// What one Claude API call put in the context window: fresh input plus the cached prompt it read or wrote, plus what it answered.
	// Prompt caching moves nearly all of the prompt out of input_tokens (a 26k-token call reports input_tokens = 2), so input_tokens + output_tokens alone reads as about 0%.
	int64 ClaudeCallContextTokens(const FObject& Usage)
	{
		double Total = 0;
		for (const TCHAR* Key : {TEXT("input_tokens"), TEXT("cache_read_input_tokens"), TEXT("cache_creation_input_tokens"), TEXT("output_tokens")})
		{
			double Value = 0;
			if (Usage) { Usage->TryGetNumberField(Key, Value); }
			Total += Value;
		}
		return static_cast<int64>(Total);
	}
	FObject Parse(const FString& Text)
	{
		FObject Result;
		FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Result);
		return Result;
	}
	void Save(const FString& Path, const FObject& Json)
	{
		FFileHelper::SaveStringToFile(ACFAssistantTools::Encode(Json) + TEXT("\r\n"), *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	}

	// Drops the wrappers a tool name carries on the wire: an "mcp__<server>__" prefix, a dotted namespace such as
	// "editor_toolset.toolsets.asset.AssetTools", and the "...Toolset_" prefix of the Ascent dynamic names. Plain
	// underscores are kept, because real tool names contain them ("find_assets").
	FString ShortToolName(const FString& Name)
	{
		FString Short = Name;
		int32 At = Short.Find(TEXT("__"), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
		if (At != INDEX_NONE) { Short = Short.Mid(At + 2); }
		At = Short.Find(TEXT("Toolset_"), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
		if (At != INDEX_NONE) { Short = Short.Mid(At + 8); }
		if (Short.FindLastChar(TEXT('.'), At)) { Short = Short.Mid(At + 1); }
		return Short;
	}

	// Numbered progress, so a long turn reads as movement instead of one frozen line.
	FString StepStatus(int32 Step, const FString& What)
	{
		return Step > 0 ? FString::Printf(TEXT("Step %d: %s"), Step, *What) : What;
	}

	// First readable argument value, so a step reads "find_assets  EffectsManager" instead of only naming the tool.
	FString ToolArgumentSummary(const FObject& Arguments)
	{
		if (!Arguments) { return FString(); }
		for (const TCHAR* Key : {
			TEXT("name"), TEXT("assetName"), TEXT("asset_name"), TEXT("assetPath"), TEXT("asset_path"), TEXT("AssetPath"),
			TEXT("packagePath"), TEXT("package_path"), TEXT("path"), TEXT("Path"), TEXT("folder_path"),
			TEXT("assetClass"), TEXT("asset_class"), TEXT("query"), TEXT("Query"), TEXT("tag"), TEXT("Tag"),
			TEXT("row_name"), TEXT("blueprint_name"), TEXT("toolset_name")})
		{
			FString Value;
			if (Arguments->TryGetStringField(Key, Value))
			{
				Value.TrimStartAndEndInline();
				if (!Value.IsEmpty()) { return Value.Left(70); }
			}
		}
		return FString();
	}

	bool IsChatImageExtension(const FString& Path)
	{
		const FString Ext = FPaths::GetExtension(Path).ToLower();
		return Ext == TEXT("png") || Ext == TEXT("jpg") || Ext == TEXT("jpeg") || Ext == TEXT("webp");
	}

	void CollectImagePathsFromObject(const FObject& Json, TArray<FString>& Out, int32 Depth);

	void CollectImagePathsFromValue(const TSharedPtr<FJsonValue>& Value, TArray<FString>& Out, int32 Depth)
	{
		if (!Value || Depth > 6) { return; }
		if (Value->Type == EJson::Object)
		{
			CollectImagePathsFromObject(Value->AsObject(), Out, Depth + 1);
		}
		else if (Value->Type == EJson::Array)
		{
			for (const TSharedPtr<FJsonValue>& ChildValue : Value->AsArray())
			{
				CollectImagePathsFromValue(ChildValue, Out, Depth + 1);
			}
		}
		else if (Value->Type == EJson::String)
		{
			const FString Text = Value->AsString().TrimStartAndEnd();
			if (Text.StartsWith(TEXT("{")) || Text.StartsWith(TEXT("[")))
			{
				if (const FObject Parsed = Parse(Text))
				{
					CollectImagePathsFromObject(Parsed, Out, Depth + 1);
				}
			}
		}
	}

	void CollectImagePathsFromObject(const FObject& Json, TArray<FString>& Out, int32 Depth)
	{
		if (!Json || Depth > 6) { return; }
		for (const TCHAR* Key : {TEXT("image_file"), TEXT("imageFile"), TEXT("savedPath"), TEXT("saved_path")})
		{
			FString Path;
			if (Json->TryGetStringField(Key, Path) && !Path.IsEmpty())
			{
				Out.AddUnique(Path);
			}
		}
		for (const auto& Pair : Json->Values)
		{
			CollectImagePathsFromValue(Pair.Value, Out, Depth + 1);
		}
	}

	void CollectImagePathsFromToolItem(const FObject& Item, TArray<FString>& Out)
	{
		if (!Item) { return; }
		for (const TCHAR* Key : {TEXT("savedPath"), TEXT("saved_path"), TEXT("path")})
		{
			const FString Path = String(Item, Key);
			if (!Path.IsEmpty()) { Out.AddUnique(Path); }
		}
		CollectImagePathsFromObject(Child(Item, TEXT("result")), Out, 0);
		FString ResultText;
		if (Item->TryGetStringField(TEXT("result"), ResultText))
		{
			CollectImagePathsFromValue(MakeShared<FJsonValueString>(ResultText), Out, 0);
		}
		for (const TSharedPtr<FJsonValue>& Content : Array(Child(Item, TEXT("result")), TEXT("content")))
		{
			CollectImagePathsFromValue(Content, Out, 0);
		}
	}
}

TSharedPtr<FAscentACFAssistant> GetSharedACFAssistant(bool bCreateIfMissing)
{
	if (!GSharedAssistant && bCreateIfMissing)
	{
		GSharedAssistant = MakeShared<FAscentACFAssistant>();
	}
	return GSharedAssistant;
}

void ShutdownSharedACFAssistant()
{
	if (!GSharedAssistant) { return; }
	GSharedAssistant->Shutdown();
	GSharedAssistant.Reset();
}

bool FAscentACFAssistant::IsChatGPTSubscriptionAccount(const TSharedPtr<FJsonObject>& Account)
{
	const FString AuthMode = String(Account, TEXT("authMode"));
	const FString Type = String(Account, TEXT("type"));
	if (AuthMode.Equals(TEXT("apikey"), ESearchCase::IgnoreCase) || AuthMode.Equals(TEXT("api_key"), ESearchCase::IgnoreCase)
		|| AuthMode.Equals(TEXT("api-key"), ESearchCase::IgnoreCase))
	{
		return false;
	}
	return Type.Equals(TEXT("chatgpt"), ESearchCase::IgnoreCase) || AuthMode.Equals(TEXT("chatgpt"), ESearchCase::IgnoreCase);
}

namespace
{
	// `claude auth status` prints one JSON object; anything around it (a warning line, a BOM) is ignored.
	TSharedPtr<FJsonObject> ParseClaudeAuthJson(const FString& AuthStatusJson)
	{
		const int32 ObjectStart = AuthStatusJson.Find(TEXT("{"));
		const int32 ObjectEnd = AuthStatusJson.Find(TEXT("}"), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
		if (ObjectStart == INDEX_NONE || ObjectEnd == INDEX_NONE || ObjectEnd <= ObjectStart) { return nullptr; }
		TSharedPtr<FJsonObject> Json;
		if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(AuthStatusJson.Mid(ObjectStart, ObjectEnd - ObjectStart + 1)), Json)) { return nullptr; }
		return Json;
	}

	// Bedrock, Vertex, Foundry and the other cloud switches report a provider other than firstParty.
	bool IsCloudProvider(const FString& ApiProvider)
	{
		return !ApiProvider.IsEmpty() && !ApiProvider.Equals(TEXT("firstParty"), ESearchCase::IgnoreCase);
	}
}

FString FAscentACFAssistant::ClaudeAccountLabel(const FString& AuthStatusJson)
{
	const TSharedPtr<FJsonObject> Json = ParseClaudeAuthJson(AuthStatusJson);
	if (!Json) { return FString(); }
	const FString Email = String(Json, TEXT("email"));
	const FString ApiProvider = String(Json, TEXT("apiProvider"));
	const FString ApiKeySource = String(Json, TEXT("apiKeySource"));
	// Names who pays, so a user whose turns bill an API key or a cloud account sees it before sending.
	FString Billing = String(Json, TEXT("subscriptionType"));
	if (IsCloudProvider(ApiProvider)) { Billing = ApiProvider; }
	else if (!ApiKeySource.IsEmpty()) { Billing = TEXT("API key from ") + ApiKeySource; }
	else if (ClassifyClaudeAuth(AuthStatusJson) == EClaudeAuth::ApiBilling) { Billing = TEXT("API billing"); }
	if (Email.IsEmpty()) { return Billing; }
	return Billing.IsEmpty() ? Email : Email + TEXT(" (") + Billing + TEXT(")");
}

FAscentACFAssistant::EClaudeAuth FAscentACFAssistant::ClassifyClaudeAuth(const FString& AuthStatusJson, FString* OutProblem)
{
	auto Explain = [OutProblem](EClaudeAuth Verdict, const FString& Problem)
	{
		if (OutProblem) { *OutProblem = Problem; }
		return Verdict;
	};
	const TSharedPtr<FJsonObject> Json = ParseClaudeAuthJson(AuthStatusJson);
	bool bLoggedIn = false;
	if (!Json || !Json->TryGetBoolField(TEXT("loggedIn"), bLoggedIn))
	{
		return Explain(EClaudeAuth::Unreadable,
			TEXT("Yarin could not read Claude Code's sign-in status. Check that `claude auth status` works in a terminal, then press Try again."));
	}
	if (!bLoggedIn) { return Explain(EClaudeAuth::SignedOut, FString()); }
	// Anthropic's terms for running Claude Code inside a product forbid restricting any sign-in method the CLI supports, so every one that
	// Claude Code reports as logged in is accepted. Measured on Claude Code 2.1.280: a Bedrock/Vertex/Foundry switch reports authMethod
	// "third_party" with that apiProvider, and an API key reports authMethod "claude.ai" plus apiKeySource.
	if (IsCloudProvider(String(Json, TEXT("apiProvider"))) || !String(Json, TEXT("apiKeySource")).IsEmpty())
	{
		return Explain(EClaudeAuth::ApiBilling, FString());
	}
	const FString AuthMethod = String(Json, TEXT("authMethod"));
	// claude.ai is a login the CLI performed; oauth_token is the same subscription credential supplied through CLAUDE_CODE_OAUTH_TOKEN (`claude setup-token`).
	if (AuthMethod.Equals(TEXT("claude.ai"), ESearchCase::IgnoreCase) || AuthMethod.Equals(TEXT("oauth_token"), ESearchCase::IgnoreCase))
	{
		return Explain(EClaudeAuth::Subscription, FString());
	}
	// A Console login, or any method a newer CLI adds.
	return Explain(EClaudeAuth::ApiBilling, FString());
}

bool FAscentACFAssistant::IsClaudeAuthFailure(const FString& Error)
{
	for (const TCHAR* Marker : {TEXT("Failed to authenticate"), TEXT("authentication_error"), TEXT("Please run /login"), TEXT("OAuth token")})
	{
		if (Error.Contains(Marker)) { return true; }
	}
	return false;
}

bool FAscentACFAssistant::IsClaudeNestingEnvironmentEntry(const FString& Entry)
{
	// Windows keeps per-drive working directories as "=C:=C:\..." entries; they have no name and always pass.
	int32 Equals = INDEX_NONE;
	if (!Entry.FindChar(TEXT('='), Equals) || Equals <= 0) { return false; }
	const FString Name = Entry.Left(Equals);
	// Keys, tokens, endpoints and cloud-provider switches all pass: Claude Code bills whatever sign-in the user configured on this computer.
	// These two are set inside every shell a Claude Code session spawns; inherited, they tell the child it is nested in another session.
	for (const TCHAR* Nesting : {TEXT("CLAUDECODE"), TEXT("CLAUDE_CODE_ENTRYPOINT")})
	{
		if (Name.Equals(Nesting, ESearchCase::IgnoreCase)) { return true; }
	}
	return false;
}

bool FAscentACFAssistant::IsClaudeLoginUrl(const FString& Url)
{
	return Url.StartsWith(TEXT("https://claude.com/")) || Url.StartsWith(TEXT("https://www.claude.com/"))
		|| Url.StartsWith(TEXT("https://claude.ai/")) || Url.StartsWith(TEXT("https://www.claude.ai/"))
		|| Url.StartsWith(TEXT("https://platform.claude.com/")) || Url.StartsWith(TEXT("https://console.anthropic.com/"))
		|| Url.StartsWith(TEXT("https://anthropic.com/")) || Url.StartsWith(TEXT("https://www.anthropic.com/"));
}

FString FAscentACFAssistant::ExtractClaudeLoginUrl(const FString& Text)
{
	int32 HttpIndex = Text.Find(TEXT("https://"));
	while (HttpIndex != INDEX_NONE)
	{
		FString Url = Text.Mid(HttpIndex);
		int32 End = INDEX_NONE;
		auto CutAt = [&Url, &End](const TCHAR Character)
		{
			int32 At = INDEX_NONE;
			if (Url.FindChar(Character, At)) { End = End == INDEX_NONE ? At : FMath::Min(End, At); }
		};
		CutAt(TEXT('\n'));
		CutAt(TEXT('\r'));
		CutAt(TEXT(' '));
		CutAt(TEXT('\t'));
		if (End != INDEX_NONE) { Url.LeftInline(End); }
		Url.TrimStartAndEndInline();
		if (IsClaudeLoginUrl(Url)) { return Url; }
		HttpIndex = Text.Find(TEXT("https://"), ESearchCase::IgnoreCase, ESearchDir::FromStart, HttpIndex + 8);
	}
	return FString();
}

const TCHAR* FAscentACFAssistant::ClaudePermissionModeName(EACFAssistantWorkMode Mode)
{
	switch (Mode)
	{
	// Not Claude's plan mode: it refuses every Editor tool, reads included. In default mode with no prompt surface, Claude's own
	// writes (Edit, Write, Bash, ...) are denied automatically and the gate blocks the mutating Editor tools.
	case EACFAssistantWorkMode::Ask:
	case EACFAssistantWorkMode::Plan:
		return TEXT("default");
	// default keeps Claude's own prompts alive; they are answered through the gate's permission tool by the Approve / Deny buttons.
	case EACFAssistantWorkMode::AskForApproval:
		return TEXT("default");
	case EACFAssistantWorkMode::ApproveForMe:
	default:
		return TEXT("bypassPermissions");
	}
}

FString FAscentACFAssistant::ClaudeMcpConfiguration(const FString& GateUrl)
{
	return FString::Printf(TEXT("{\r\n  \"mcpServers\": {\r\n    \"ue-editor\": {\r\n      \"type\": \"http\",\r\n      \"url\": \"%s\"\r\n    }\r\n  }\r\n}\r\n"), *GateUrl);
}

FString FAscentACFAssistant::GateUrlFor(const TSharedPtr<FACFAssistantSession>& Session) const
{
	if (!Session.IsValid() || Session->GateToken.IsEmpty()) { return ACFAssistantToolGate::Url(); }
	return FString::Printf(TEXT("%s?%s=%s"), *ACFAssistantToolGate::Url(), ACFAssistantToolGate::ConversationQueryKey, *Session->GateToken);
}

void FAscentACFAssistant::LoadClaudeModels()
{
	TArray<TSharedPtr<FACFAssistantModel>> Loaded;
	auto Add = [this, &Loaded](const TCHAR* Id, const TCHAR* Label, int64 Window, bool bDefault, bool bHasEffort = true)
	{
		TSharedPtr<FACFAssistantModel> Model = MakeShared<FACFAssistantModel>();
		Model->Id = Id;
		// The alias label until a turn reports what it resolves to; after that, the saved version ("Claude Opus 5.5").
		const FString* Resolved = ClaudeResolvedModels.Find(Id);
		const FString Versioned = Resolved ? ClaudeModelLabel(*Resolved) : FString();
		Model->Label = Versioned.IsEmpty() ? FString(Label) : Versioned;
		Model->ContextWindowTokens = Window;
		Model->bDefault = bDefault;
		Model->Provider = EACFAssistantProvider::ClaudeCode;
		// The levels the claude CLI accepts for --effort; Effort stays empty so the CLI keeps its own default.
		if (bHasEffort) { Model->Efforts = {TEXT("low"), TEXT("medium"), TEXT("high"), TEXT("xhigh"), TEXT("max")}; }
		Loaded.Add(Model);
	};
	// The window is learned from the first turn's modelUsage: this list does not carry it, and a guessed 200k would be wrong for the 1M models.
	Add(TEXT("sonnet"), TEXT("Claude Sonnet"), 0, true);
	Add(TEXT("opus"), TEXT("Claude Opus"), 0, false);
	Add(TEXT("haiku"), TEXT("Claude Haiku"), 0, false, false);
	Add(TEXT("fable"), TEXT("Claude Fable"), 0, false);
	SetProviderModels(EACFAssistantProvider::ClaudeCode, MoveTemp(Loaded));
}

TArray<TSharedPtr<FACFAssistantModel>> FAscentACFAssistant::ParseClaudeModels(const FString& CatalogOutput)
{
	FObject Answer;
	TArray<FString> Lines;
	CatalogOutput.ParseIntoArrayLines(Lines);
	for (const FString& Line : Lines)
	{
		const FObject Message = Parse(Line);
		if (String(Message, TEXT("type")) == TEXT("control_response")) { Answer = Child(Child(Message, TEXT("response")), TEXT("response")); break; }
	}
	TArray<TSharedPtr<FACFAssistantModel>> Parsed;
	// The exact model each entry runs. Two entries that run the same model (an alias and its full id) are listed once.
	TArray<FString> ParsedKeys;
	TArray<bool> ParsedMillion;
	FString DefaultKey;
	for (const TSharedPtr<FJsonValue>& Value : Array(Answer, TEXT("models")))
	{
		const FObject Entry = Value.IsValid() && Value->Type == EJson::Object ? Value->AsObject() : FObject();
		const FString Id = String(Entry, TEXT("value"));
		const FString Resolved = String(Entry, TEXT("resolvedModel"));
		const FString Key = Resolved.IsEmpty() ? Id : Resolved;
		// "default" is Claude Code's name for the model it recommends; that model is already listed under its own name.
		if (Id == TEXT("default")) { DefaultKey = Key; continue; }
		if (Id.IsEmpty() || ParsedKeys.Contains(Key)) { continue; }
		TSharedPtr<FACFAssistantModel> Model = MakeShared<FACFAssistantModel>();
		Model->Id = Id;
		Model->Label = ClaudeModelLabel(Key);
		if (Model->Label.IsEmpty()) { Model->Label = String(Entry, TEXT("displayName")); }
		if (Model->Label.IsEmpty()) { Model->Label = Id; }
		const bool bMillion = Id.Contains(TEXT("[1m]")) || Resolved.Contains(TEXT("[1m]"));
		// Unknown unless the id says 1M; the first turn's modelUsage reports the real window.
		Model->ContextWindowTokens = bMillion ? 1000000 : 0;
		Model->Provider = EACFAssistantProvider::ClaudeCode;
		bool bSupportsEffort = false;
		Entry->TryGetBoolField(TEXT("supportsEffort"), bSupportsEffort);
		if (bSupportsEffort)
		{
			for (const TSharedPtr<FJsonValue>& Level : Array(Entry, TEXT("supportedEffortLevels")))
			{
				FString Name;
				if (Level.IsValid() && Level->TryGetString(Name) && !Name.IsEmpty()) { Model->Efforts.Add(Name); }
			}
		}
		Parsed.Add(Model);
		ParsedKeys.Add(Key);
		ParsedMillion.Add(bMillion);
	}
	// The same model can be offered with the standard and the 1M context window; only then does the label need to tell them apart.
	for (int32 Index = 0; Index < Parsed.Num(); ++Index)
	{
		if (!ParsedMillion[Index]) { continue; }
		const FString Label = Parsed[Index]->Label;
		if (Parsed.ContainsByPredicate([&Label, &Parsed, Index](const TSharedPtr<FACFAssistantModel>& Other) { return Other != Parsed[Index] && Other->Label == Label; }))
		{
			Parsed[Index]->Label += TEXT(" (1M context)");
		}
	}
	// Yarin's own background work (titles, translations) runs on the default model, so it stays on the efficient Sonnet whenever Claude Code offers it.
	int32 DefaultIndex = Parsed.IndexOfByPredicate([](const TSharedPtr<FACFAssistantModel>& Model) { return Model->Id == TEXT("sonnet"); });
	if (DefaultIndex == INDEX_NONE) { DefaultIndex = ParsedKeys.IndexOfByKey(DefaultKey); }
	if (Parsed.Num() > 0) { Parsed[DefaultIndex != INDEX_NONE ? DefaultIndex : 0]->bDefault = true; }
	return Parsed;
}

FString FAscentACFAssistant::ClaudeModelLabel(const FString& ResolvedId)
{
	// claude-haiku-4-5-20251001 -> Haiku 4.5; the older claude-3-5-sonnet-20241022 order works too. A "[1m]" context suffix is not part of the name.
	FString Clean = ResolvedId;
	int32 Bracket = INDEX_NONE;
	if (Clean.FindChar(TEXT('['), Bracket)) { Clean.LeftInline(Bracket); }
	TArray<FString> Parts;
	Clean.ParseIntoArray(Parts, TEXT("-"));
	FString Family;
	TArray<FString> Version;
	for (const FString& Part : Parts)
	{
		if (Part.Equals(TEXT("claude"), ESearchCase::IgnoreCase)) { continue; }
		if (Part.IsNumeric())
		{
			// Eight digits is the release date, not part of the version.
			if (Part.Len() < 8) { Version.Add(Part); }
			continue;
		}
		if (Family.IsEmpty()) { Family = Part.Left(1).ToUpper() + Part.Mid(1).ToLower(); }
	}
	if (Family.IsEmpty()) { return FString(); }
	return Version.IsEmpty() ? TEXT("Claude ") + Family : TEXT("Claude ") + Family + TEXT(" ") + FString::Join(Version, TEXT("."));
}

void FAscentACFAssistant::NoteClaudeResolvedModel(const FString& Alias, const FString& ResolvedId)
{
	if (Alias.IsEmpty() || ResolvedId.IsEmpty()) { return; }
	const FString* Known = ClaudeResolvedModels.Find(Alias);
	if (Known && *Known == ResolvedId) { return; }
	ClaudeResolvedModels.Add(Alias, ResolvedId);
	FObject Aliases = Object();
	for (const TPair<FString, FString>& Pair : ClaudeResolvedModels) { Aliases->SetStringField(Pair.Key, Pair.Value); }
	FObject Root = Object();
	Root->SetObjectField(TEXT("aliases"), Aliases);
	Save(ClaudeModelsPath(), Root);
	const FString Label = ClaudeModelLabel(ResolvedId);
	if (Label.IsEmpty()) { return; }
	for (const TSharedPtr<FACFAssistantModel>& Model : Models)
	{
		if (Model->Provider == EACFAssistantProvider::ClaudeCode && Model->Id == Alias) { Model->Label = Label; }
	}
	++ModelsRevision;
	Changed.Broadcast();
}

FString FAscentACFAssistant::ClaudeModelsPath() const
{
	return FPaths::Combine(ProfileDirectory, TEXT("claude_models.json"));
}

void FAscentACFAssistant::SetProviderModels(EACFAssistantProvider InProvider, TArray<TSharedPtr<FACFAssistantModel>>&& InModels)
{
	Models.RemoveAll([InProvider](const TSharedPtr<FACFAssistantModel>& Model) { return Model->Provider == InProvider; });
	if (IsProviderAvailable(InProvider)) { Models.Append(MoveTemp(InModels)); }
	// Yarin, ChatGPT, then Claude: the dropdown reads as stable groups whichever provider finished connecting first.
	auto GroupRank = [](EACFAssistantProvider Each)
	{
		return Each == EACFAssistantProvider::Yarin ? 0 : Each == EACFAssistantProvider::Codex ? 1 : 2;
	};
	Models.StableSort([&GroupRank](const TSharedPtr<FACFAssistantModel>& A, const TSharedPtr<FACFAssistantModel>& B)
	{
		return GroupRank(A->Provider) < GroupRank(B->Provider);
	});
	++ModelsRevision;
}

TSharedPtr<FACFAssistantModel> FAscentACFAssistant::FindModel(const FString& Id) const
{
	const TSharedPtr<FACFAssistantModel>* Match = Models.FindByPredicate([&Id](const TSharedPtr<FACFAssistantModel>& Model) { return Model->Id == Id; });
	return Match ? *Match : nullptr;
}

FString FAscentACFAssistant::DefaultModelFor(EACFAssistantProvider InProvider) const
{
	const TSharedPtr<FACFAssistantModel> Preferred = FindModel(ModelId);
	if (Preferred.IsValid() && Preferred->Provider == InProvider) { return ModelId; }
	const TSharedPtr<FACFAssistantModel>* Fallback = Models.FindByPredicate([InProvider](const TSharedPtr<FACFAssistantModel>& Model)
	{
		return Model->Provider == InProvider && Model->bDefault;
	});
	if (!Fallback) { Fallback = Models.FindByPredicate([InProvider](const TSharedPtr<FACFAssistantModel>& Model) { return Model->Provider == InProvider; }); }
	return Fallback ? (*Fallback)->Id : FString();
}

const TCHAR* FAscentACFAssistant::ProviderName(EACFAssistantProvider InProvider)
{
	return InProvider == EACFAssistantProvider::ClaudeCode ? TEXT("Claude") : InProvider == EACFAssistantProvider::Yarin ? TEXT("Yarin") : TEXT("ChatGPT");
}

FString FAscentACFAssistant::WorkModeContextText(const TSharedPtr<FACFAssistantSession>& Session) const
{
	const EACFAssistantWorkMode Mode = Session.IsValid() ? Session->Permissions.WorkMode : EACFAssistantWorkMode::Plan;
	FString ContextText = TEXT("Client work mode: ") + FACFAssistantPermissions::Label(Mode) + TEXT(" (this replaces any earlier work mode in this conversation)");
	const bool bAsksForApproval = Session.IsValid() && Session->Permissions.AsksForApproval();
	if (!Session.IsValid() || Session->Permissions.BlocksMutations())
	{
		ContextText += TEXT(". Mutating ue-editor tools are blocked by the Editor and return an error if called. Only read and inspect. Describe the proposed change. Do not ask for permission to edit.");
	}
	else if (bAsksForApproval)
	{
		ContextText += TEXT(". Mutating ue-editor tools are enabled; the Editor holds each one until the user presses Approve, so call them directly and never ask for permission in chat. ")
			TEXT("If a tool result says the user denied the change, do not retry it: explain what you intended and ask how to proceed.");
	}
	else
	{
		ContextText += TEXT(". Mutating ue-editor tools are enabled. When the request is clear, call them directly and do not ask for permission in chat.");
	}
	const EACFAssistantProvider TurnProvider = SessionProvider(Session);
	if (HarnessOf(TurnProvider) == EACFAssistantProvider::Codex)
	{
		const FString Project = CodexProjectDirectory();
		if (!Session.IsValid() || Session->Permissions.BlocksMutations())
		{
			ContextText += TEXT("\nProject files at ") + Project + TEXT(" are readable; the sandbox is read-only in this mode, so do not edit files.");
		}
		else if (bAsksForApproval)
		{
			ContextText += TEXT("\nProject files at ") + Project + TEXT(" are readable. The sandbox is read-only, but any file edit or command that writes is offered to the user for approval automatically: just attempt it, once. ")
				+ TEXT("A declined command or patch means the user said no; do not retry it.");
		}
		else
		{
			ContextText += TEXT("\nThe sandbox is now full access: edit any file under ") + Project
				+ TEXT(" directly with your file and shell tools, including C++ in Source and in every plugin under Plugins, .Build.cs, .uplugin, Config and docs, and run builds and scripts. ")
				+ TEXT("Do not route file edits through the Editor Python tool.");
		}
	}
	else
	{
		const UACFAssistantSettings& Settings = UACFAssistantSettings::Get();
		if (!Session.IsValid() || Session->Permissions.BlocksMutations())
		{
			ContextText += TEXT("\nProject files: read anything under the project root (Source, Plugins including every plugin's Source, Config, docs) and the engine source. Do not edit files in this mode.");
		}
		else
		{
			ContextText += Settings.bAllowSourceFileEdits
				? TEXT("\nProject files: you may read and edit any file under the project root, including C++ in Source and in every plugin under Plugins, .Build.cs, .uplugin, Config and docs. Engine source is read-only reference.")
				: TEXT("\nProject files: read anything under the project root and the engine source. File edits are turned off in Yarin settings.");
			ContextText += Settings.bAllowShell
				? TEXT(" Shell is available for builds, searches, scripts and git.")
				: TEXT(" Shell is turned off in Yarin settings.");
			if (bAsksForApproval)
			{
				ContextText += TEXT(" Every file edit and shell command is offered to the user for approval automatically; attempt it once and never ask in chat. A denied action means no; do not retry it.");
			}
		}
		ContextText += Settings.bAllowWeb ? TEXT(" Web search is available.") : TEXT(" Web search is turned off.");
	}
	const FString ContentFolder = UACFAssistantSettings::Get().ResolvedContentFolder();
	ContextText += TEXT("\nDefault content folder: ") + ContentFolder
		+ TEXT(". Save every new asset in this folder. Do not use /Game/AIGenerated or a second generated folder.");
	return ContextText;
}

FString FAscentACFAssistant::CodexProjectDirectory()
{
	FString Project = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
	FPaths::NormalizeDirectoryName(Project);
	FPaths::MakePlatformFilename(Project);
	return Project;
}

TSharedPtr<FJsonObject> FAscentACFAssistant::CodexSandboxPolicy(const TSharedPtr<FACFAssistantSession>& Session) const
{
	FObject Policy = Object();
	// Ask for approval also starts read-only: Codex then raises item/commandExecution/requestApproval and item/fileChange/requestApproval
	// for anything that writes, and each one becomes an Approve / Deny on the banner. Approve all lifts the sandbox for the conversation.
	if (!Session.IsValid() || Session->Permissions.BlocksMutations() || Session->Permissions.AsksForApproval())
	{
		Policy->SetStringField(TEXT("type"), TEXT("readOnly"));
		return Policy;
	}
	// Approve for me is unrestricted: files anywhere, shell, builds and network, like Claude Code's bypassPermissions.
	Policy->SetStringField(TEXT("type"), TEXT("dangerFullAccess"));
	return Policy;
}

const TCHAR* FAscentACFAssistant::CodexApprovalPolicy(const TSharedPtr<FACFAssistantSession>& Session) const
{
	return Session.IsValid() && Session->Permissions.AsksForApproval() ? TEXT("on-request") : TEXT("never");
}

void FAscentACFAssistant::NoteContextAnnounced(const TSharedPtr<FACFAssistantSession>& Session) const
{
	if (!Session.IsValid()) { return; }
	Session->bContextAnnounced = true;
	Session->AnnouncedWorkMode = Session->Permissions.WorkMode;
}

FString FAscentACFAssistant::TurnContextDelta(const TSharedPtr<FACFAssistantSession>& Session) const
{
	if (!Session.IsValid()) { return FString(); }
	if (!Session->bContextAnnounced)
	{
		NoteContextAnnounced(Session);
		return WorkModeContextText(Session) + TEXT("\ncall_tool takes toolset_name and tool_name separately. ")
			TEXT("Mesh from an image: toolset_name AscentAIAgent.AscentGenerationToolset, tool_name GenerateMeshFromImage, arguments filePath, assetName, bRig, heightMeters.");
	}
	if (Session->AnnouncedWorkMode == Session->Permissions.WorkMode) { return FString(); }
	NoteContextAnnounced(Session);
	return WorkModeContextText(Session);
}

FString FAscentACFAssistant::ClaudeTurnPrompt(const TSharedPtr<FACFAssistantSession>& Session) const
{
	const FString Delta = TurnContextDelta(Session);
	FString Prompt = Delta.IsEmpty() ? FString() : Delta + TEXT("\n\n");
	if (Session.IsValid() && !Session->PendingText.TrimStartAndEnd().IsEmpty())
	{
		Prompt += TEXT("\n\n") + Session->PendingText;
	}
	if (Session.IsValid())
	{
		for (const FString& ImagePath : Session->PendingImages)
		{
			FString LocalPath = FPaths::ConvertRelativePathToFull(ImagePath);
			FPaths::MakePlatformFilename(LocalPath);
			Prompt += TEXT("\n\nAttached image file: ") + LocalPath;
		}
	}
	return Prompt;
}

bool FAscentACFAssistant::HandleClaudeStreamLine(FACFAssistantSession& Session, const TSharedPtr<FJsonObject>& Message, FString& OutError)
{
	OutError.Reset();
	if (!Message) { return false; }
	const FString Type = String(Message, TEXT("type"));
	if (Type == TEXT("system"))
	{
		if (!String(Message, TEXT("session_id")).IsEmpty()) { Session.bClaudeHasStarted = true; }
		TArray<FString> Errors;
		for (const auto& Value : Array(Message, TEXT("mcp_server_errors")))
		{
			if (Value->Type == EJson::String && !Value->AsString().TrimStartAndEnd().IsEmpty()) { Errors.Add(Value->AsString()); }
			else if (Value->Type == EJson::Object)
			{
				const FString Error = String(Value->AsObject(), TEXT("error"));
				if (!Error.IsEmpty()) { Errors.Add(Error); }
			}
		}
		if (!Errors.IsEmpty())
		{
			OutError = TEXT("Yarin could not reach Unreal's Editor tools. Restart the Editor and Connect again.");
			return true;
		}
		if (Session.WorkStepCount == 0) { Session.Status = TEXT("Thinking..."); }
		return false;
	}
	if (Type == TEXT("stream_event"))
	{
		const FObject Event = Child(Message, TEXT("event"));
		const FString EventType = String(Event, TEXT("type"));
		if (EventType == TEXT("content_block_start"))
		{
			const FObject Block = Child(Event, TEXT("content_block"));
			if (String(Block, TEXT("type")) == TEXT("tool_use"))
			{
				// The arguments are still streaming here, so the real tool is not known yet: the completed assistant
				// message below names it. Saying a step started is what keeps the panel from looking stalled.
				Session.Status = StepStatus(Session.WorkStepCount + 1, TEXT("Preparing a tool call"));
			}
		}
		else if (EventType == TEXT("content_block_delta"))
		{
			const FObject Delta = Child(Event, TEXT("delta"));
			const FString DeltaType = String(Delta, TEXT("type"));
			FString Text = String(Delta, TEXT("text"));
			if (Text.IsEmpty()) { Text = String(Delta, TEXT("thinking")); }
			if (Text.IsEmpty()) { return false; }
			const bool bThinking = DeltaType.Contains(TEXT("thinking"));
			const TCHAR* Role = bThinking ? TEXT("Reasoning") : TEXT("Assistant");
			FString& LastItem = bThinking ? Session.LastReasoningItem : Session.LastAssistantItem;
			if (Session.Messages.IsEmpty() || Session.Messages.Last().Role != Role || LastItem != Role)
			{
				Session.Messages.Add({Role, FString()});
				LastItem = Role;
			}
			Session.Messages.Last().Text += Text;
			Session.Status = bThinking ? TEXT("Thinking...") : TEXT("Answering...");
		}
		return false;
	}
	if (Type == TEXT("assistant"))
	{
		const FObject Assistant = Child(Message, TEXT("message"));
		const FString MessageId = String(Assistant, TEXT("id"));
		// Every API call of a turn reports the whole conversation it was given. The last one is the context size now; the result below sums all calls and would count the same cached prompt once per tool step.
		const int64 CallContext = ClaudeCallContextTokens(Child(Assistant, TEXT("usage")));
		if (CallContext > 0) { Session.UsedContextTokens = CallContext; }
		for (const auto& Value : Array(Assistant, TEXT("content")))
		{
			if (Value->Type != EJson::Object) { continue; }
			const FObject Block = Value->AsObject();
			const FString BlockType = String(Block, TEXT("type"));
			if (BlockType == TEXT("text"))
			{
				const FString Text = String(Block, TEXT("text"));
				if (Text.IsEmpty()) { continue; }
				if (Session.LastAssistantItem == TEXT("Assistant") && !Session.Messages.IsEmpty() && Session.Messages.Last().Role == TEXT("Assistant"))
				{
					continue;
				}
				if (Session.LastAssistantItem != MessageId)
				{
					Session.Messages.Add({TEXT("Assistant"), Text});
					Session.LastAssistantItem = MessageId;
				}
			}
			else if (BlockType == TEXT("tool_use"))
			{
				const FString ToolLabel = ToolActivityLabel(Block);
				if (!ToolLabel.IsEmpty() && (Session.Messages.IsEmpty() || Session.Messages.Last().Text != ToolLabel))
				{
					Session.Messages.Add({TEXT("Tool"), ToolLabel});
					Session.Status = StepStatus(++Session.WorkStepCount, ToolLabel);
				}
			}
		}
		return false;
	}
	if (Type == TEXT("user"))
	{
		const FObject User = Child(Message, TEXT("message"));
		for (const auto& Value : Array(User, TEXT("content")))
		{
			if (Value->Type != EJson::Object) { continue; }
			const FObject Block = Value->AsObject();
			if (String(Block, TEXT("type")) != TEXT("tool_result")) { continue; }
			FString Output;
			const TSharedPtr<FJsonValue> Content = Block->TryGetField(TEXT("content"));
			if (Content && Content->Type == EJson::String) { Output = Content->AsString(); }
			if (Output.IsEmpty()) { Output = String(Block, TEXT("content")); }
			Output.TrimStartAndEndInline();
			if (!Output.IsEmpty()) { Session.Messages.Add({TEXT("ToolOutput"), Output.Left(4000)}); }
			Session.Status = StepStatus(Session.WorkStepCount, TEXT("Thinking about the next step"));
		}
		return false;
	}
	if (Type == TEXT("result"))
	{
		Session.bClaudeGotResult = true;
		bool bError = false;
		Message->TryGetBoolField(TEXT("is_error"), bError);
		const FString ResultText = String(Message, TEXT("result"));
		if (bError || String(Message, TEXT("subtype")).Contains(TEXT("error")))
		{
			OutError = ResultText.IsEmpty() ? TEXT("Yarin could not complete this response. Try again; if it repeats, reconnect from Settings.") : ResultText.Left(1500);
			return true;
		}
		if (!ResultText.IsEmpty() && (Session.Messages.IsEmpty() || Session.Messages.Last().Role != TEXT("Assistant")))
		{
			Session.Messages.Add({TEXT("Assistant"), ResultText});
			Session.LastAssistantItem = TEXT("result");
		}
		// modelUsage names every model the turn touched. The conversation's own model carried the context, so the entry with the most input is the one whose window counts.
		// Sonnet 5.5 reports 1,000,000 here; the catalog does not carry it.
		double Window = 0;
		double BestLoad = -1;
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Child(Message, TEXT("modelUsage"))->Values)
		{
			if (!Pair.Value.IsValid() || Pair.Value->Type != EJson::Object) { continue; }
			const FObject Entry = Pair.Value->AsObject();
			double EntryWindow = 0;
			Entry->TryGetNumberField(TEXT("contextWindow"), EntryWindow);
			double Load = 0;
			for (const TCHAR* Key : {TEXT("inputTokens"), TEXT("cacheReadInputTokens"), TEXT("cacheCreationInputTokens")})
			{
				double Value = 0;
				Entry->TryGetNumberField(Key, Value);
				Load += Value;
			}
			if (EntryWindow > 0 && Load > BestLoad) { BestLoad = Load; Window = EntryWindow; }
		}
		if (Window > 0) { Session.ReportedContextWindowTokens = static_cast<int64>(Window); }
		return true;
	}
	if (Type == TEXT("error"))
	{
		OutError = String(Message, TEXT("error"));
		if (OutError.IsEmpty()) { OutError = String(Child(Message, TEXT("error")), TEXT("message")); }
		if (OutError.IsEmpty()) { OutError = TEXT("The AI service reported a problem. Try again or reconnect."); }
		return true;
	}
	return false;
}

bool FAscentACFAssistant::IsSupportedCodexVersion(const FString& Version)
{
	const FString Prefix = TEXT("codex-cli ");
	const FString Text = Version.TrimStartAndEnd();
	if (!Text.StartsWith(Prefix)) { return false; }
	const FString VersionText = Text.Mid(Prefix.Len());
	int32 PrereleaseIndex = INDEX_NONE;
	int32 BuildIndex = INDEX_NONE;
	VersionText.FindChar(TEXT('-'), PrereleaseIndex);
	VersionText.FindChar(TEXT('+'), BuildIndex);
	int32 SuffixIndex = VersionText.Len();
	if (PrereleaseIndex != INDEX_NONE) { SuffixIndex = PrereleaseIndex; }
	if (BuildIndex != INDEX_NONE) { SuffixIndex = FMath::Min(SuffixIndex, BuildIndex); }
	TArray<FString> Parts;
	VersionText.Left(SuffixIndex).ParseIntoArray(Parts, TEXT("."), true);
	if (Parts.Num() != 3 || !Parts[0].IsNumeric() || !Parts[1].IsNumeric() || !Parts[2].IsNumeric()) { return false; }
	int32 Major = 0;
	int32 Minor = 0;
	int32 Patch = 0;
	if (!LexTryParseString(Major, *Parts[0]) || !LexTryParseString(Minor, *Parts[1]) || !LexTryParseString(Patch, *Parts[2])) { return false; }
	return Major > 0 || Minor > 153 || (Minor == 153 && Patch >= 4);
}

FString FAscentACFAssistant::CodexRuntimeConfiguration(const FString& GatewayBaseUrl)
{
	// With the sandboxed shell enabled, Codex commands are confined by its own sandbox to the isolated Workspace folder (with network), which lets the built-in imagegen skill create images
	// from the ChatGPT subscription without an API key. With it disabled the profile is read-only and shell-less.
	const bool bShell = UACFAssistantSettings::Get().bEnableCodexShell;
	FString Configuration = TEXT("forced_login_method = \"chatgpt\"\r\napproval_policy = \"never\"\r\n");
	Configuration += bShell ? TEXT("sandbox_mode = \"workspace-write\"\r\n") : TEXT("sandbox_mode = \"read-only\"\r\n");
	Configuration += UACFAssistantSettings::Get().bAllowWeb ? TEXT("web_search = \"live\"\r\n") : TEXT("web_search = \"disabled\"\r\n");
	Configuration += TEXT("project_doc_max_bytes = 0\r\n");
	if (bShell) { Configuration += TEXT("[sandbox_workspace_write]\r\nnetwork_access = true\r\n"); }
	Configuration += TEXT("[features]\r\napps = false\r\nplugins = false\r\nhooks = false\r\n");
	Configuration += bShell ? TEXT("shell_tool = true\r\nunified_exec = true\r\nshell_snapshot = false\r\n") : TEXT("shell_tool = false\r\nunified_exec = false\r\nshell_snapshot = false\r\n");
	Configuration += TEXT("code_mode_host = true\r\nbrowser_use = false\r\nview_image = false\r\n");
	Configuration += bShell
		? TEXT("multi_agent = false\r\nmulti_agent_v2 = false\r\nskip_host_skill_discovery = false\r\n")
		: TEXT("multi_agent = false\r\nmulti_agent_v2 = false\r\nskip_host_skill_discovery = true\r\n");
	Configuration += TEXT("[analytics]\r\nenabled = false\r\n");
	// default_tools_approval_mode marks Yarin's own loopback endpoint as trusted. Without it, approval_policy "never" makes Codex reject every ue-editor call as "requires approval",
	// because Epic's tools carry no readOnlyHint annotation and therefore all count as approval-needing writes. Approval of Editor changes is the
	// work-mode gate's job: the URL below is the gate, which relays to the Editor tool server after judging each tools/call.
	// tool_timeout_sec covers a call held on the Approve banner; Codex's default of 60 seconds would fail the tool while the user is still reading.
	Configuration += TEXT("[mcp_servers.ue-editor]\r\nurl = \"") + ACFAssistantToolGate::Url() + TEXT("\"\r\ndefault_tools_approval_mode = \"approve\"\r\ntool_timeout_sec = 3600\r\n");
	// The Yarin cloud gateway as a second model provider. Its key is never written here: env_key names the variable the process is started with.
	// This is a table, so it must stay below every bare key above.
	if (IsProviderAvailable(EACFAssistantProvider::Yarin) && IsUsableGatewayUrl(GatewayBaseUrl))
	{
		Configuration += TEXT("[model_providers.yarin]\r\nname = \"Yarin\"\r\nbase_url = \"") + GatewayBaseUrl + TEXT("\"\r\nenv_key = \"YARIN_GATEWAY_API_KEY\"\r\nwire_api = \"responses\"\r\n");
	}
	// User-provided TOML (typically [mcp_servers.*] blocks) so any external MCP server becomes available to the model alongside the built-in tools.
	const FString Extra = UACFAssistantSettings::Get().ExtraCodexConfigurationToml;
	if (!Extra.TrimStartAndEnd().IsEmpty())
	{
		Configuration += TEXT("\r\n") + Extra.Replace(TEXT("\r\n"), TEXT("\n")).Replace(TEXT("\n"), TEXT("\r\n")) + TEXT("\r\n");
	}
	return Configuration;
}

FString FAscentACFAssistant::FindCodexExecutable()
{
#if PLATFORM_WINDOWS
	WCHAR Path[32768];
	const DWORD Length = ::SearchPathW(nullptr, L"codex.exe", nullptr, UE_ARRAY_COUNT(Path), Path, nullptr);
	if (Length > 0 && Length < UE_ARRAY_COUNT(Path)) { return Path; }
	const FString UserSettings = FPlatformProcess::UserSettingsDir();
	for (const FString& Candidate : {
		FPaths::Combine(UserSettings, TEXT("Programs/OpenAI/Codex/bin/codex.exe")),
		FPaths::Combine(UserSettings, TEXT("OpenAI/Codex/bin/codex.exe"))})
	{
		if (FPaths::FileExists(Candidate)) { return Candidate; }
	}
	TArray<FString> DesktopVersions;
	IFileManager::Get().FindFiles(DesktopVersions, *FPaths::Combine(UserSettings, TEXT("OpenAI/Codex/bin/*")), false, true);
	DesktopVersions.Sort();
	for (const FString& VersionDirectory : DesktopVersions)
	{
		const FString Candidate = FPaths::Combine(UserSettings, TEXT("OpenAI/Codex/bin"), VersionDirectory, TEXT("codex.exe"));
		if (FPaths::FileExists(Candidate)) { return Candidate; }
	}
#endif
	return FString();
}

FString FAscentACFAssistant::FindClaudeExecutable()
{
#if PLATFORM_WINDOWS
	WCHAR Path[32768];
	const DWORD Length = ::SearchPathW(nullptr, L"claude.exe", nullptr, UE_ARRAY_COUNT(Path), Path, nullptr);
	if (Length > 0 && Length < UE_ARRAY_COUNT(Path)) { return Path; }
	const FString Home = FPlatformProcess::UserHomeDir();
	for (const FString& Candidate : {
		FPaths::Combine(Home, TEXT(".local/bin/claude.exe")),
		FPaths::Combine(FPlatformProcess::UserSettingsDir(), TEXT("Programs/Claude/claude.exe"))})
	{
		if (FPaths::FileExists(Candidate)) { return Candidate; }
	}
	const DWORD LengthCmd = ::SearchPathW(nullptr, L"claude.cmd", nullptr, UE_ARRAY_COUNT(Path), Path, nullptr);
	if (LengthCmd > 0 && LengthCmd < UE_ARRAY_COUNT(Path)) { return Path; }
#else
	FString Which;
	int32 Code = -1;
	FPlatformProcess::ExecProcess(TEXT("/usr/bin/which"), TEXT("claude"), &Code, &Which, nullptr);
	Which.TrimStartAndEndInline();
	if (Code == 0 && !Which.IsEmpty() && FPaths::FileExists(Which)) { return Which; }
	const FString HomeCandidate = FPaths::Combine(FPlatformProcess::UserHomeDir(), TEXT(".local/bin/claude"));
	if (FPaths::FileExists(HomeCandidate)) { return HomeCandidate; }
#endif
	return FString();
}

FAscentACFAssistant::FAscentACFAssistant()
{
	const FString Project = FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath()).ToLower();
	ProfileDirectory = FPaths::Combine(FPlatformProcess::UserSettingsDir(), TEXT("ACFAssistant/NativeClient"), FMD5::HashAnsiString(*Project));
	WorkingDirectory = FPaths::Combine(ProfileDirectory, TEXT("Workspace"));
	PreferencesPath = FPaths::Combine(ProfileDirectory, TEXT("preferences.json"));
	IFileManager::Get().MakeDirectory(*WorkingDirectory, true);
	ModelId = UACFAssistantSettings::Get().ModelId;
	ReasoningEffort = UACFAssistantSettings::Get().ReasoningEffort;
	FString SavedAliases;
	if (FFileHelper::LoadFileToString(SavedAliases, *ClaudeModelsPath()))
	{
		// Held in a local: iterating Child(...)->Values directly would read an object the temporary already released.
		const FObject Aliases = Child(Parse(SavedAliases), TEXT("aliases"));
		for (const auto& Pair : Aliases->Values)
		{
			FString Resolved;
			if (Pair.Value.IsValid() && Pair.Value->TryGetString(Resolved) && !Resolved.IsEmpty()) { ClaudeResolvedModels.Add(FString(Pair.Key), Resolved); }
		}
	}
	LoadHistory();
	OpenSession();
	Codex.Status = TEXT("Not signed in.");
	Claude.Status = TEXT("Not signed in.");
	Yarin.Status = TEXT("Coming soon.");
	if (IsProviderAvailable(EACFAssistantProvider::Yarin)) { LoadYarinCredential(); }
}

FAscentACFAssistant::~FAscentACFAssistant()
{
	Shutdown();
}

void FAscentACFAssistant::Shutdown()
{
	if (bShutDown) { return; }
	bShutDown = true;
	Changed.Clear();
	StopClaudeLogin();
	Disconnect();
}

TSharedPtr<FACFAssistantSession> FAscentACFAssistant::OpenSession()
{
	TSharedPtr<FACFAssistantSession> Session = MakeShared<FACFAssistantSession>();
	Session->LocalId = NextSessionLocalId++;
	Session->GateToken = FGuid::NewGuid().ToString(EGuidFormats::Digits);
	// A new conversation starts in the work mode of the one it was opened from, so the user does not have to re-pick it every time.
	const TSharedPtr<FACFAssistantSession> Current = GetActiveSession();
	Session->Permissions.WorkMode = Current.IsValid() ? Current->Permissions.WorkMode : UACFAssistantSettings::Get().WorkMode;
	Session->ModelId = SessionModelId(Current);
	Session->Provider = SessionProvider(Session);
	Sessions.Add(Session);
	ActiveSessionId = Session->LocalId;
	return Session;
}

TSharedPtr<FACFAssistantSession> FAscentACFAssistant::FindSession(int32 LocalId) const
{
	if (LocalId == 0) { return nullptr; }
	const TSharedPtr<FACFAssistantSession>* Match = Sessions.FindByPredicate([LocalId](const TSharedPtr<FACFAssistantSession>& Session)
	{
		return Session->LocalId == LocalId;
	});
	return Match ? *Match : nullptr;
}

TSharedPtr<FACFAssistantSession> FAscentACFAssistant::FindSessionByThread(const FString& InThreadId) const
{
	if (InThreadId.IsEmpty()) { return nullptr; }
	const TSharedPtr<FACFAssistantSession>* Match = Sessions.FindByPredicate([&InThreadId](const TSharedPtr<FACFAssistantSession>& Session)
	{
		return Session->ThreadId == InThreadId;
	});
	return Match ? *Match : nullptr;
}

TSharedPtr<FACFAssistantSession> FAscentACFAssistant::FindSessionByTitleThread(const FString& InThreadId) const
{
	if (InThreadId.IsEmpty()) { return nullptr; }
	const TSharedPtr<FACFAssistantSession>* Match = Sessions.FindByPredicate([&InThreadId](const TSharedPtr<FACFAssistantSession>& Session)
	{
		return Session->TitleThreadId == InThreadId;
	});
	return Match ? *Match : nullptr;
}

TSharedPtr<FACFAssistantSession> FAscentACFAssistant::GetActiveSession() const
{
	if (const TSharedPtr<FACFAssistantSession> Session = FindSession(ActiveSessionId)) { return Session; }
	return Sessions.IsEmpty() ? nullptr : Sessions[0];
}

bool FAscentACFAssistant::IsSessionOpen(const FString& InThreadId) const
{
	return FindSessionByThread(InThreadId).IsValid();
}

void FAscentACFAssistant::ActivateSession(int32 LocalId)
{
	// Switching is pure presentation: nothing is unsubscribed, so the conversation left behind keeps answering and running tools.
	if (ActiveSessionId == LocalId || !FindSession(LocalId)) { return; }
	ActiveSessionId = LocalId;
	Changed.Broadcast();
}

void FAscentACFAssistant::CloseSession(int32 LocalId)
{
	const int32 Index = Sessions.IndexOfByPredicate([LocalId](const TSharedPtr<FACFAssistantSession>& Session)
	{
		return Session->LocalId == LocalId;
	});
	if (Index == INDEX_NONE) { return; }
	const TSharedPtr<FACFAssistantSession> Session = Sessions[Index];
	if (Session->bBusy) { Cancel(Session); }
	DenyApprovals(Session, nullptr);
	SaveSession(Session);
	StopClaudeProcess(Session);
	// Only Codex keeps a server-side subscription; a Claude thread is just a session id on disk.
	if (HarnessOf(Session->Provider) == EACFAssistantProvider::Codex && Codex.bInitialized && !Session->bNeedsThreadResume)
	{
		Unsubscribe(Session->ThreadId, true);
		Unsubscribe(Session->TitleThreadId, false);
	}
	Sessions.RemoveAt(Index);
	if (Sessions.IsEmpty()) { OpenSession(); }
	else if (ActiveSessionId == LocalId) { ActiveSessionId = Sessions[FMath::Min(Index, Sessions.Num() - 1)]->LocalId; }
	Changed.Broadcast();
}

bool FAscentACFAssistant::MakeRoomForConversation(int32 ProtectedSessionId)
{
	if (CanOpenAnotherConversation()) { return true; }
	const TSharedPtr<FACFAssistantSession>* Idle = Sessions.FindByPredicate([ProtectedSessionId](const auto& Candidate)
	{
		return Candidate->LocalId != ProtectedSessionId && Candidate->HistoryEntry.IsValid() && !Candidate->bBusy && Candidate->PendingImages.IsEmpty()
			&& Candidate->PendingText.IsEmpty() && Candidate->SteerText.IsEmpty() && Candidate->Approvals.IsEmpty()
			&& !Candidate->UserInputRequestId.IsValid();
	});
	if (!Idle) { return false; }
	CloseSession((*Idle)->LocalId);
	return true;
}

void FAscentACFAssistant::Unsubscribe(const FString& InThreadId, bool bCritical)
{
	if (InThreadId.IsEmpty()) { return; }
	FObject Params = Object();
	Params->SetStringField(TEXT("threadId"), InThreadId);
	Request(TEXT("thread/unsubscribe"), Params, TFunction<void(FObject)>(), bCritical);
}

const TArray<FACFAssistantMessage>& FAscentACFAssistant::GetMessages() const
{
	static const TArray<FACFAssistantMessage> Empty;
	const TSharedPtr<FACFAssistantSession> Session = GetActiveSession();
	return Session.IsValid() ? Session->Messages : Empty;
}

const TArray<FString>& FAscentACFAssistant::GetPendingImages() const
{
	static const TArray<FString> Empty;
	const TSharedPtr<FACFAssistantSession> Session = GetActiveSession();
	return Session.IsValid() ? Session->PendingImages : Empty;
}

FString FAscentACFAssistant::GetStatus() const
{
	const TSharedPtr<FACFAssistantSession> Session = GetActiveSession();
	if (!Session.IsValid()) { return FString(); }
	const FProviderLink& Owner = Link(SessionProvider(Session));
	return Session->Status.IsEmpty() && !Owner.IsReady() ? Owner.Status : Session->Status;
}

FString FAscentACFAssistant::ToolActivityLabel(const TSharedPtr<FJsonObject>& Item)
{
	FString Name = String(Item, TEXT("tool"));
	if (Name.IsEmpty()) { Name = String(Item, TEXT("name")); }
	if (Name.IsEmpty()) { Name = String(Item, TEXT("title")); }
	// Codex names the payload "arguments", Claude names it "input".
	FObject Arguments = Child(Item, TEXT("arguments"));
	if (Arguments->Values.IsEmpty()) { Arguments = Child(Item, TEXT("input")); }
	// The Editor tools arrive through a relay: the item names "call_tool" and the tool actually being run sits in its arguments.
	const FString Relayed = String(Arguments, TEXT("tool_name"));
	if (!Relayed.IsEmpty())
	{
		Name = Relayed;
		Arguments = Child(Arguments, TEXT("arguments"));
	}
	Name = ShortToolName(Name);
	const FString Summary = ToolArgumentSummary(Arguments);
	return Summary.IsEmpty() ? Name : Name + TEXT("  ") + Summary;
}

EACFAssistantWorkMode FAscentACFAssistant::GetWorkMode() const
{
	const TSharedPtr<FACFAssistantSession> Session = GetActiveSession();
	return Session.IsValid() ? Session->Permissions.WorkMode : UACFAssistantSettings::Get().WorkMode;
}

bool FAscentACFAssistant::IsPlanMode() const
{
	const TSharedPtr<FACFAssistantSession> Session = GetActiveSession();
	return !Session.IsValid() || Session->Permissions.BlocksMutations();
}

bool FAscentACFAssistant::IsApproveForMe() const
{
	const TSharedPtr<FACFAssistantSession> Session = GetActiveSession();
	return Session.IsValid() && Session->Permissions.AutoApproves();
}

bool FAscentACFAssistant::HasConversationApproval() const
{
	const TSharedPtr<FACFAssistantSession> Session = GetActiveSession();
	return Session.IsValid() && Session->Permissions.bConversationApproved;
}

bool FAscentACFAssistant::IsWaitingForApproval() const
{
	const TSharedPtr<FACFAssistantSession> Session = GetActiveSession();
	return Session.IsValid() && !Session->Approvals.IsEmpty();
}

int32 FAscentACFAssistant::GetPendingApprovalCount() const
{
	const TSharedPtr<FACFAssistantSession> Session = GetActiveSession();
	return Session.IsValid() ? Session->Approvals.Num() : 0;
}

FACFAssistantToolVerdict FAscentACFAssistant::JudgeMutation(const FString& ConversationToken) const
{
	TArray<TSharedPtr<FACFAssistantSession>> Candidates;
	FACFAssistantToolVerdict Verdict;
	if (!ConversationToken.IsEmpty())
	{
		for (const TSharedPtr<FACFAssistantSession>& Session : Sessions)
		{
			if (Session.IsValid() && Session->GateToken == ConversationToken) { Candidates.Add(Session); }
		}
		if (Candidates.IsEmpty())
		{
			Verdict.Verdict = EACFAssistantToolVerdict::Block;
			Verdict.Reason = TEXT("Blocked: the conversation that made this call is no longer open.");
			return Verdict;
		}
	}
	else
	{
		for (const TSharedPtr<FACFAssistantSession>& Session : Sessions)
		{
			if (Session.IsValid() && Session->bBusy && HarnessOf(Session->Provider) == EACFAssistantProvider::Codex) { Candidates.Add(Session); }
		}
		if (Candidates.IsEmpty())
		{
			if (const TSharedPtr<FACFAssistantSession> Active = GetActiveSession()) { Candidates.Add(Active); }
		}
	}
	if (Candidates.IsEmpty())
	{
		Verdict.Verdict = EACFAssistantToolVerdict::Block;
		Verdict.Reason = TEXT("Blocked: no Yarin conversation is open to authorize changes.");
		return Verdict;
	}
	const TSharedPtr<FACFAssistantSession> Active = GetActiveSession();
	Candidates.Sort([&Active](const TSharedPtr<FACFAssistantSession>& A, const TSharedPtr<FACFAssistantSession>& B) { return A == Active && B != Active; });
	for (const TSharedPtr<FACFAssistantSession>& Session : Candidates)
	{
		if (Session->Permissions.BlocksMutations())
		{
			Verdict.Verdict = EACFAssistantToolVerdict::Block;
			Verdict.Session = Session;
			Verdict.Reason = FString::Printf(TEXT("Blocked by the Yarin work mode \"%s\": Editor changes are not allowed in this conversation. Describe the change instead, or ask the user to switch to \"Ask for approval\" or \"Approve for me\"."),
				*FACFAssistantPermissions::Label(Session->Permissions.WorkMode));
			return Verdict;
		}
	}
	for (const TSharedPtr<FACFAssistantSession>& Session : Candidates)
	{
		if (Session->Permissions.AsksForApproval())
		{
			Verdict.Verdict = EACFAssistantToolVerdict::Ask;
			Verdict.Session = Session;
			return Verdict;
		}
	}
	Verdict.Verdict = EACFAssistantToolVerdict::Allow;
	Verdict.Session = Candidates[0];
	return Verdict;
}

FACFAssistantToolVerdict FAscentACFAssistant::JudgeEditorTool(const FString& McpToolName, const TSharedPtr<FJsonObject>& McpArguments, const FString& ConversationToken) const
{
	const ACFAssistantToolPolicy::FToolCall Call = ACFAssistantToolPolicy::Unwrap(McpToolName, McpArguments);
	if (ACFAssistantToolPolicy::RequiresIndividualApproval(Call))
	{
		FACFAssistantToolVerdict Verdict = JudgeMutation(ConversationToken);
		if (Verdict.Verdict == EACFAssistantToolVerdict::Block) { return Verdict; }
		Verdict.Verdict = EACFAssistantToolVerdict::Ask;
		Verdict.bAlwaysAsk = true;
		return Verdict;
	}
	if (ACFAssistantToolPolicy::IsReadOnly(Call))
	{
		FACFAssistantToolVerdict Verdict;
		Verdict.Verdict = EACFAssistantToolVerdict::Allow;
		Verdict.Session = GetActiveSession();
		return Verdict;
	}
	return JudgeMutation(ConversationToken);
}

TSharedPtr<FACFAssistantApproval> FAscentACFAssistant::RequestApproval(const TSharedPtr<FACFAssistantSession>& Session, const FString& Kind,
	const FString& Title, const TSharedPtr<FJsonObject>& Preview, TFunction<void(bool bApproved)> Decide, bool bAlwaysAsk)
{
	TSharedPtr<FACFAssistantApproval> Approval = MakeShared<FACFAssistantApproval>();
	Approval->Id = NextApprovalId++;
	Approval->Kind = Kind;
	Approval->Title = Title;
	Approval->Preview = Preview;
	Approval->Requested = FPlatformTime::Seconds();
	Approval->bAlwaysAsk = bAlwaysAsk;
	Approval->Decide = MoveTemp(Decide);
	if (!Session.IsValid() || Session->Permissions.BlocksMutations())
	{
		// Nobody can approve for a closed or read-only conversation: answer at once so the caller never hangs.
		if (Approval->Decide) { Approval->Decide(false); Approval->Decide = nullptr; }
		return Approval;
	}
	if (!bAlwaysAsk && (Session->Permissions.AutoApproves() || Session->Permissions.bConversationApproved))
	{
		if (Approval->Decide) { Approval->Decide(true); Approval->Decide = nullptr; }
		return Approval;
	}
	Session->Approvals.Add(Approval);
	Session->Status = FString::Printf(TEXT("Waiting for your approval: %s"), *Title);
	UE_LOG(LogAscentACFAssistant, Log, TEXT("Yarin approval requested (%s): %s"), *Kind, *Title);
	Changed.Broadcast();
	return Approval;
}

void FAscentACFAssistant::SettleApproval(const TSharedPtr<FACFAssistantSession>& Session, const TSharedPtr<FACFAssistantApproval>& Approval, bool bApproved)
{
	if (!Approval.IsValid()) { return; }
	if (Session.IsValid()) { Session->Approvals.Remove(Approval); }
	// Take the callback first: Decide may re-enter (a forwarded tool completing synchronously) and must not run twice.
	TFunction<void(bool)> Decide = MoveTemp(Approval->Decide);
	Approval->Decide = nullptr;
	if (Decide) { Decide(bApproved); }
	if (Session.IsValid() && !bApproved) { Session->bApprovalDeniedThisTurn = true; }
}

void FAscentACFAssistant::DenyApprovals(const TSharedPtr<FACFAssistantSession>& Session, const TCHAR* StatusText)
{
	if (!Session.IsValid() || Session->Approvals.IsEmpty()) { return; }
	TArray<TSharedPtr<FACFAssistantApproval>> Held = MoveTemp(Session->Approvals);
	Session->Approvals.Reset();
	for (const TSharedPtr<FACFAssistantApproval>& Approval : Held)
	{
		SettleApproval(nullptr, Approval, false);
	}
	if (StatusText) { Session->Status = StatusText; }
	Changed.Broadcast();
}

void FAscentACFAssistant::ApproveConversation()
{
	const TSharedPtr<FACFAssistantSession> Session = GetActiveSession();
	if (!Session.IsValid() || Session->Permissions.BlocksMutations()) { return; }
	Session->Permissions.bConversationApproved = true;
	const TArray<TSharedPtr<FACFAssistantApproval>> Held = Session->Approvals;
	const TSharedPtr<FACFAssistantApproval> Visible = Held.IsEmpty() ? nullptr : Held[0];
	for (const TSharedPtr<FACFAssistantApproval>& Approval : Held)
	{
		if (Approval.IsValid() && (!Approval->bAlwaysAsk || Approval == Visible))
		{
			SettleApproval(Session, Approval, true);
		}
	}
	Session->Status = Session->Approvals.IsEmpty()
		? TEXT("Approved for this conversation. Changes run without asking until the work mode changes.")
		: FString::Printf(TEXT("Waiting for your approval: %s"), *Session->Approvals[0]->Title);
	SaveSession(Session);
	Changed.Broadcast();
}

void FAscentACFAssistant::HandleCodexApprovalRequest(const FObject& Message, const FString& Method, const FObject& Params)
{
	const TSharedPtr<FJsonValue> RequestId = Message->TryGetField(TEXT("id"));
	const bool bPermissions = Method == TEXT("item/permissions/requestApproval");
	const TSharedPtr<FACFAssistantSession> Session = FindSessionByThread(String(Params, TEXT("threadId")));
	auto Reply = [this, RequestId, bPermissions, Params](bool bApproved)
	{
		FObject Result = Object();
		if (bPermissions)
		{
			// Granting echoes the requested profile back for this turn; denying grants nothing.
			const FObject Requested = Child(Params, TEXT("permissions"));
			Result->SetObjectField(TEXT("permissions"), bApproved ? Requested : Object());
			Result->SetStringField(TEXT("scope"), TEXT("turn"));
		}
		else
		{
			Result->SetStringField(TEXT("decision"), bApproved ? TEXT("accept") : TEXT("decline"));
		}
		FObject Response = Object();
		Response->SetField(TEXT("id"), RequestId);
		Response->SetObjectField(TEXT("result"), Result);
		Write(Response);
	};
	if (!Session.IsValid() || !Session->Permissions.AsksForApproval())
	{
		// Approve for me never reaches here (approval policy "never"); anything else that does is refused, as before.
		Reply(Session.IsValid() && (Session->Permissions.AutoApproves() || Session->Permissions.bConversationApproved));
		return;
	}
	FString Kind = TEXT("command");
	FString Title = String(Params, TEXT("command"));
	if (Method == TEXT("item/fileChange/requestApproval")) { Kind = TEXT("file-change"); Title = TEXT("apply_patch (file changes)"); }
	else if (bPermissions) { Kind = TEXT("permissions"); Title = TEXT("additional sandbox permissions"); }
	if (Title.IsEmpty()) { Title = Kind; }
	const FObject Preview = Object();
	Preview->SetStringField(TEXT("tool"), Title);
	Preview->SetStringField(TEXT("kind"), Kind);
	const FString Reason = String(Params, TEXT("reason"));
	if (!Reason.IsEmpty()) { Preview->SetStringField(TEXT("reason"), Reason); }
	const FString Cwd = String(Params, TEXT("cwd"));
	if (!Cwd.IsEmpty()) { Preview->SetStringField(TEXT("cwd"), Cwd); }
	if (bPermissions) { Preview->SetObjectField(TEXT("arguments"), Child(Params, TEXT("permissions"))); }
	RequestApproval(Session, Kind, Title.Left(200), Preview, Reply);
}

bool FAscentACFAssistant::IsBusy() const
{
	const TSharedPtr<FACFAssistantSession> Session = GetActiveSession();
	return Session.IsValid() && Session->bBusy;
}

bool FAscentACFAssistant::IsAnyBusy() const
{
	return Sessions.ContainsByPredicate([](const TSharedPtr<FACFAssistantSession>& Session) { return Session->bBusy; });
}

bool FAscentACFAssistant::IsAnyBusy(EACFAssistantProvider InProvider) const
{
	return Sessions.ContainsByPredicate([InProvider](const TSharedPtr<FACFAssistantSession>& Session)
	{
		return Session->bBusy && HarnessOf(Session->Provider) == HarnessOf(InProvider);
	});
}

void FAscentACFAssistant::SavePreferences()
{
	UACFAssistantSettings& Settings = UACFAssistantSettings::Get();
	Settings.ModelId = ModelId;
	Settings.ReasoningEffort = ReasoningEffort;
	Settings.WorkMode = GetWorkMode();
	Settings.SaveConfig();
	FObject Preferences = Object();
	Preferences->SetStringField(TEXT("model"), ModelId);
	Preferences->SetStringField(TEXT("reasoningEffort"), ReasoningEffort);
	Preferences->SetNumberField(TEXT("workMode"), static_cast<int32>(GetWorkMode()));
	Save(PreferencesPath, Preferences);
}

void FAscentACFAssistant::ReloadExecutablesFromSettings()
{
	// Paths are owned by Project Settings; Connect always re-reads them.
}

void FAscentACFAssistant::SyncSkills(const FString& DestinationSkillsDirectory)
{
	const TSharedPtr<IPlugin> Plugin = ACFAssistantPlugin::Owner();
	if (!Plugin) { return; }
	const FString Source = FPaths::ConvertRelativePathToFull(Plugin->GetBaseDir() / TEXT("Resources/Skills"));
	if (!IFileManager::Get().DirectoryExists(*Source)) { return; }
	const FString MarkerPath = DestinationSkillsDirectory / TEXT(".acf_skills_version");
	const FString PluginVersion = Plugin->GetDescriptor().VersionName;
	FString InstalledVersion;
	FFileHelper::LoadFileToString(InstalledVersion, *MarkerPath);
	if (InstalledVersion.Equals(PluginVersion)) { return; }
	const bool bSynced = FPlatformFileManager::Get().GetPlatformFile().CopyDirectoryTree(*DestinationSkillsDirectory, *Source, true);
	if (bSynced) { FFileHelper::SaveStringToFile(PluginVersion, *MarkerPath); }
	UE_LOG(LogAscentACFAssistant, Log, TEXT("Yarin skills sync to %s: %s (version %s)"), *DestinationSkillsDirectory, bSynced ? TEXT("done") : TEXT("failed"), *PluginVersion);
}

FString FAscentACFAssistant::GetPastedImagesDirectory() const
{
	const FString Directory = FPaths::ConvertRelativePathToFull(WorkingDirectory / TEXT("PastedImages"));
	IFileManager::Get().MakeDirectory(*Directory, true);
	return Directory;
}

void FAscentACFAssistant::AttachImage(const FString& Path)
{
	const TSharedPtr<FACFAssistantSession> Session = GetActiveSession();
	if (!Session.IsValid() || Session->PendingImages.Num() >= 8) { return; }
	FString Source = FPaths::ConvertRelativePathToFull(Path);
	FPaths::NormalizeFilename(Source);
	if (!FPaths::FileExists(Source)) { return; }
	FString Workspace = FPaths::ConvertRelativePathToFull(WorkingDirectory);
	FPaths::NormalizeDirectoryName(Workspace);
	FString Attached = Source;
	if (!FPaths::IsUnderDirectory(Source, Workspace))
	{
		const FString Destination = FPaths::ConvertRelativePathToFull(GetPastedImagesDirectory() / FPaths::GetCleanFilename(Source));
		IFileManager::Get().Delete(*Destination, false, true, true);
		if (FPlatformFileManager::Get().GetPlatformFile().CopyFile(*Destination, *Source)) { Attached = Destination; }
	}
	FPaths::MakePlatformFilename(Attached);
	if (!Session->PendingImages.Contains(Attached))
	{
		Session->PendingImages.Add(Attached);
		Session->Status = FString::Printf(TEXT("%d image(s) attached to the next message."), Session->PendingImages.Num());
		Changed.Broadcast();
	}
}

void FAscentACFAssistant::RemoveImage(int32 Index)
{
	const TSharedPtr<FACFAssistantSession> Session = GetActiveSession();
	if (Session.IsValid() && Session->PendingImages.IsValidIndex(Index))
	{
		Session->PendingImages.RemoveAt(Index);
		Changed.Broadcast();
	}
}

void FAscentACFAssistant::Request(const FString& Method, const TSharedPtr<FJsonObject>& Params, TFunction<void(TSharedPtr<FJsonObject>)> Callback,
	bool bCritical, int32 SessionId)
{
	const int32 Id = NextRequest++;
	FObject Message = Object();
	Message->SetNumberField(TEXT("id"), Id);
	Message->SetStringField(TEXT("method"), Method);
	Message->SetObjectField(TEXT("params"), Params);
	Pending.Add(Id, {Method, FPlatformTime::Seconds(), bCritical, SessionId, MoveTemp(Callback)});
	// A request is activity. The 5-minute watchdog measures silence during a turn; without this stamp it
	// counts the idle time since the last byte, so the first message after a quiet connection is killed at once.
	Codex.LastActivity = FPlatformTime::Seconds();
	Write(Message);
}

void FAscentACFAssistant::Fail(EACFAssistantProvider InProvider, const FString& Message)
{
	// The protocol log exists for exactly this moment, so it must be on disk before anything else happens.
	FlushProtocolLog();
	UE_LOG(LogAscentACFAssistant, Warning, TEXT("%s failure: %s"), ProviderName(InProvider), *Message);
	FProviderLink& Failed = Link(InProvider);
	if (!Failed.IsReady()) { Failed.State = EConnectionState::Failed; Failed.Failure = Message.Left(1500); }
	Failed.Status = Message.Left(1500);
	for (const TSharedPtr<FACFAssistantSession>& Session : Sessions)
	{
		// A dead Codex runtime ends the Yarin conversations on it too.
		if (Session->Provider != InProvider && HarnessOf(Session->Provider) != InProvider) { continue; }
		Session->bBusy = false;
		Session->PendingText.Reset();
	}
	FailOneShots(InProvider, Message.Left(1500));
	Changed.Broadcast();
	// Yarin's models run on this process, so its failure is theirs.
	if (InProvider == EACFAssistantProvider::Codex) { FinishYarinConnect(); }
}

void FAscentACFAssistant::FailSession(const TSharedPtr<FACFAssistantSession>& Session, const FString& Message)
{
	// Without a conversation the failure came from the shared Codex process (Claude has no connection-level requests).
	if (!Session.IsValid()) { Fail(EACFAssistantProvider::Codex, Message); return; }
	FlushProtocolLog();
	UE_LOG(LogAscentACFAssistant, Warning, TEXT("Conversation failure: %s"), *Message);
	DenyApprovals(Session, nullptr);
	StopClaudeProcess(Session);
	Session->Status = Message.Left(1500);
	Session->bBusy = false;
	Session->bCancelRequested = false;
	Session->PendingText.Reset();
	Session->SteerText.Reset();
	Changed.Broadcast();
}

void FAscentACFAssistant::CompleteClaudeTurn(const TSharedPtr<FACFAssistantSession>& Session, const FString& Error)
{
	if (!Session.IsValid()) { return; }
	DenyApprovals(Session, nullptr);
	StopClaudeProcess(Session);
	if (!Error.IsEmpty())
	{
		Session->Messages.Add({TEXT("System"), Error.Left(1500)});
		Session->Status = TEXT("The answer failed. Try again.");
		// `claude auth status` only reads the local credentials, so an expired or revoked sign-in still connects and fails here.
		// Dropping to SignedOut puts "Sign in with Claude" back on the account button.
		if (IsClaudeAuthFailure(Error))
		{
			const FString Expired = TEXT("Your Claude sign-in expired. Open Settings (gear icon) and press Sign in with Claude.");
			Session->Status = Expired;
			// Leaving Ready stops the Claude ticker, so another Claude conversation still running would stay busy forever. Codex ones are untouched.
			for (const TSharedPtr<FACFAssistantSession>& Other : Sessions)
			{
				if (!Other.IsValid() || Other == Session || !Other->bBusy || Other->Provider != EACFAssistantProvider::ClaudeCode) { continue; }
				StopClaudeProcess(Other);
				Other->bBusy = false;
				Other->PendingText.Reset();
				Other->SteerText.Reset();
				Other->Status = Expired;
			}
			FailOneShots(EACFAssistantProvider::ClaudeCode, Expired);
			EnterSignedOut(EACFAssistantProvider::ClaudeCode, Expired);
		}
	}
	else
	{
		Session->Status = Session->bCancelRequested ? TEXT("Stopped. You can continue this conversation.") : TEXT("Ready.");
	}
	Session->bBusy = false;
	Session->TurnId.Reset();
	SaveSession(Session);
	if (!Session->PendingTitleSource.IsEmpty()) { StartTitleGeneration(Session); }
	if (!Session->SteerText.IsEmpty() && Claude.IsReady() && !Session->ThreadId.IsEmpty())
	{
		Session->PendingText = Session->SteerText;
		Session->SteerText.Reset();
		Session->bBusy = true;
		Session->bCancelRequested = false;
		Session->bApprovalDeniedThisTurn = false;
		Session->LastAssistantItem.Reset();
		Session->LastReasoningItem.Reset();
		Session->WorkStepCount = 0;
		Session->Status = TEXT("Redirected. Working...");
		StartClaudeTurn(Session);
	}
	Changed.Broadcast();
}

void FAscentACFAssistant::LogProtocol(const TCHAR* Direction, const FString& Line)
{
	if (ProtocolLogBytes > 4 * 1024 * 1024) { return; }
	const FString Stamp = FDateTime::Now().ToString(TEXT("%H:%M:%S.%s"));
	const FString Entry = Stamp + TEXT(" ") + Direction + TEXT(" ") + Line.Left(4000) + TEXT("\r\n");
	ProtocolLogBytes += Entry.Len();
	ProtocolLogPending += Entry;
	// A long burst must not grow the buffer without bound before the tick ends.
	if (ProtocolLogPending.Len() > 64 * 1024) { FlushProtocolLog(); }
}

void FAscentACFAssistant::FlushProtocolLog()
{
	if (ProtocolLogPending.IsEmpty()) { return; }
	FFileHelper::SaveStringToFile(ProtocolLogPending, *FPaths::Combine(ProfileDirectory, TEXT("protocol.log")),
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), EFileWrite::FILEWRITE_Append);
	ProtocolLogPending.Reset();
}

void FAscentACFAssistant::AutoConnect()
{
	// Both providers, each only from rest: a SignedOut or SigningIn one already knows where it stands, and reconnecting would kill a sign-in in progress.
	for (const EACFAssistantProvider Each : {EACFAssistantProvider::Codex, EACFAssistantProvider::ClaudeCode, EACFAssistantProvider::Yarin})
	{
		if (!IsProviderAvailable(Each)) { continue; }
		const EConnectionState State = Link(Each).State;
		if ((State == EConnectionState::Disconnected || State == EConnectionState::Failed) && !IsAnyBusy(Each)) { Connect(Each, false); }
	}
}

void FAscentACFAssistant::SignIn(EACFAssistantProvider InProvider)
{
	if (!IsProviderAvailable(InProvider)) { return; }
	const EConnectionState State = Link(InProvider).State;
	if (State == EConnectionState::Ready || State == EConnectionState::Connecting || State == EConnectionState::SigningIn || IsAnyBusy(InProvider)) { return; }
	if (State == EConnectionState::SignedOut)
	{
		if (InProvider == EACFAssistantProvider::ClaudeCode) { LoginClaude(); }
		else if (InProvider == EACFAssistantProvider::Yarin) { StartYarinLogin(); }
		else { StartCodexLogin(); }
		return;
	}
	Connect(InProvider, true);
}

void FAscentACFAssistant::CancelSignIn(EACFAssistantProvider InProvider)
{
	if (Link(InProvider).State != EConnectionState::SigningIn) { return; }
	if (InProvider == EACFAssistantProvider::ClaudeCode) { StopClaudeLogin(); }
	else if (InProvider == EACFAssistantProvider::Yarin) { StopYarinLogin(); }
	else { CancelCodexLogin(); }
	EnterSignedOut(InProvider, TEXT("Sign-in canceled."));
}

void FAscentACFAssistant::SignOut(EACFAssistantProvider InProvider)
{
	if (InProvider == EACFAssistantProvider::Yarin)
	{
		if (Yarin.State == EConnectionState::Ready && !IsAnyBusy(InProvider)) { SignOutYarin(); }
		return;
	}
	if (!IsProviderReady(InProvider) || IsAnyBusy(InProvider)) { return; }
	if (InProvider == EACFAssistantProvider::ClaudeCode)
	{
		Disconnect(EACFAssistantProvider::ClaudeCode);
		Claude.State = EConnectionState::Connecting;
		Claude.Status = TEXT("Signing out of Claude Code...");
		Changed.Broadcast();
		StartClaudeAuthCheck(true);
		return;
	}
	Codex.State = EConnectionState::Connecting;
	Codex.Status = TEXT("Signing out of ChatGPT...");
	Changed.Broadcast();
	Request(TEXT("account/logout"), Object(), [this](FObject) { EnterSignedOut(EACFAssistantProvider::Codex, TEXT("Signed out of ChatGPT.")); });
}

void FAscentACFAssistant::EnterSignedOut(EACFAssistantProvider InProvider, const FString& Message)
{
	FProviderLink& Out = Link(InProvider);
	Out.State = EConnectionState::SignedOut;
	Out.Failure.Reset();
	Out.bAuthenticated = false;
	// Yarin is "initialized" only while the Codex runtime carries this account's key; signed out, it carries nothing of it.
	if (InProvider == EACFAssistantProvider::Yarin) { Out.bInitialized = false; }
	Out.AccountLabel.Reset();
	Out.Status = Message;
	// A signed-out provider's models leave the shared dropdown, so nothing offers a model that cannot answer.
	SetProviderModels(InProvider, {});
	Changed.Broadcast();
}

FString FAscentACFAssistant::GetConnectionSummary(EACFAssistantProvider InProvider) const
{
	if (!IsProviderAvailable(InProvider)) { return TEXT("Coming soon"); }
	const FProviderLink& Of = Link(InProvider);
	switch (Of.State)
	{
	case EConnectionState::Ready:
		return TEXT("Signed in");
	case EConnectionState::Failed:
		return Of.Failure.IsEmpty() ? FString(TEXT("Could not connect. Press Try again.")) : Of.Failure;
	case EConnectionState::Disconnected:
	case EConnectionState::SignedOut:
		return TEXT("Not signed in");
	default:
		return Of.Status;
	}
}

void FAscentACFAssistant::Connect(EACFAssistantProvider InProvider, bool bThenSignIn)
{
	if (!IsProviderAvailable(InProvider)) { return; }
	const EConnectionState State = Link(InProvider).State;
	if (State == EConnectionState::Connecting || State == EConnectionState::SigningIn || IsAnyBusy(InProvider)) { return; }
	if (!bProtocolLogCleared)
	{
		// Once per editor run, not per connect: two providers connecting side by side would erase each other's lines.
		bProtocolLogCleared = true;
		IFileManager::Get().Delete(*FPaths::Combine(ProfileDirectory, TEXT("protocol.log")), false, true, true);
		ProtocolLogPending.Reset();
		ProtocolLogBytes = 0;
	}
	Disconnect(InProvider);
	FProviderLink& Starting = Link(InProvider);
	Starting.bSignInIfSignedOut = bThenSignIn;
	Starting.State = EConnectionState::Connecting;
	Starting.Failure.Reset();
	Starting.AccountLabel.Reset();
	Starting.Status = TEXT("Connecting...");
	Changed.Broadcast();
	if (InProvider == EACFAssistantProvider::ClaudeCode) { ConnectClaude(); }
	else if (InProvider == EACFAssistantProvider::Yarin) { ConnectYarin(); }
	else { ConnectCodex(); }
	Changed.Broadcast();
}

void FAscentACFAssistant::ReadAccount()
{
	Request(TEXT("account/read"), Object(), [this](FObject Result)
	{
		const FObject Account = Child(Result, TEXT("account"));
		const FString AuthMode = String(Account, TEXT("authMode"));
		if (AuthMode.Equals(TEXT("apikey"), ESearchCase::IgnoreCase) || AuthMode.Equals(TEXT("api_key"), ESearchCase::IgnoreCase))
		{
			Fail(EACFAssistantProvider::Codex, TEXT("Sign in with your subscription, not an API key."));
			return;
		}
		Codex.bAuthenticated = IsChatGPTSubscriptionAccount(Account);
		if (Codex.bAuthenticated)
		{
			const FString Email = String(Account, TEXT("email"));
			const FString Plan = String(Account, TEXT("planType"));
			Codex.AccountLabel = Email.IsEmpty() ? Plan : Plan.IsEmpty() ? Email : Email + TEXT(" (") + Plan + TEXT(")");
			Codex.Status = TEXT("Signed in. Loading models...");
			Codex.State = EConnectionState::Connecting;
			PendingCodexModels.Reset();
			RefreshModels();
			Changed.Broadcast();
			return;
		}
		EnterSignedOut(EACFAssistantProvider::Codex, TEXT("Sign in with your ChatGPT subscription (Plus, Pro, Team, or Enterprise) to use it in Yarin."));
		if (Codex.bSignInIfSignedOut) { StartCodexLogin(); }
	});
}

void FAscentACFAssistant::StartCodexLogin()
{
	if (!Codex.bInitialized) { Connect(EACFAssistantProvider::Codex, true); return; }
	Codex.bSignInIfSignedOut = false;
	CancelCodexLogin();
	Codex.State = EConnectionState::SigningIn;
	Codex.Failure.Reset();
	Codex.Status = TEXT("Opening the ChatGPT sign-in page...");
	Changed.Broadcast();
	FObject Params = Object();
	Params->SetStringField(TEXT("type"), TEXT("chatgpt"));
	Request(TEXT("account/login/start"), Params, [this](FObject Result)
	{
		const FString Url = String(Result, TEXT("authUrl"));
		const FString StartedId = String(Result, TEXT("loginId"));
		if (Codex.State != EConnectionState::SigningIn)
		{
			// Canceled before Codex answered: release the login it just opened, or it would wait for a browser nobody will use.
			LoginId = StartedId;
			CancelCodexLogin();
			return;
		}
		if (Url.StartsWith(TEXT("https://auth.openai.com/")) || Url.StartsWith(TEXT("https://chatgpt.com/")))
		{
			LoginId = StartedId;
			FPlatformProcess::LaunchURL(*Url, nullptr, nullptr);
			Codex.Status = TEXT("Finish signing in to ChatGPT in your browser. Yarin continues on its own.");
			Changed.Broadcast();
		}
		else { EnterSignedOut(EACFAssistantProvider::Codex, TEXT("Yarin did not receive a secure sign-in link. Try again.")); }
	});
}

void FAscentACFAssistant::CancelCodexLogin()
{
	if (LoginId.IsEmpty()) { return; }
	if (Codex.bInitialized)
	{
		FObject Params = Object();
		Params->SetStringField(TEXT("loginId"), LoginId);
		Request(TEXT("account/login/cancel"), Params, TFunction<void(FObject)>(), false);
	}
	LoginId.Reset();
}

void FAscentACFAssistant::RefreshModels(const FString& Cursor)
{
	FObject Params = Object();
	Params->SetNumberField(TEXT("limit"), 100);
	if (!Cursor.IsEmpty()) { Params->SetStringField(TEXT("cursor"), Cursor); }
	Request(TEXT("model/list"), Params, [this](FObject Result)
	{
		// Field names differ between app-server protocol revisions (camelCase) and the raw backend catalog (snake_case), so read every known spelling.
		auto FirstString = [](const FObject& Json, std::initializer_list<const TCHAR*> Keys)
		{
			for (const TCHAR* Key : Keys)
			{
				const FString Value = String(Json, Key);
				if (!Value.IsEmpty()) { return Value; }
			}
			return FString();
		};
		TArray<TSharedPtr<FJsonValue>> Data = Array(Result, TEXT("data"));
		if (Data.IsEmpty()) { Data = Array(Result, TEXT("models")); }
		if (Data.IsEmpty()) { Data = Array(Result, TEXT("items")); }
		for (const auto& Value : Data)
		{
			if (Value->Type != EJson::Object) { continue; }
			const FObject Item = Value->AsObject();
			bool bHidden = false;
			Item->TryGetBoolField(TEXT("hidden"), bHidden);
			if (bHidden || String(Item, TEXT("visibility")) == TEXT("hidden")) { continue; }
			auto Model = MakeShared<FACFAssistantModel>();
			Model->Id = FirstString(Item, {TEXT("model"), TEXT("id"), TEXT("slug")});
			if (Model->Id.IsEmpty()) { continue; }
			Model->Label = FirstString(Item, {TEXT("displayName"), TEXT("display_name")});
			double Window = 0;
			if (!Item->TryGetNumberField(TEXT("contextWindow"), Window)
				&& !Item->TryGetNumberField(TEXT("context_window"), Window)
				&& !Item->TryGetNumberField(TEXT("maxContextWindow"), Window))
			{
				Item->TryGetNumberField(TEXT("max_context_window"), Window);
			}
			Model->ContextWindowTokens = static_cast<int64>(Window);
			Model->Effort = FirstString(Item, {TEXT("defaultReasoningEffort"), TEXT("default_reasoning_level"), TEXT("default_reasoning_effort")});
			TArray<TSharedPtr<FJsonValue>> Efforts = Array(Item, TEXT("supportedReasoningEfforts"));
			if (Efforts.IsEmpty()) { Efforts = Array(Item, TEXT("supported_reasoning_levels")); }
			for (const auto& Effort : Efforts)
			{
				const FString Level = Effort->Type == EJson::Object ? FirstString(Effort->AsObject(), {TEXT("reasoningEffort"), TEXT("effort")})
					: Effort->Type == EJson::String ? Effort->AsString() : FString();
				if (Level.IsEmpty()) { continue; }
				Model->Efforts.AddUnique(Level);
				if (Level == TEXT("medium")) { Model->Effort = Level; }
			}
			bool bDefault = false;
			if (!Item->TryGetBoolField(TEXT("isDefault"), bDefault)) { Item->TryGetBoolField(TEXT("is_default"), bDefault); }
			Model->bDefault = bDefault;
			PendingCodexModels.Add(Model);
		}
		const FString Next = String(Result, TEXT("nextCursor"));
		if (!Next.IsEmpty()) { RefreshModels(Next); return; }
		// Every page is in: swap Codex's entries in one step, so the dropdown never shows a half list or loses the Claude models.
		SetProviderModels(EACFAssistantProvider::Codex, MoveTemp(PendingCodexModels));
		PendingCodexModels.Reset();
		UE_LOG(LogAscentACFAssistant, Log, TEXT("model/list returned %d usable Codex models; default: %s"),
			Models.FilterByPredicate([](const TSharedPtr<FACFAssistantModel>& Model) { return Model->Provider == EACFAssistantProvider::Codex; }).Num(),
			*DefaultModelFor(EACFAssistantProvider::Codex));
		if (DefaultModelFor(EACFAssistantProvider::Codex).IsEmpty()) { Fail(EACFAssistantProvider::Codex, TEXT("No available models were returned for this account.")); return; }
		Codex.State = EConnectionState::Ready;
		Codex.Status = TEXT("Connected. Yarin is ready.");
		Changed.Broadcast();
	});
}

FString FAscentACFAssistant::SessionModelId(const TSharedPtr<FACFAssistantSession>& Session) const
{
	// The preferred model when it is loaded, otherwise the first default of whichever provider is up.
	auto Preferred = [this]() -> FString
	{
		if (FindModel(ModelId).IsValid()) { return ModelId; }
		const TSharedPtr<FACFAssistantModel>* Default = Models.FindByPredicate([](const TSharedPtr<FACFAssistantModel>& Model) { return Model->bDefault; });
		if (Default) { return (*Default)->Id; }
		return Models.IsEmpty() ? ModelId : Models[0]->Id;
	};
	if (!Session.IsValid()) { return Preferred(); }
	if (!Session->ThreadId.IsEmpty() || Session->bBusy)
	{
		// A started conversation can only use models of the provider holding its thread.
		const TSharedPtr<FACFAssistantModel> Own = FindModel(Session->ModelId);
		if (Own.IsValid() && Own->Provider == Session->Provider) { return Session->ModelId; }
		const FString Fallback = DefaultModelFor(Session->Provider);
		return Fallback.IsEmpty() ? Session->ModelId : Fallback;
	}
	return FindModel(Session->ModelId).IsValid() ? Session->ModelId : Preferred();
}

EACFAssistantProvider FAscentACFAssistant::SessionProvider(const TSharedPtr<FACFAssistantSession>& Session) const
{
	if (Session.IsValid() && (!Session->ThreadId.IsEmpty() || Session->bBusy)) { return Session->Provider; }
	const TSharedPtr<FACFAssistantModel> Model = FindModel(SessionModelId(Session));
	if (Model.IsValid()) { return Model->Provider; }
	return Session.IsValid() ? Session->Provider : EACFAssistantProvider::Codex;
}

void FAscentACFAssistant::SetReasoningEffort(const FString& InEffort)
{
	if (ReasoningEffort == InEffort) { return; }
	ReasoningEffort = InEffort;
	SavePreferences();
	Changed.Broadcast();
}

FString FAscentACFAssistant::EffortFor(const FString& InModelId) const
{
	const TSharedPtr<FACFAssistantModel> Model = FindModel(InModelId);
	if (!Model.IsValid()) { return FString(); }
	return Model->Efforts.Contains(ReasoningEffort) ? ReasoningEffort : Model->Effort;
}

void FAscentACFAssistant::SelectModel(const FString& Id, int32 LocalId)
{
	// The model belongs to one conversation, like its work mode; the pick also becomes the default for new conversations.
	const TSharedPtr<FACFAssistantSession> Session = LocalId != 0 ? FindSession(LocalId) : GetActiveSession();
	const TSharedPtr<FACFAssistantModel> Model = FindModel(Id);
	if (!Session.IsValid() || Session->bBusy || !Model.IsValid()) { return; }
	if (!Session->ThreadId.IsEmpty() && Model->Provider != Session->Provider)
	{
		const TSharedPtr<FACFAssistantSession>* Blank = Sessions.FindByPredicate([](const TSharedPtr<FACFAssistantSession>& Candidate)
		{
			return Candidate->ThreadId.IsEmpty() && Candidate->Messages.IsEmpty() && !Candidate->bBusy
				&& Candidate->PendingImages.IsEmpty() && Candidate->PendingText.IsEmpty() && Candidate->SteerText.IsEmpty()
				&& Candidate->Approvals.IsEmpty() && !Candidate->UserInputRequestId.IsValid();
		});
		if (!Blank && !MakeRoomForConversation(Session->LocalId))
		{
			Session->Status = TEXT("All open conversations are working or have unsent content. Wait for one to finish before opening another.");
			Changed.Broadcast();
			return;
		}
		const TSharedPtr<FACFAssistantSession> Fresh = Blank ? *Blank : OpenSession();
		Fresh->ModelId = Id;
		Fresh->Provider = Model->Provider;
		Fresh->Permissions.WorkMode = Session->Permissions.WorkMode;
		Fresh->Status = FString::Printf(TEXT("New %s conversation."), ProviderName(Model->Provider));
		ActiveSessionId = Fresh->LocalId;
		ModelId = Id;
		SavePreferences();
		Changed.Broadcast();
		return;
	}
	Session->ModelId = Id;
	if (Session->ThreadId.IsEmpty()) { Session->Provider = Model->Provider; }
	ModelId = Id;
	SavePreferences();
	SaveSession(Session);
	Changed.Broadcast();
}

FString FAscentACFAssistant::Instructions() const
{
	return TEXT("You are Yarin, an in-Editor assistant for Unreal Engine. State your name only when asked; never open replies with it.\n")
		TEXT("Tool priority: general Editor tools first, then Yarin's own Ascent tools, and Python only as the last resort. ")
		TEXT("When the user asks for a 3D mesh, always ask first whether they want to start from an image. ")
		TEXT("When the request is clear, call the mutating tool directly so the Editor can show its own approval controls. ")
		TEXT("When it is ambiguous, call request_user_input with one question and 2-4 clickable options and wait; do not guess or start editing.\n")
		TEXT("Shell, web, and file edits follow the turn context. Inspect before creating or editing.\n")
		TEXT("Git is available through the shell whenever the shell is: use log, blame, show, diff and status to answer questions about history and authorship.\n")
		TEXT("When writing C++, prefer functions exposed to Blueprint over a namespace: BlueprintCallable or BlueprintPure on a UBlueprintFunctionLibrary, or an instance function on the actor or component that owns the behavior. Use a namespace or a file-local helper only for private implementation a Blueprint user should not call.\n")
		TEXT("Live Coding is available: to compile C++ changes, call AscentSystemToolset.TriggerLiveCoding (never a key press or a Python or console command). ")
		TEXT("The Editor asks the user before it runs, in every work mode; if they deny it, do not retry. Then poll AscentSystemToolset.GetLiveCodingStatus ")
		TEXT("until compiling is false and read the LogLiveCoding log entries for the result.\n")
		+ ACFAssistantTools::Instructions();
}

TSharedPtr<FJsonObject> FAscentACFAssistant::ThreadParameters(const TSharedPtr<FACFAssistantSession>& Session) const
{
	FObject Params = Object();
	// Chat threads run from the project root, as Codex does when started in a terminal there.
	Params->SetStringField(TEXT("cwd"), CodexProjectDirectory());
	Params->SetStringField(TEXT("sandbox"), TEXT("read-only"));
	Params->SetStringField(TEXT("approvalPolicy"), TEXT("never"));
	Params->SetStringField(TEXT("approvalsReviewer"), TEXT("user"));
	// The work mode is not baked in here: it can change mid-conversation, so every turn carries it as context instead.
	Params->SetStringField(TEXT("baseInstructions"), Instructions());
	const FString ThreadModel = SessionModelId(Session);
	Params->SetStringField(TEXT("model"), WireModelId(ThreadModel));
	if (SessionProvider(Session) == EACFAssistantProvider::Yarin) { ApplyYarinRoute(Params, ThreadModel); }
	// No environments / workspace-root overrides: an empty list disables environment access, which removes the shell and apply_patch.
	return Params;
}

void FAscentACFAssistant::Send(const FString& Text)
{
	const TSharedPtr<FACFAssistantSession> Session = GetActiveSession();
	if (!Session.IsValid()) { return; }
	// A typed reply while the choice buttons are up answers the question. Steering would interrupt the turn Codex is holding open for that answer.
	if (Session->UserInputRequestId.IsValid() && !Text.TrimStartAndEnd().IsEmpty())
	{
		AnswerSuggestedReply(FString(), Text);
		return;
	}
	// Never refuse silently: every early exit must tell the user why nothing was sent. A message can be images only.
	if (Text.TrimStartAndEnd().IsEmpty() && Session->PendingImages.IsEmpty()) { return; }
	if (Session->bBusy)
	{
		// Steering: show the redirection immediately, interrupt the running turn, and let the turn-completion handler dispatch it.
		// Only this conversation is interrupted; the other open ones keep working.
		HarnessLink(Session->Provider).LastActivity = FPlatformTime::Seconds();
		Session->SteerText = Text;
		Session->Messages.Add({TEXT("You"), Text});
		Session->Status = TEXT("Redirecting Yarin...");
		Cancel(Session);
		Changed.Broadcast();
		return;
	}
	const EACFAssistantProvider TurnProvider = SessionProvider(Session);
	const FString TurnModel = SessionModelId(Session);
	if (!IsProviderReady(TurnProvider))
	{
		// Say which provider, because the other one may be connected and the panel otherwise looks ready.
		const FString Why = !IsProviderAvailable(TurnProvider)
			? TEXT("Yarin gateway is coming soon. This saved conversation is read-only.")
			: Link(TurnProvider).State == EConnectionState::Ready
			? FString::Printf(TEXT("%s is reconnecting. Try again in a moment."), ProviderName(TurnProvider))
			: IsReady()
			? FString::Printf(TEXT("This conversation runs on %s, which is not signed in. Open Settings (gear icon) and sign in with %s, or pick another model to start a new conversation."),
				ProviderName(TurnProvider), ProviderName(TurnProvider))
			: FString(TEXT("You must sign in before chatting. Click the gear icon in the top right, then Sign in."));
		Session->Status = Why;
		Session->Messages.Add({TEXT("System"), Why});
		UE_LOG(LogAscentACFAssistant, Warning, TEXT("Send refused: %s is not ready (state %d)"), ProviderName(TurnProvider), static_cast<int32>(Link(TurnProvider).State));
		Changed.Broadcast();
		return;
	}
	if (TurnModel.IsEmpty())
	{
		Session->Status = TEXT("No model selected. Pick a model in the dropdown next to the message box.");
		Session->Messages.Add({TEXT("System"), TEXT("No model selected. Pick a model in the dropdown next to the message box.")});
		UE_LOG(LogAscentACFAssistant, Warning, TEXT("Send refused: empty model id with %d models loaded"), Models.Num());
		Changed.Broadcast();
		return;
	}
	if (Text.Len() > 12000) { FailSession(Session, TEXT("Please shorten the question to 12000 characters or fewer.")); return; }
	// The first message binds the conversation to its provider; before bBusy, because SessionProvider reads the pinned value from then on.
	Session->Provider = TurnProvider;
	Session->ModelId = TurnModel;
	// Stamp before bBusy. The watchdog runs on the next tick, and a new conversation does not reach StartTurn until thread/start returns.
	HarnessLink(TurnProvider).LastActivity = FPlatformTime::Seconds();
	Session->bBusy = true;
	Session->bCancelRequested = false;
	Session->bApprovalDeniedThisTurn = false;
	Session->LastAssistantItem.Reset();
	Session->LastReasoningItem.Reset();
	Session->WorkStepCount = 0;
	if (Session->HistoryEntry.IsValid())
	{
		// Most recent first across both providers: a conversation that just got a message moves to the top of Recent.
		Session->HistoryEntry->Updated = FDateTime::UtcNow().ToUnixTimestamp();
		History.Remove(Session->HistoryEntry);
		History.Insert(Session->HistoryEntry, 0);
	}
	if (Session->ThreadId.IsEmpty())
	{
		Session->PendingTitleSource = Text.TrimStartAndEnd().IsEmpty() ? TEXT("The user started with an image.") : Text.TrimStartAndEnd().Left(1000);
	}
	Session->PendingText = Text;
	if (!Text.TrimStartAndEnd().IsEmpty()) { Session->Messages.Add({TEXT("You"), Text}); }
	else if (!Session->PendingImages.IsEmpty()) { Session->Messages.Add({TEXT("You"), FString()}); }
	for (const FString& ImagePath : Session->PendingImages) { Session->Messages.Add({TEXT("Image"), ImagePath}); }
	SaveSession(Session);
	Session->Status = TEXT("Sent. Yarin is starting...");
	if (TurnProvider == EACFAssistantProvider::ClaudeCode)
	{
		if (Session->ThreadId.IsEmpty())
		{
			Session->ThreadId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens).ToLower();
			auto Entry = MakeShared<FACFAssistantHistory>();
			Entry->Id = Session->ThreadId;
			Entry->Title = TEXT("(new conversation)");
			Entry->Provider = EACFAssistantProvider::ClaudeCode;
			Entry->Updated = FDateTime::UtcNow().ToUnixTimestamp();
			History.Insert(Entry, 0);
			Session->HistoryEntry = Entry;
			SaveHistory();
		}
		StartClaudeTurn(Session);
		Changed.Broadcast();
		return;
	}
	if (Session->ThreadId.IsEmpty())
	{
		FObject Params = ThreadParameters(Session);
		Params->SetStringField(TEXT("serviceName"), TEXT("acf_assistant_client"));
		const int32 SessionId = Session->LocalId;
		Request(TEXT("thread/start"), Params, [this, SessionId](FObject Result)
		{
			const TSharedPtr<FACFAssistantSession> Started = FindSession(SessionId);
			if (!Started.IsValid()) { return; }
			Started->ThreadId = String(Child(Result, TEXT("thread")), TEXT("id"));
			if (Started->ThreadId.IsEmpty()) { FailSession(Started, TEXT("The conversation could not be started. Try again.")); return; }
			auto Entry = MakeShared<FACFAssistantHistory>();
			Entry->Id = Started->ThreadId;
			Entry->Title = TEXT("(new conversation)");
			Entry->Provider = Started->Provider;
			Entry->Updated = FDateTime::UtcNow().ToUnixTimestamp();
			History.Insert(Entry, 0);
			Started->HistoryEntry = Entry;
			SaveHistory();
			if (Started->bCancelRequested)
			{
				// Steering during thread creation: swap in the redirection instead of aborting, so the queued message still goes out.
				if (Started->SteerText.IsEmpty()) { Started->bBusy = false; Started->PendingText.Reset(); Changed.Broadcast(); return; }
				Started->PendingText = Started->SteerText;
				Started->SteerText.Reset();
				Started->bCancelRequested = false;
			}
			StartTurn(Started);
		}, true, SessionId);
	}
	else { StartTurn(Session); }
	Changed.Broadcast();
}

void FAscentACFAssistant::StartTurn(const TSharedPtr<FACFAssistantSession>& Session)
{
	if (!Session.IsValid()) { return; }
	if (Session->bNeedsThreadResume)
	{
		Session->Status = TEXT("Reconnecting conversation...");
		const FString ThreadId = Session->ThreadId;
		const int32 SessionId = Session->LocalId;
		FObject ResumeParams = ThreadParameters(Session);
		ResumeParams->SetStringField(TEXT("threadId"), ThreadId);
		ResumeParams->SetBoolField(TEXT("excludeTurns"), true);
		Request(TEXT("thread/resume"), ResumeParams, [this, ThreadId, SessionId](FObject Result)
		{
			const TSharedPtr<FACFAssistantSession> Resumed = FindSession(SessionId);
			if (!Resumed.IsValid()) { return; }
			if (String(Child(Result, TEXT("thread")), TEXT("id")) != ThreadId)
			{
				FailSession(Resumed, TEXT("The saved conversation could not be reconnected. Its transcript is still available."));
				return;
			}
			Resumed->bNeedsThreadResume = false;
			if (Resumed->bCancelRequested)
			{
				if (Resumed->SteerText.IsEmpty())
				{
					Resumed->bBusy = false;
					Resumed->PendingText.Reset();
					Changed.Broadcast();
					return;
				}
				Resumed->PendingText = MoveTemp(Resumed->SteerText);
				Resumed->SteerText.Reset();
				Resumed->bCancelRequested = false;
			}
			StartTurn(Resumed);
		}, true, SessionId);
		return;
	}
	FObject Params = Object();
	const FString TurnModel = SessionModelId(Session);
	Params->SetStringField(TEXT("threadId"), Session->ThreadId);
	Params->SetStringField(TEXT("model"), WireModelId(TurnModel));
	// The thread starts read-only; each turn carries the sandbox and approval policy of the conversation's current work mode, so switching mode mid-conversation takes effect.
	Params->SetObjectField(TEXT("sandboxPolicy"), CodexSandboxPolicy(Session));
	Params->SetStringField(TEXT("approvalPolicy"), CodexApprovalPolicy(Session));
	Params->SetStringField(TEXT("cwd"), CodexProjectDirectory());
	const FString TurnEffort = EffortFor(TurnModel);
	if (!TurnEffort.IsEmpty()) { Params->SetStringField(TEXT("effort"), TurnEffort); }
	FObject Input = Object();
	Input->SetStringField(TEXT("type"), TEXT("text"));
	Input->SetStringField(TEXT("text"), Session->PendingText);
	Input->SetArrayField(TEXT("text_elements"), {});
	FString ContextText = TurnContextDelta(Session);
	TArray<TSharedPtr<FJsonValue>> InputItems;
	if (!ContextText.IsEmpty())
	{
		FObject Context = Object();
		Context->SetStringField(TEXT("type"), TEXT("text"));
		Context->SetStringField(TEXT("text"), ContextText);
		Context->SetArrayField(TEXT("text_elements"), {});
		InputItems.Add(MakeShared<FJsonValueObject>(Context));
	}
	if (!Session->PendingText.TrimStartAndEnd().IsEmpty()) { InputItems.Add(MakeShared<FJsonValueObject>(Input)); }
	for (const FString& ImagePath : Session->PendingImages)
	{
		FString LocalPath = FPaths::ConvertRelativePathToFull(ImagePath);
		FPaths::MakePlatformFilename(LocalPath);
		FObject Image = Object();
		Image->SetStringField(TEXT("type"), TEXT("localImage"));
		Image->SetStringField(TEXT("path"), LocalPath);
		InputItems.Add(MakeShared<FJsonValueObject>(Image));
	}
	Session->PendingImages.Reset();
	Params->SetArrayField(TEXT("input"), InputItems);
	Session->PendingText.Reset();
	Codex.LastActivity = FPlatformTime::Seconds();
	const int32 SessionId = Session->LocalId;
	Request(TEXT("turn/start"), Params, [this, SessionId](FObject Result)
	{
		const TSharedPtr<FACFAssistantSession> Running = FindSession(SessionId);
		if (!Running.IsValid()) { return; }
		Running->TurnId = String(Child(Result, TEXT("turn")), TEXT("id"));
		if (Running->bCancelRequested) { Cancel(Running); }
	}, true, SessionId);
}

void FAscentACFAssistant::StartTitleGeneration(const TSharedPtr<FACFAssistantSession>& Session)
{
	if (!Session.IsValid() || Session->PendingTitleSource.IsEmpty() || !Session->TitleThreadId.IsEmpty() || !IsProviderReady(Session->Provider)) { return; }
	if (Session->Provider == EACFAssistantProvider::ClaudeCode)
	{
		Session->TitleText = Session->PendingTitleSource;
		ApplyGeneratedTitle(Session);
		return;
	}
	const int32 ThisTitleGeneration = ++Session->TitleGeneration;
	const int32 SessionId = Session->LocalId;
	const FString SourceText = Session->PendingTitleSource;
	FString AssistantText;
	for (int32 Index = Session->Messages.Num() - 1; Index >= 0; --Index)
	{
		if (Session->Messages[Index].Role == TEXT("Assistant"))
		{
			AssistantText = Session->Messages[Index].Text.Left(1000);
			break;
		}
	}
	FObject Params = Object();
	Params->SetStringField(TEXT("cwd"), WorkingDirectory);
	Params->SetStringField(TEXT("sandbox"), TEXT("read-only"));
	Params->SetStringField(TEXT("approvalPolicy"), TEXT("never"));
	Params->SetStringField(TEXT("baseInstructions"),
		TEXT("Generate a concise conversation title in the user's language. Return only 3 to 7 plain words, with no quotes, punctuation, markdown, or explanation."));
	const FString TitleModel = DefaultModelFor(Session->Provider);
	Params->SetStringField(TEXT("model"), WireModelId(TitleModel));
	if (Session->Provider == EACFAssistantProvider::Yarin) { ApplyYarinRoute(Params, TitleModel); }
	Params->SetArrayField(TEXT("runtimeWorkspaceRoots"), {});
	Request(TEXT("thread/start"), Params, [this, AssistantText, SourceText, SessionId, ThisTitleGeneration, TitleModel](FObject Result)
	{
		const TSharedPtr<FACFAssistantSession> Titled = FindSession(SessionId);
		if (!Titled.IsValid() || Titled->TitleGeneration != ThisTitleGeneration) { return; }
		Titled->TitleThreadId = String(Child(Result, TEXT("thread")), TEXT("id"));
		if (Titled->TitleThreadId.IsEmpty()) { Titled->PendingTitleSource.Reset(); return; }
		FObject TurnParams = Object();
		TurnParams->SetStringField(TEXT("threadId"), Titled->TitleThreadId);
		TurnParams->SetStringField(TEXT("model"), WireModelId(TitleModel));
		FObject Input = Object();
		Input->SetStringField(TEXT("type"), TEXT("text"));
		Input->SetStringField(TEXT("text"), TEXT("User: ") + SourceText + TEXT("\nAssistant: ") + AssistantText);
		Input->SetArrayField(TEXT("text_elements"), {});
		TurnParams->SetArrayField(TEXT("input"), {MakeShared<FJsonValueObject>(Input)});
		Titled->TitleText.Reset();
		Request(TEXT("turn/start"), TurnParams, [this, SessionId, ThisTitleGeneration](FObject TurnResult)
		{
			const TSharedPtr<FACFAssistantSession> Running = FindSession(SessionId);
			if (!Running.IsValid() || Running->TitleGeneration != ThisTitleGeneration) { return; }
			Running->TitleTurnId = String(Child(TurnResult, TEXT("turn")), TEXT("id"));
		}, false);
	}, false);
}

void FAscentACFAssistant::ApplyGeneratedTitle(const TSharedPtr<FACFAssistantSession>& Session)
{
	if (!Session.IsValid()) { return; }
	FString Title = Session->TitleText.TrimStartAndEnd();
	Title.ReplaceInline(TEXT("\""), TEXT(""));
	Title.ReplaceInline(TEXT("'"), TEXT(""));
	Title.ReplaceInline(TEXT("`"), TEXT(""));
	Title.ReplaceInline(TEXT("\r"), TEXT(" "));
	Title.ReplaceInline(TEXT("\n"), TEXT(" "));
	while (Title.Contains(TEXT("  "))) { Title.ReplaceInline(TEXT("  "), TEXT(" ")); }
	Title = Title.Left(80).TrimStartAndEnd();
	if (!Title.IsEmpty() && !Session->ThreadId.IsEmpty())
	{
		if (Session->HistoryEntry.IsValid()) { Session->HistoryEntry->Title = Title; }
		if (Session->Provider != EACFAssistantProvider::ClaudeCode)
		{
			FObject NameParams = Object();
			NameParams->SetStringField(TEXT("threadId"), Session->ThreadId);
			NameParams->SetStringField(TEXT("name"), Title);
			Request(TEXT("thread/name/set"), NameParams, TFunction<void(FObject)>(), false);
		}
		SaveHistory();
	}
	Unsubscribe(Session->TitleThreadId, false);
	Session->TitleThreadId.Reset();
	Session->TitleTurnId.Reset();
	Session->TitleText.Reset();
	Session->PendingTitleSource.Reset();
}

TSharedPtr<FAscentACFAssistant::FOneShotCompletion> FAscentACFAssistant::FindOneShotByThread(const FString& InThreadId) const
{
	if (InThreadId.IsEmpty()) { return nullptr; }
	const TSharedPtr<FOneShotCompletion>* Found = OneShotCompletions.FindByPredicate([&](const TSharedPtr<FOneShotCompletion>& Shot)
	{
		return Shot.IsValid() && Shot->ThreadId == InThreadId;
	});
	return Found ? *Found : nullptr;
}

void FAscentACFAssistant::CompleteOneShot(const TSharedPtr<FOneShotCompletion>& Shot, bool bSuccess, const FString& TextOrError)
{
	if (!Shot.IsValid()) { return; }
	const FACFAssistantCompletionResult Callback = Shot->Callback;
	StopOneShotProcess(Shot);
	if (Shot->Provider != EACFAssistantProvider::ClaudeCode && !Shot->ThreadId.IsEmpty()) { Unsubscribe(Shot->ThreadId, false); }
	OneShotCompletions.RemoveAll([&](const TSharedPtr<FOneShotCompletion>& Entry)
	{
		return Entry.IsValid() && Entry->Id == Shot->Id;
	});
	Callback.ExecuteIfBound(bSuccess, TextOrError);
}

void FAscentACFAssistant::FailOneShots(EACFAssistantProvider InProvider, const FString& Reason)
{
	// Yarin's one-shots run on the Codex process, so a Codex failure takes them down too.
	TArray<TSharedPtr<FOneShotCompletion>> Failing;
	for (int32 Index = OneShotCompletions.Num() - 1; Index >= 0; --Index)
	{
		const TSharedPtr<FOneShotCompletion>& Shot = OneShotCompletions[Index];
		if (Shot.IsValid() && (Shot->Provider == InProvider || HarnessOf(Shot->Provider) == InProvider))
		{
			Failing.Add(Shot);
			OneShotCompletions.RemoveAt(Index);
		}
	}
	for (const TSharedPtr<FOneShotCompletion>& Shot : Failing)
	{
		StopOneShotProcess(Shot);
		Shot->Callback.ExecuteIfBound(false, Reason);
	}
}

bool FAscentACFAssistant::RequestOneShotCompletion(
	const FString& SystemInstructions,
	const FString& UserMessage,
	FACFAssistantCompletionResult OnComplete)
{
	// The preferred model's provider when it is up, otherwise the first one that is.
	const TSharedPtr<FACFAssistantModel> Preferred = FindModel(ModelId);
	EACFAssistantProvider ShotProvider = Preferred.IsValid() ? Preferred->Provider : EACFAssistantProvider::Yarin;
	if (!IsProviderReady(ShotProvider))
	{
		for (const EACFAssistantProvider Each : {EACFAssistantProvider::Yarin, EACFAssistantProvider::Codex, EACFAssistantProvider::ClaudeCode})
		{
			if (IsProviderReady(Each)) { ShotProvider = Each; break; }
		}
	}
	if (!IsProviderReady(ShotProvider))
	{
		OnComplete.ExecuteIfBound(false, TEXT("Open Yarin and sign in first."));
		return false;
	}
	if (SystemInstructions.TrimStartAndEnd().IsEmpty() || UserMessage.TrimStartAndEnd().IsEmpty())
	{
		OnComplete.ExecuteIfBound(false, TEXT("System instructions and user message are required."));
		return false;
	}

	TSharedPtr<FOneShotCompletion> Shot = MakeShared<FOneShotCompletion>();
	Shot->Id = NextOneShotId++;
	Shot->Callback = OnComplete;
	Shot->Started = FPlatformTime::Seconds();
	Shot->Provider = ShotProvider;
	OneShotCompletions.Add(Shot);
	if (ShotProvider == EACFAssistantProvider::ClaudeCode)
	{
		return StartClaudeOneShot(Shot, SystemInstructions, UserMessage);
	}
	const int32 ShotId = Shot->Id;

	FObject Params = Object();
	Params->SetStringField(TEXT("cwd"), WorkingDirectory);
	Params->SetStringField(TEXT("sandbox"), TEXT("read-only"));
	Params->SetStringField(TEXT("approvalPolicy"), TEXT("never"));
	// Explicitly forbid tools: this thread must answer with text only (JSON for localization, plain text otherwise).
	Params->SetStringField(TEXT("baseInstructions"),
		SystemInstructions
		+ TEXT("\n\nYou have no tools. Do not call tools, browse, or edit files. Reply with the requested text only."));
	const FString ShotModel = DefaultModelFor(ShotProvider);
	Params->SetStringField(TEXT("model"), WireModelId(ShotModel));
	if (ShotProvider == EACFAssistantProvider::Yarin) { ApplyYarinRoute(Params, ShotModel); }
	Params->SetArrayField(TEXT("runtimeWorkspaceRoots"), {});
	Params->SetArrayField(TEXT("environments"), {});
	Params->SetArrayField(TEXT("selectedCapabilityRoots"), {});

	Request(TEXT("thread/start"), Params, [this, ShotId, UserMessage, ShotModel](FObject Result)
	{
		const TSharedPtr<FOneShotCompletion>* Found = OneShotCompletions.FindByPredicate([ShotId](const TSharedPtr<FOneShotCompletion>& Entry)
		{
			return Entry.IsValid() && Entry->Id == ShotId;
		});
		if (!Found || !Found->IsValid()) { return; }
		TSharedPtr<FOneShotCompletion> Running = *Found;
		Running->ThreadId = String(Child(Result, TEXT("thread")), TEXT("id"));
		if (Running->ThreadId.IsEmpty())
		{
			CompleteOneShot(Running, false, TEXT("The translation request could not start a Codex thread."));
			return;
		}
		FObject TurnParams = Object();
		TurnParams->SetStringField(TEXT("threadId"), Running->ThreadId);
		TurnParams->SetStringField(TEXT("model"), WireModelId(ShotModel));
		TurnParams->SetArrayField(TEXT("environments"), {});
		TurnParams->SetArrayField(TEXT("runtimeWorkspaceRoots"), {});
		FObject Input = Object();
		Input->SetStringField(TEXT("type"), TEXT("text"));
		Input->SetStringField(TEXT("text"), UserMessage);
		Input->SetArrayField(TEXT("text_elements"), {});
		TurnParams->SetArrayField(TEXT("input"), {MakeShared<FJsonValueObject>(Input)});
		Running->AccumulatedText.Reset();
		Request(TEXT("turn/start"), TurnParams, [this, ShotId](FObject TurnResult)
		{
			const TSharedPtr<FOneShotCompletion>* Still = OneShotCompletions.FindByPredicate([ShotId](const TSharedPtr<FOneShotCompletion>& Entry)
			{
				return Entry.IsValid() && Entry->Id == ShotId;
			});
			if (!Still || !Still->IsValid()) { return; }
			(*Still)->TurnId = String(Child(TurnResult, TEXT("turn")), TEXT("id"));
		}, false);
	}, false);
	return true;
}

void FAscentACFAssistant::StopTurn()
{
	const TSharedPtr<FACFAssistantSession> Session = GetActiveSession();
	if (!Session.IsValid()) { return; }
	Session->SteerText.Reset();
	Cancel(Session);
}

void FAscentACFAssistant::Cancel()
{
	Cancel(GetActiveSession());
}

void FAscentACFAssistant::Cancel(const TSharedPtr<FACFAssistantSession>& Session)
{
	if (!Session.IsValid()) { return; }
	// Stop must release the question Codex is blocked on, or the next turn waits forever for a click that will never come.
	DismissSuggestedReplies(Session);
	Session->bCancelRequested = true;
	// Stop answers every held change with a refusal so the gate and Codex are never left waiting on a click that will not come.
	DenyApprovals(Session, TEXT("No change was approved."));
	if (SessionProvider(Session) == EACFAssistantProvider::ClaudeCode)
	{
		StopClaudeProcess(Session);
		Session->TurnId.Reset();
		if (!Session->SteerText.IsEmpty() && Claude.IsReady() && !Session->ThreadId.IsEmpty())
		{
			Session->PendingText = Session->SteerText;
			Session->SteerText.Reset();
			Session->bBusy = true;
			Session->bCancelRequested = false;
			Session->bApprovalDeniedThisTurn = false;
			Session->LastAssistantItem.Reset();
			Session->LastReasoningItem.Reset();
			Session->Status = TEXT("Redirected. Working...");
			StartClaudeTurn(Session);
		}
		else
		{
			Session->bBusy = false;
			Session->Status = TEXT("Stopped. You can continue this conversation.");
		}
		Changed.Broadcast();
		return;
	}
	if (!Session->TurnId.IsEmpty())
	{
		FObject Params = Object();
		Params->SetStringField(TEXT("threadId"), Session->ThreadId);
		Params->SetStringField(TEXT("turnId"), Session->TurnId);
		Request(TEXT("turn/interrupt"), Params, TFunction<void(FObject)>(), true, Session->LocalId);
		Session->Status = TEXT("Stopping...");
	}
	else if (Session->bBusy) { Session->Status = TEXT("Stopping when the pending request is acknowledged..."); }
	Changed.Broadcast();
}

void FAscentACFAssistant::NewConversation()
{
	// A conversation already open and untouched is the new conversation; opening a second empty tab would only add clutter.
	for (const TSharedPtr<FACFAssistantSession>& Session : Sessions)
	{
		if (Session->ThreadId.IsEmpty() && Session->Messages.IsEmpty() && !Session->bBusy)
		{
			ActivateSession(Session->LocalId);
			Changed.Broadcast();
			return;
		}
	}
	if (!CanOpenAnotherConversation())
	{
		if (const TSharedPtr<FACFAssistantSession> Active = GetActiveSession())
		{
			Active->Status = FString::Printf(TEXT("%d conversations are already open. Close one to start another."), MaxOpenSessions);
		}
		Changed.Broadcast();
		return;
	}
	OpenSession();
	Changed.Broadcast();
}

FString FAscentACFAssistant::ConversationHistoryPath(EACFAssistantProvider InProvider) const
{
	const TCHAR* FileName = InProvider == EACFAssistantProvider::ClaudeCode ? TEXT("conversations_claude.json")
		: InProvider == EACFAssistantProvider::Yarin ? TEXT("conversations_yarin.json") : TEXT("conversations.json");
	return FPaths::Combine(ProfileDirectory, FileName);
}

void FAscentACFAssistant::LoadHistory()
{
	History.Reset();
	for (const EACFAssistantProvider Each : {EACFAssistantProvider::Codex, EACFAssistantProvider::ClaudeCode, EACFAssistantProvider::Yarin})
	{
		const FString Path = ConversationHistoryPath(Each);
		FString Text;
		if (IFileManager::Get().FileSize(*Path) >= 1024 * 1024 || !FFileHelper::LoadFileToString(Text, *Path)) { continue; }
		for (const auto& Value : Array(Parse(Text), TEXT("conversations")))
		{
			if (Value->Type != EJson::Object) { continue; }
			const FObject Item = Value->AsObject();
			auto Entry = MakeShared<FACFAssistantHistory>();
			Entry->Id = String(Item, TEXT("id"));
			Entry->Title = String(Item, TEXT("title"));
			Entry->Provider = Each;
			Item->TryGetNumberField(TEXT("updated"), Entry->Updated);
			if (!Entry->Id.IsEmpty()) { History.Add(Entry); }
		}
	}
	// One list, most recent first. Stable, so entries saved before timestamps existed keep their file order.
	History.StableSort([](const TSharedPtr<FACFAssistantHistory>& A, const TSharedPtr<FACFAssistantHistory>& B) { return A->Updated > B->Updated; });
}

void FAscentACFAssistant::SaveHistory()
{
	// Each provider keeps its own file, so its thread ids stay next to the store that can resume them.
	for (const EACFAssistantProvider Each : {EACFAssistantProvider::Codex, EACFAssistantProvider::ClaudeCode, EACFAssistantProvider::Yarin})
	{
		TArray<TSharedPtr<FJsonValue>> Entries;
		for (const auto& Entry : History)
		{
			if (Entry->Provider != Each) { continue; }
			FObject Item = Object();
			Item->SetStringField(TEXT("id"), Entry->Id);
			Item->SetStringField(TEXT("title"), Entry->Title);
			Item->SetNumberField(TEXT("updated"), static_cast<double>(Entry->Updated));
			Entries.Add(MakeShared<FJsonValueObject>(Item));
		}
		FObject Root = Object();
		Root->SetArrayField(TEXT("conversations"), Entries);
		Save(ConversationHistoryPath(Each), Root);
	}
}

FString FAscentACFAssistant::SessionTranscriptPath(const FString& InThreadId) const
{
	return FPaths::Combine(ProfileDirectory, FMD5::HashAnsiString(*InThreadId) + TEXT(".json"));
}

void FAscentACFAssistant::SaveSession(const TSharedPtr<FACFAssistantSession>& Session)
{
	SaveHistory();
	if (!Session.IsValid() || Session->ThreadId.IsEmpty()) { return; }
	TArray<TSharedPtr<FJsonValue>> Rows;
	for (const FACFAssistantMessage& Message : Session->Messages)
	{
		FObject Item = Object();
		Item->SetStringField(TEXT("role"), Message.Role);
		Item->SetStringField(TEXT("text"), Message.Text);
		Rows.Add(MakeShared<FJsonValueObject>(Item));
	}
	FObject Conversation = Object();
	Conversation->SetArrayField(TEXT("messages"), Rows);
	Conversation->SetBoolField(TEXT("editingApproved"), Session->Permissions.bConversationApproved);
	Conversation->SetStringField(TEXT("model"), Session->ModelId);
	Conversation->SetNumberField(TEXT("workMode"), static_cast<int32>(Session->Permissions.WorkMode));
	Save(SessionTranscriptPath(Session->ThreadId), Conversation);
}

void FAscentACFAssistant::RestoreSessionSettings(FACFAssistantSession& Session, const TSharedPtr<FJsonObject>& Conversation)
{
	if (!Conversation.IsValid()) { return; }
	Conversation->TryGetBoolField(TEXT("editingApproved"), Session.Permissions.bConversationApproved);
	FString SavedModel;
	if (Conversation->TryGetStringField(TEXT("model"), SavedModel) && !SavedModel.IsEmpty()) { Session.ModelId = SavedModel; }
	int32 SavedMode = 0;
	if (Conversation->TryGetNumberField(TEXT("workMode"), SavedMode)
		&& SavedMode >= static_cast<int32>(EACFAssistantWorkMode::Ask) && SavedMode <= static_cast<int32>(EACFAssistantWorkMode::ApproveForMe))
	{
		Session.Permissions.WorkMode = static_cast<EACFAssistantWorkMode>(SavedMode);
	}
}

void FAscentACFAssistant::Resume(const FString& Id)
{
	if (Id.IsEmpty()) { return; }
	if (const TSharedPtr<FACFAssistantSession> Open = FindSessionByThread(Id)) { ActivateSession(Open->LocalId); return; }
	const TSharedPtr<FACFAssistantHistory>* Entry = History.FindByPredicate([&](const auto& Item) { return Item->Id == Id; });
	if (!Entry) { return; }
	const TSharedPtr<FACFAssistantHistory> SavedEntry = *Entry;
	const TSharedPtr<FACFAssistantSession> Active = GetActiveSession();
	FString Text;
	const FString Path = SessionTranscriptPath(Id);
	const int64 FileSize = IFileManager::Get().FileSize(*Path);
	const FObject Conversation = FileSize >= 0 && FileSize <= 8 * 1024 * 1024 && FFileHelper::LoadFileToString(Text, *Path)
		? Parse(Text) : nullptr;
	if (!Conversation.IsValid())
	{
		if (Active.IsValid()) { Active->Status = TEXT("The saved transcript is missing or unreadable. It has not been changed."); }
		Changed.Broadcast();
		return;
	}
	TSharedPtr<FACFAssistantSession> Session = Active;
	const bool bReuseBlank = Session.IsValid() && Session->ThreadId.IsEmpty() && Session->Messages.IsEmpty()
		&& !Session->bBusy && Session->PendingImages.IsEmpty();
	if (!bReuseBlank)
	{
		if (!MakeRoomForConversation())
		{
			if (Active.IsValid()) { Active->Status = TEXT("All open conversations are working or have unsent content. Wait for one to finish before opening another."); }
			Changed.Broadcast();
			return;
		}
		Session = OpenSession();
	}
	Session->Provider = SavedEntry->Provider;
	Session->ModelId = DefaultModelFor(SavedEntry->Provider);
	Session->ThreadId = Id;
	Session->HistoryEntry = SavedEntry;
	Session->bClaudeHasStarted = SavedEntry->Provider == EACFAssistantProvider::ClaudeCode;
	Session->bNeedsThreadResume = HarnessOf(SavedEntry->Provider) == EACFAssistantProvider::Codex;
	RestoreSessionSettings(*Session, Conversation);
	for (const auto& Value : Array(Conversation, TEXT("messages")))
	{
		if (Value->Type != EJson::Object) { continue; }
		const FObject Item = Value->AsObject();
		FString SavedRole = String(Item, TEXT("role"));
		FString SavedText = String(Item, TEXT("text"));
		// A mesh job cannot outlive the Editor, so a card saved mid generation was interrupted and must not spin forever.
		if (SavedRole == ACFAssistantMesh::RolePending)
		{
			SavedRole = ACFAssistantMesh::RoleFailed;
			SavedText = TEXT("The Editor closed before the 3D generation finished.");
		}
		Session->Messages.Add({SavedRole, SavedText});
	}
	Session->Status = !IsProviderAvailable(SavedEntry->Provider)
		? TEXT("Saved conversation. Yarin gateway is coming soon; this transcript is read-only.")
		: IsProviderReady(SavedEntry->Provider) ? TEXT("Conversation loaded.")
		: FString::Printf(TEXT("Saved conversation. Sign in with %s to continue."), ProviderName(SavedEntry->Provider));
	Changed.Broadcast();
}

void FAscentACFAssistant::SetWorkMode(EACFAssistantWorkMode InMode, int32 LocalId)
{
	// Work mode belongs to one conversation: changing it must not interrupt the others.
	const TSharedPtr<FACFAssistantSession> Session = LocalId != 0 ? FindSession(LocalId) : GetActiveSession();
	if (!Session.IsValid() || Session->Permissions.WorkMode == InMode) { return; }
	Session->Permissions.WorkMode = InMode;
	// Approve all is scoped to the mode it was given in. Changing mode also refuses whatever was still waiting on the banner.
	Session->Permissions.bConversationApproved = false;
	DenyApprovals(Session, nullptr);
	if (Session->bBusy) { Cancel(Session); }
	SavePreferences();
	SaveSession(Session);
	Session->Status = TEXT("Work mode: ") + FACFAssistantPermissions::Label(InMode);
	Changed.Broadcast();
}

void FAscentACFAssistant::UpdateTokenUsage(const TSharedPtr<FACFAssistantSession>& Session, const FObject& Params)
{
	if (!Session.IsValid()) { return; }
	// tokenUsage.total is cumulative account/session traffic and can exceed the model window. The "last" object is the current thread context and is the correct numerator.
	const FObject TokenUsage = Child(Params, TEXT("tokenUsage"));
	const FObject LastUsage = Child(TokenUsage, TEXT("last"));
	const FObject Info = Child(Params, TEXT("info"));
	const FObject TotalUsage = Child(Info->Values.IsEmpty() ? Params : Info, TEXT("total_token_usage"));
	const FObject Usage = Child(Params, TEXT("usage"));
	double Total = 0;
	if (!LastUsage->TryGetNumberField(TEXT("totalTokens"), Total) && !LastUsage->TryGetNumberField(TEXT("total_tokens"), Total)
		&& !TotalUsage->TryGetNumberField(TEXT("total_tokens"), Total) && !TotalUsage->TryGetNumberField(TEXT("totalTokens"), Total)
		&& !Usage->TryGetNumberField(TEXT("totalTokens"), Total) && !Usage->TryGetNumberField(TEXT("total_tokens"), Total))
	{
		Params->TryGetNumberField(TEXT("tokenCount"), Total);
	}
	if (Total > 0) { Session->UsedContextTokens = static_cast<int64>(Total); }
	double Window = 0;
	if (!TokenUsage->TryGetNumberField(TEXT("modelContextWindow"), Window) && !TokenUsage->TryGetNumberField(TEXT("model_context_window"), Window)
		&& !Info->TryGetNumberField(TEXT("model_context_window"), Window) && !Info->TryGetNumberField(TEXT("modelContextWindow"), Window))
	{
		Params->TryGetNumberField(TEXT("modelContextWindow"), Window);
	}
	if (Window > 0) { Session->ReportedContextWindowTokens = static_cast<int64>(Window); }
}

double FAscentACFAssistant::GetContextUsageFraction() const
{
	const TSharedPtr<FACFAssistantSession> Session = GetActiveSession();
	if (!Session.IsValid()) { return 0.0; }
	int64 Window = Session->ReportedContextWindowTokens;
	if (Window <= 0)
	{
		for (const auto& Model : Models)
		{
			if (Model->Id == SessionModelId(Session)) { Window = Model->ContextWindowTokens; break; }
		}
	}
	return Window > 0 && Session->UsedContextTokens > 0
		? FMath::Clamp(static_cast<double>(Session->UsedContextTokens) / static_cast<double>(Window), 0.0, 1.0)
		: 0.0;
}

FString FAscentACFAssistant::GetContextUsageText() const
{
	const double Fraction = GetContextUsageFraction();
	const TSharedPtr<FACFAssistantSession> Session = GetActiveSession();
	if (Fraction <= 0.0 || !Session.IsValid()) { return FString(); }
	int64 Window = Session->ReportedContextWindowTokens;
	if (Window <= 0)
	{
		for (const auto& Model : Models)
		{
			if (Model->Id == SessionModelId(Session)) { Window = Model->ContextWindowTokens; break; }
		}
	}
	return FString::Printf(TEXT("Context %.0f%% (%lldk / %lldk)"), Fraction * 100.0, Session->UsedContextTokens / 1000, Window / 1000);
}

FString FAscentACFAssistant::GetActiveConversationTitle() const
{
	const TSharedPtr<FACFAssistantSession> Session = GetActiveSession();
	return Session.IsValid() ? Session->GetTitle() : TEXT("(no conversation)");
}

TSharedPtr<FJsonObject> FAscentACFAssistant::GetApprovalPreview() const
{
	const TSharedPtr<FACFAssistantSession> Session = GetActiveSession();
	if (!Session.IsValid() || Session->Approvals.IsEmpty() || !Session->Approvals[0].IsValid()) { return nullptr; }
	const TSharedPtr<FACFAssistantApproval>& Approval = Session->Approvals[0];
	FObject Preview = Approval->Preview.IsValid() ? MakeShared<FJsonObject>(*Approval->Preview) : Object();
	if (!Preview->HasField(TEXT("tool"))) { Preview->SetStringField(TEXT("tool"), Approval->Title); }
	if (!Preview->HasField(TEXT("kind"))) { Preview->SetStringField(TEXT("kind"), Approval->Kind); }
	Preview->SetNumberField(TEXT("pending"), Session->Approvals.Num());
	return Preview;
}

bool FAscentACFAssistant::HasSuggestedReplies() const
{
	const TSharedPtr<FACFAssistantSession> Session = GetActiveSession();
	if (!Session.IsValid() || !Session->UserInputRequestId.IsValid()) { return false; }
	for (const FACFAssistantQuestion& Question : Session->UserInputQuestions)
	{
		if (!Session->UserInputAnswers.Contains(Question.Id)) { return true; }
	}
	return false;
}

const TArray<FACFAssistantQuestion>& FAscentACFAssistant::GetSuggestedQuestions() const
{
	static const TArray<FACFAssistantQuestion> Empty;
	const TSharedPtr<FACFAssistantSession> Session = GetActiveSession();
	return Session.IsValid() ? Session->UserInputQuestions : Empty;
}

bool FAscentACFAssistant::IsSuggestedAnswered(const FString& QuestionId) const
{
	const TSharedPtr<FACFAssistantSession> Session = GetActiveSession();
	return Session.IsValid() && Session->UserInputAnswers.Contains(QuestionId);
}

void FAscentACFAssistant::AnswerSuggestedReply(const FString& QuestionId, const FString& Label)
{
	const TSharedPtr<FACFAssistantSession> Session = GetActiveSession();
	if (!Session.IsValid() || !Session->UserInputRequestId.IsValid()) { return; }
	const FString Choice = Label.TrimStartAndEnd();
	if (Choice.IsEmpty()) { return; }
	FString Id = QuestionId;
	const bool bKnown = Session->UserInputQuestions.ContainsByPredicate([&Id](const FACFAssistantQuestion& Question) { return Question.Id == Id; });
	if (!bKnown)
	{
		Id.Reset();
		for (const FACFAssistantQuestion& Question : Session->UserInputQuestions)
		{
			if (!Session->UserInputAnswers.Contains(Question.Id)) { Id = Question.Id; break; }
		}
	}
	if (Id.IsEmpty() || Session->UserInputAnswers.Contains(Id)) { return; }
	Session->UserInputAnswers.Add(Id, Choice);
	Session->Messages.Add({TEXT("You"), Choice});
	for (const FACFAssistantQuestion& Question : Session->UserInputQuestions)
	{
		if (!Session->UserInputAnswers.Contains(Question.Id))
		{
			Session->Status = TEXT("Choose one of the suggested answers.");
			Changed.Broadcast();
			return;
		}
	}
	SubmitSuggestedReplies(Session);
}

void FAscentACFAssistant::OfferSuggestedReplies(const TSharedPtr<FJsonObject>& Message, const TSharedPtr<FJsonObject>& Params)
{
	const TSharedPtr<FJsonValue> RequestId = Message.IsValid() ? Message->TryGetField(TEXT("id")) : nullptr;
	auto ReplyEmpty = [this, RequestId]()
	{
		if (!RequestId.IsValid()) { return; }
		FObject Result = Object();
		Result->SetObjectField(TEXT("answers"), Object());
		FObject Reply = Object();
		Reply->SetField(TEXT("id"), RequestId);
		Reply->SetObjectField(TEXT("result"), Result);
		Write(Reply);
	};
	const TSharedPtr<FACFAssistantSession> Session = FindSessionByThread(String(Params, TEXT("threadId")));
	TArray<FACFAssistantQuestion> Questions;
	for (const TSharedPtr<FJsonValue>& Value : Array(Params, TEXT("questions")))
	{
		if (!Value.IsValid() || Value->Type != EJson::Object) { continue; }
		const FObject QuestionJson = Value->AsObject();
		FACFAssistantQuestion Question;
		Question.Id = String(QuestionJson, TEXT("id"));
		Question.Header = String(QuestionJson, TEXT("header"));
		Question.Question = String(QuestionJson, TEXT("question"));
		for (const TSharedPtr<FJsonValue>& OptionValue : Array(QuestionJson, TEXT("options")))
		{
			if (!OptionValue.IsValid() || OptionValue->Type != EJson::Object) { continue; }
			FACFAssistantChoice Choice;
			Choice.Label = String(OptionValue->AsObject(), TEXT("label"));
			Choice.Description = String(OptionValue->AsObject(), TEXT("description"));
			if (!Choice.Label.IsEmpty()) { Question.Options.Add(Choice); }
		}
		if (!Question.Id.IsEmpty() && !Question.Question.IsEmpty()) { Questions.Add(Question); }
	}
	if (!Session.IsValid() || Questions.IsEmpty() || !RequestId.IsValid())
	{
		ReplyEmpty();
		return;
	}
	// A second question replaces the first. The old request still has to be released or that turn stays blocked.
	DismissSuggestedReplies(Session);
	Session->UserInputRequestId = RequestId;
	Session->UserInputQuestions = MoveTemp(Questions);
	FString Prompt;
	for (const FACFAssistantQuestion& Question : Session->UserInputQuestions)
	{
		if (!Prompt.IsEmpty()) { Prompt += TEXT("\n"); }
		Prompt += Question.Question;
	}
	Session->Messages.Add({TEXT("Assistant"), Prompt});
	Session->Status = TEXT("Choose one of the suggested answers.");
	Changed.Broadcast();
}

void FAscentACFAssistant::SubmitSuggestedReplies(const TSharedPtr<FACFAssistantSession>& Session)
{
	if (!Session.IsValid() || !Session->UserInputRequestId.IsValid()) { return; }
	FObject Answers = Object();
	for (const TPair<FString, FString>& Pair : Session->UserInputAnswers)
	{
		FObject Answer = Object();
		Answer->SetArrayField(TEXT("answers"), {MakeShared<FJsonValueString>(Pair.Value)});
		Answers->SetObjectField(Pair.Key, Answer);
	}
	FObject Result = Object();
	Result->SetObjectField(TEXT("answers"), Answers);
	FObject Reply = Object();
	Reply->SetField(TEXT("id"), Session->UserInputRequestId);
	Reply->SetObjectField(TEXT("result"), Result);
	Session->UserInputRequestId.Reset();
	Session->UserInputQuestions.Reset();
	Session->UserInputAnswers.Reset();
	Write(Reply);
	Session->Status = TEXT("Working...");
	Changed.Broadcast();
}

void FAscentACFAssistant::DismissSuggestedReplies(const TSharedPtr<FACFAssistantSession>& Session)
{
	if (!Session.IsValid() || !Session->UserInputRequestId.IsValid()) { return; }
	FObject Result = Object();
	Result->SetObjectField(TEXT("answers"), Object());
	FObject Reply = Object();
	Reply->SetField(TEXT("id"), Session->UserInputRequestId);
	Reply->SetObjectField(TEXT("result"), Result);
	Session->UserInputRequestId.Reset();
	Session->UserInputQuestions.Reset();
	Session->UserInputAnswers.Reset();
	Write(Reply);
}

void FAscentACFAssistant::ResolveApproval(bool bApproved)
{
	const TSharedPtr<FACFAssistantSession> Session = GetActiveSession();
	if (!Session.IsValid() || Session->Approvals.IsEmpty()) { return; }
	const TSharedPtr<FACFAssistantApproval> Approval = Session->Approvals[0];
	SettleApproval(Session, Approval, bApproved);
	if (!Session->Approvals.IsEmpty())
	{
		Session->Status = FString::Printf(TEXT("Waiting for your approval: %s"), *Session->Approvals[0]->Title);
	}
	else
	{
		Session->Status = bApproved ? FString::Printf(TEXT("Approved: %s"), *Approval->Title) : FString::Printf(TEXT("Denied: %s"), *Approval->Title);
	}
	Changed.Broadcast();
}

void FAscentACFAssistant::AppendChatImage(const TSharedPtr<FACFAssistantSession>& Session, const FString& Path)
{
	if (!Session.IsValid()) { return; }
	FString Candidate = Path.TrimStartAndEnd().TrimQuotes();
	if (Candidate.IsEmpty() || !IsChatImageExtension(Candidate)) { return; }
	if (FPaths::IsRelative(Candidate))
	{
		Candidate = WorkingDirectory / Candidate;
	}
	Candidate = FPaths::ConvertRelativePathToFull(Candidate);
	FPaths::NormalizeFilename(Candidate);
	FPaths::MakePlatformFilename(Candidate);
	if (!FPaths::FileExists(Candidate)) { return; }
	for (const FACFAssistantMessage& Message : Session->Messages)
	{
		if (Message.Role == TEXT("Image") && Message.Text.Equals(Candidate, ESearchCase::IgnoreCase)) { return; }
	}
	Session->Messages.Add({TEXT("Image"), Candidate});
}

int32 FAscentACFAssistant::BeginMeshCard(const TSharedPtr<FACFAssistantSession>& Session)
{
	if (!Session.IsValid()) { return INDEX_NONE; }
	const int32 Index = Session->Messages.Add({ACFAssistantMesh::RolePending, FString()});
	Session->Status = StepStatus(++Session->WorkStepCount, TEXT("Generating a 3D mesh"));
	Changed.Broadcast();
	return Index;
}

void FAscentACFAssistant::FinishMeshCard(const TSharedPtr<FACFAssistantSession>& Session, int32 Index, bool bOk, const FString& Detail)
{
	if (!Session.IsValid() || !Session->Messages.IsValidIndex(Index)) { return; }
	FACFAssistantMessage& Card = Session->Messages[Index];
	if (Card.Role != ACFAssistantMesh::RolePending) { return; }
	Card.Role = bOk ? ACFAssistantMesh::RoleReady : ACFAssistantMesh::RoleFailed;
	Card.Text = Detail;
	if (Session->bBusy)
	{
		Session->Status = StepStatus(Session->WorkStepCount, TEXT("Thinking about the next step"));
	}
	Changed.Broadcast();
}

void FAscentACFAssistant::CompleteToolCall(const FObject& Message, const FObject& ToolResult)
{
	FObject Result = ToolResult ? ToolResult : ACFAssistantTools::Error(TEXT("The tool returned no result."));
	FString Encoded = ACFAssistantTools::Encode(Result);
	FObject Content = Object();
	Content->SetStringField(TEXT("type"), TEXT("inputText"));
	Content->SetStringField(TEXT("text"), Encoded);
	FObject Output = Object();
	bool bSuccess = true;
	Result->TryGetBoolField(TEXT("success"), bSuccess);
	Output->SetBoolField(TEXT("success"), bSuccess && !Result->HasField(TEXT("error")));
	Output->SetArrayField(TEXT("contentItems"), {MakeShared<FJsonValueObject>(Content)});
	FObject Reply = Object();
	Reply->SetField(TEXT("id"), Message->TryGetField(TEXT("id")));
	Reply->SetObjectField(TEXT("result"), Output);
	Write(Reply);
}

void FAscentACFAssistant::HandleToolCall(const FObject& Message)
{
	// Yarin does not expose dynamic tools. This defensive reply prevents a stale thread created by an older build from hanging; all current tools come from ue-editor MCP.
	CompleteToolCall(Message, ACFAssistantTools::Error(TEXT("This conversation used a retired tool surface. Start a New conversation to use the direct Unreal MCP tools.")));
}

void FAscentACFAssistant::Handle(const TSharedPtr<FJsonObject>& Message)
{
	const FString Method = String(Message, TEXT("method"));
	const FObject Params = Child(Message, TEXT("params"));
	if (Method.IsEmpty())
	{
		double RawId = 0;
		if (!Message->TryGetNumberField(TEXT("id"), RawId)) { return; }
		FPending Entry;
		if (!Pending.RemoveAndCopyValue(static_cast<int32>(RawId), Entry)) { return; }
		if (Message->HasField(TEXT("error")))
		{
			// Raw protocol errors stay in the Editor log; the panel only ever shows plain-language text.
			const FString Raw = String(Child(Message, TEXT("error")), TEXT("message"));
			UE_LOG(LogAscentACFAssistant, Warning, TEXT("%s failed: %s"), *Entry.Method, *Raw);
			FString Friendly;
			if (Entry.Method == TEXT("initialize")) { Friendly = TEXT("Could not connect. Open the gear settings and press Try again."); }
			else if (Entry.Method.StartsWith(TEXT("account/"))) { Friendly = TEXT("Sign-in problem. Open the gear settings and sign in again."); }
			else if (Entry.Method == TEXT("model/list")) { Friendly = TEXT("Could not load the model list. Reconnect and try again."); }
			else if (Entry.Method.StartsWith(TEXT("thread/"))) { Friendly = TEXT("Could not start the conversation. Reconnect and try again."); }
			else if (Entry.Method.StartsWith(TEXT("turn/"))) { Friendly = TEXT("Your message could not be sent. Try again."); }
			else { Friendly = TEXT("Something went wrong. Reconnect and try again."); }
			// A conversation failing is its own problem: the shared provider process and the other open conversations stay up.
			if (const TSharedPtr<FACFAssistantSession> Session = FindSession(Entry.SessionId))
			{
				if (Session->bBusy) { Session->Messages.Add({TEXT("System"), Friendly}); }
				FailSession(Session, Friendly);
			}
			else { Fail(EACFAssistantProvider::Codex, Friendly); }
		}
		else if (Entry.Callback) { Entry.Callback(Child(Message, TEXT("result"))); }
		return;
	}
	if (Message->HasField(TEXT("id")))
	{
		FObject Reply = Object();
		Reply->SetField(TEXT("id"), Message->TryGetField(TEXT("id")));
		if (Method == TEXT("item/tool/call"))
		{
			HandleToolCall(Message);
			return;
		}
		else if (Method == TEXT("item/commandExecution/requestApproval") || Method == TEXT("item/fileChange/requestApproval")
			|| Method == TEXT("item/permissions/requestApproval"))
		{
			// Codex waits on this request. In Ask for approval it becomes an Approve / Deny on the banner and is answered from there.
			HandleCodexApprovalRequest(Message, Method, Params);
			return;
		}
		else if (Method == TEXT("item/tool/requestUserInput"))
		{
			// Codex waits on this request until we reply. Answering later, from a clicked option, is what shows the suggestion buttons.
			OfferSuggestedReplies(Message, Params);
			return;
		}
		else
		{
			FObject Error = Object();
			Error->SetNumberField(TEXT("code"), -32601);
			Error->SetStringField(TEXT("message"), TEXT("Only the tools Yarin supplies are available."));
			Reply->SetObjectField(TEXT("error"), Error);
		}
		Write(Reply);
	}
	else if (Method == TEXT("account/login/completed"))
	{
		// Only the attempt Yarin is waiting on counts. A canceled earlier attempt also reports here, as a failure.
		const FString CompletedId = String(Params, TEXT("loginId"));
		if (!LoginId.IsEmpty() && (CompletedId.IsEmpty() || CompletedId == LoginId))
		{
			LoginId.Reset();
			bool bSuccess = false;
			Params->TryGetBoolField(TEXT("success"), bSuccess);
			if (bSuccess)
			{
				Codex.State = EConnectionState::Connecting;
				Codex.Status = TEXT("Signed in. Checking the account...");
				ReadAccount();
			}
			else
			{
				const FString Error = String(Params, TEXT("error"));
				UE_LOG(LogAscentACFAssistant, Warning, TEXT("ChatGPT sign-in did not complete: %s"), *Error);
				EnterSignedOut(EACFAssistantProvider::Codex, Error.IsEmpty()
					? FString(TEXT("ChatGPT sign-in did not complete. Press Sign in with ChatGPT to try again."))
					: TEXT("ChatGPT sign-in did not complete: ") + Error.Left(300));
			}
		}
	}
	else if (Method == TEXT("account/updated"))
	{
		const FString AuthMode = String(Params, TEXT("authMode"));
		if (!IsChatGPTSubscriptionAccount(Params) && AuthMode.Equals(TEXT("apikey"), ESearchCase::IgnoreCase))
		{
			Fail(EACFAssistantProvider::Codex, TEXT("Sign in with your subscription, not an API key."));
		}
		// No auth mode means the account was signed out underneath a ready client; the next turn would fail anyway.
		else if (AuthMode.IsEmpty() && Codex.bAuthenticated && !IsAnyBusy(EACFAssistantProvider::Codex))
		{
			EnterSignedOut(EACFAssistantProvider::Codex, TEXT("Signed out of ChatGPT."));
		}
	}
	else if (Method == TEXT("thread/name/updated"))
	{
		const FString NamedThreadId = String(Params, TEXT("threadId"));
		const FString ThreadName = String(Params, TEXT("threadName"));
		if (!ThreadName.IsEmpty())
		{
			for (const auto& Entry : History)
			{
				if (Entry->Id == NamedThreadId) { Entry->Title = ThreadName; break; }
			}
			SaveHistory();
		}
	}
	// Every notification below carries the thread it belongs to, which is how one shared provider process feeds several conversations at once.
	else if (const TSharedPtr<FOneShotCompletion> OneShot = FindOneShotByThread(String(Params, TEXT("threadId"))))
	{
		if (Method == TEXT("item/agentMessage/delta"))
		{
			OneShot->AccumulatedText += String(Params, TEXT("delta"));
		}
		else if (Method == TEXT("turn/completed"))
		{
			const FString Text = OneShot->AccumulatedText.TrimStartAndEnd();
			if (Text.IsEmpty())
			{
				CompleteOneShot(OneShot, false, FString::Printf(TEXT("%s returned an empty reply."), ProviderName(OneShot->Provider)));
			}
			else
			{
				CompleteOneShot(OneShot, true, Text);
			}
		}
		else if (Method == TEXT("turn/failed") || Method == TEXT("error"))
		{
			FString Error = String(Params, TEXT("error"));
			if (Error.IsEmpty()) { Error = String(Child(Params, TEXT("error")), TEXT("message")); }
			if (Error.IsEmpty()) { Error = TEXT("The translation turn failed."); }
			CompleteOneShot(OneShot, false, Error);
		}
	}
	else if (const TSharedPtr<FACFAssistantSession> TitleSession = FindSessionByTitleThread(String(Params, TEXT("threadId"))))
	{
		if (Method == TEXT("item/agentMessage/delta")) { TitleSession->TitleText += String(Params, TEXT("delta")); }
		else if (Method == TEXT("turn/completed")) { ApplyGeneratedTitle(TitleSession); }
	}
	else if (const TSharedPtr<FACFAssistantSession> Session = FindSessionByThread(String(Params, TEXT("threadId"))))
	{
		HandleSessionEvent(Session, Method, Params);
	}
	else if (Method.Contains(TEXT("tokenCount")) || Method.Contains(TEXT("tokenUsage")))
	{
		// Some token events carry no thread id. They describe the turn the user is watching.
		UpdateTokenUsage(GetActiveSession(), Params);
	}
	else if (Method == TEXT("error"))
	{
		UE_LOG(LogAscentACFAssistant, Warning, TEXT("Codex error notification: %s"), *String(Child(Params, TEXT("error")), TEXT("message")));
		Codex.Status = TEXT("The AI service reported a problem. Try again or reconnect.");
	}
	Changed.Broadcast();
}

// One conversation's stream. Deltas, work items, and turn completion all arrive tagged with the thread they belong to, which is how several
// conversations share a single provider process without interfering with each other.
void FAscentACFAssistant::HandleSessionEvent(const TSharedPtr<FACFAssistantSession>& Session, const FString& Method, const TSharedPtr<FJsonObject>& Params)
{
	if (Method == TEXT("item/agentMessage/delta"))
	{
		const FString ItemId = String(Params, TEXT("itemId"));
		if (Session->Messages.IsEmpty() || Session->Messages.Last().Role != TEXT("Assistant") || Session->LastAssistantItem != ItemId)
		{
			Session->Messages.Add({TEXT("Assistant"), FString()});
			Session->LastAssistantItem = ItemId;
		}
		Session->Messages.Last().Text += String(Params, TEXT("delta"));
		Session->Status = TEXT("Answering...");
	}
	else if (Method.StartsWith(TEXT("item/reasoning")))
	{
		// Streams the model's working notes into the chat, matching what a terminal Codex session would print while it thinks.
		FString Delta = String(Params, TEXT("delta"));
		if (Delta.IsEmpty()) { Delta = String(Params, TEXT("text")); }
		if (!Delta.IsEmpty())
		{
			const FString ItemId = String(Params, TEXT("itemId"));
			if (Session->Messages.IsEmpty() || Session->Messages.Last().Role != TEXT("Reasoning") || Session->LastReasoningItem != ItemId)
			{
				Session->Messages.Add({TEXT("Reasoning"), FString()});
				Session->LastReasoningItem = ItemId;
			}
			Session->Messages.Last().Text += Delta;
			Session->Status = TEXT("Thinking...");
		}
	}
	else if (Method == TEXT("turn/started"))
	{
		Session->TurnId = String(Child(Params, TEXT("turn")), TEXT("id"));
		if (Session->WorkStepCount == 0) { Session->Status = TEXT("Thinking..."); }
	}
	else if (Method.Contains(TEXT("tokenCount")) || Method.Contains(TEXT("tokenUsage")))
	{
		UpdateTokenUsage(Session, Params);
	}
	else if (Method == TEXT("item/started") || Method == TEXT("item/completed") || Method == TEXT("item/updated"))
	{
		// Mirrors everything a terminal Codex session would print: commands, their output, web searches, and full texts that never streamed as deltas.
		const FObject Item = Child(Params, TEXT("item"));
		const FString Type = String(Item, TEXT("type"));
		const FString ItemId = String(Item, TEXT("id"));
		const bool bStarted = Method == TEXT("item/started");
		if (Type == TEXT("commandExecution"))
		{
			if (bStarted)
			{
				Session->Messages.Add({TEXT("Tool"), TEXT("command: ") + String(Item, TEXT("command"))});
				Session->Status = StepStatus(++Session->WorkStepCount, TEXT("Running a command"));
			}
			else if (Method == TEXT("item/completed"))
			{
				const FString Output = String(Item, TEXT("aggregatedOutput"));
				if (!Output.TrimStartAndEnd().IsEmpty()) { Session->Messages.Add({TEXT("ToolOutput"), Output.Left(4000)}); }
				TArray<FString> ImagePaths;
				CollectImagePathsFromValue(MakeShared<FJsonValueString>(Output), ImagePaths, 0);
				for (const FString& ImagePath : ImagePaths) { AppendChatImage(Session, ImagePath); }
			}
		}
		else if (Type == TEXT("webSearch"))
		{
			if (bStarted)
			{
				Session->Messages.Add({TEXT("Tool"), TEXT("web search: ") + String(Item, TEXT("query"))});
				Session->Status = StepStatus(++Session->WorkStepCount, TEXT("Searching the web"));
			}
		}
		else if (Type.Equals(TEXT("imageGeneration"), ESearchCase::IgnoreCase) || Type.Equals(TEXT("imageView"), ESearchCase::IgnoreCase))
		{
			if (bStarted)
			{
				Session->Messages.Add({TEXT("Tool"), TEXT("Generating image...")});
				Session->Status = StepStatus(++Session->WorkStepCount, TEXT("Generating image"));
			}
			if (Method == TEXT("item/completed") || Method == TEXT("item/updated"))
			{
				TArray<FString> ImagePaths;
				CollectImagePathsFromToolItem(Item, ImagePaths);
				for (const FString& ImagePath : ImagePaths) { AppendChatImage(Session, ImagePath); }
			}
		}
		else if (Type.Contains(TEXT("ToolCall"), ESearchCase::IgnoreCase))
		{
			// MCP tool calls (the standard Unreal tools) run directly between the AI runtime and the Editor MCP server; mirror them in the chat work lines.
			if (bStarted)
			{
				const FString ToolLabel = ToolActivityLabel(Item);
				if (!ToolLabel.IsEmpty())
				{
					Session->Messages.Add({TEXT("Tool"), ToolLabel});
					Session->Status = StepStatus(++Session->WorkStepCount, ToolLabel);
				}
			}
			if (Method == TEXT("item/completed") || Method == TEXT("item/updated"))
			{
				TArray<FString> ImagePaths;
				CollectImagePathsFromToolItem(Item, ImagePaths);
				for (const FString& ImagePath : ImagePaths) { AppendChatImage(Session, ImagePath); }
			}
			// The model usually thinks again before the next step, so say so instead of leaving the finished tool on screen.
			if (Method == TEXT("item/completed") && Session->bBusy)
			{
				Session->Status = StepStatus(Session->WorkStepCount, TEXT("Thinking about the next step"));
			}
		}
		else if (Type == TEXT("fileChange"))
		{
			if (bStarted) { Session->Messages.Add({TEXT("Tool"), TEXT("file change proposed")}); }
		}
		else if (Type == TEXT("error"))
		{
			const FString Text = String(Item, TEXT("message"));
			if (!Text.IsEmpty()) { Session->Messages.Add({TEXT("System"), Text}); }
		}
		else if (Type == TEXT("agentMessage") && Method == TEXT("item/completed"))
		{
			// Some models deliver the whole answer only at completion, without deltas.
			const FString Text = String(Item, TEXT("text"));
			if (!Text.IsEmpty() && Session->LastAssistantItem != ItemId)
			{
				Session->Messages.Add({TEXT("Assistant"), Text});
				Session->LastAssistantItem = ItemId;
			}
		}
		else if (Type == TEXT("reasoning") && Method == TEXT("item/completed"))
		{
			FString Text = String(Item, TEXT("text"));
			if (Text.IsEmpty())
			{
				for (const auto& Piece : Array(Item, TEXT("summary")))
				{
					if (Piece->Type == EJson::String) { Text += Piece->AsString() + TEXT("\n"); }
					else if (Piece->Type == EJson::Object) { Text += String(Piece->AsObject(), TEXT("text")) + TEXT("\n"); }
				}
			}
			Text.TrimStartAndEndInline();
			if (!Text.IsEmpty() && Session->LastReasoningItem != ItemId)
			{
				Session->Messages.Add({TEXT("Reasoning"), Text});
				Session->LastReasoningItem = ItemId;
			}
		}
	}
	else if (Method == TEXT("turn/completed"))
	{
		DenyApprovals(Session, nullptr);
		// The turn is over, so a click can no longer be delivered. Drop the buttons without a late reply.
		Session->UserInputRequestId.Reset();
		Session->UserInputQuestions.Reset();
		Session->UserInputAnswers.Reset();
		const FObject Turn = Child(Params, TEXT("turn"));
		UpdateTokenUsage(Session, Turn);
		if (String(Turn, TEXT("status")) == TEXT("failed"))
		{
			const FString Raw = String(Child(Turn, TEXT("error")), TEXT("message"));
			UE_LOG(LogAscentACFAssistant, Warning, TEXT("Turn failed: %s"), *Raw);
			Session->Messages.Add({TEXT("System"), TEXT("Yarin could not complete this response. Try again; if it repeats, reconnect from Settings.")});
			Session->Status = TEXT("The answer failed. Try again.");
		}
		else { Session->Status = Session->bCancelRequested ? TEXT("Stopped. You can continue this conversation.") : TEXT("Ready."); }
		Session->bBusy = false;
		Session->TurnId.Reset();
		SaveSession(Session);
		if (!Session->PendingTitleSource.IsEmpty()) { StartTitleGeneration(Session); }
		// Steering: the interrupted turn is done, dispatch the queued redirection on the same thread. It goes out on the conversation's OWN provider:
		// a Yarin conversation must not wait for a ChatGPT sign-in it never uses.
		if (!Session->SteerText.IsEmpty())
		{
			if (IsProviderReady(Session->Provider) && !Session->ThreadId.IsEmpty())
			{
				Session->PendingText = Session->SteerText;
				Session->SteerText.Reset();
				Session->bBusy = true;
				Session->bCancelRequested = false;
				Session->bApprovalDeniedThisTurn = false;
				Session->LastAssistantItem.Reset();
				Session->LastReasoningItem.Reset();
				Session->WorkStepCount = 0;
				Session->Status = TEXT("Redirected. Working...");
				StartTurn(Session);
			}
			else
			{
				// Never left behind to fire on some later turn.
				Session->SteerText.Reset();
				Session->Status = FString::Printf(TEXT("Stopped. %s is not signed in, so the redirection was not sent."), ProviderName(Session->Provider));
			}
		}
	}
}
