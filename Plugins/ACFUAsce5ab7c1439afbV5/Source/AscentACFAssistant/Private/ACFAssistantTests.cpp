// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "ACFAssistantClientInternal.h"
#include "ACFAssistantMesh.h"
#include "ACFAssistantOwnPlugin.h"
#include "ACFAssistantMarkdown.h"
#include "ACFAssistantToolGate.h"
#include "ACFAssistantToolPolicy.h"
#include "ACFAssistantTools.h"
#include "AscentEpicMCP.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "HAL/FileManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/App.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "ToolsetRegistry/UToolsetRegistry.h"
#include "Widgets/Text/SMultiLineEditableText.h"
#include "Widgets/Layout/SGridPanel.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAscentACFAssistantAuthTest, "Ascent.ACFAssistant.AuthGate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAscentACFAssistantAuthTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("Codex 0.153.4 is supported"), FAscentACFAssistant::IsSupportedCodexVersion(TEXT("codex-cli 0.153.4")));
	TestTrue(TEXT("Newer Codex is supported"), FAscentACFAssistant::IsSupportedCodexVersion(TEXT("codex-cli 0.154.0")));
	TestFalse(TEXT("Older Codex is rejected"), FAscentACFAssistant::IsSupportedCodexVersion(TEXT("codex-cli 0.153.3")));

	TSharedPtr<FJsonObject> Plus = MakeShared<FJsonObject>();
	Plus->SetStringField(TEXT("type"), TEXT("chatgpt"));
	Plus->SetStringField(TEXT("authMode"), TEXT("chatgpt"));
	Plus->SetStringField(TEXT("planType"), TEXT("plus"));
	TestTrue(TEXT("ChatGPT Plus subscription is accepted"), FAscentACFAssistant::IsChatGPTSubscriptionAccount(Plus));

	TSharedPtr<FJsonObject> Pro = MakeShared<FJsonObject>();
	Pro->SetStringField(TEXT("type"), TEXT("chatgpt"));
	Pro->SetStringField(TEXT("authMode"), TEXT("chatgpt"));
	Pro->SetStringField(TEXT("planType"), TEXT("pro"));
	TestTrue(TEXT("ChatGPT Pro subscription is accepted"), FAscentACFAssistant::IsChatGPTSubscriptionAccount(Pro));

	TSharedPtr<FJsonObject> ApiKey = MakeShared<FJsonObject>();
	ApiKey->SetStringField(TEXT("type"), TEXT("api"));
	ApiKey->SetStringField(TEXT("authMode"), TEXT("apikey"));
	TestFalse(TEXT("API-key auth is rejected"), FAscentACFAssistant::IsChatGPTSubscriptionAccount(ApiKey));

	using EClaudeAuth = FAscentACFAssistant::EClaudeAuth;
	const FString ClaudeMax = TEXT("{\"loggedIn\":true,\"authMethod\":\"claude.ai\",\"subscriptionType\":\"max\"}");
	TestTrue(TEXT("Claude.ai Max subscription is accepted"), FAscentACFAssistant::ClassifyClaudeAuth(ClaudeMax) == EClaudeAuth::Subscription);
	TestTrue(TEXT("A setup-token subscription credential is accepted"), FAscentACFAssistant::ClassifyClaudeAuth(
		TEXT("{\"loggedIn\":true,\"authMethod\":\"oauth_token\",\"apiProvider\":\"firstParty\"}")) == EClaudeAuth::Subscription);
	TestTrue(TEXT("Logged-out Claude status offers sign-in"),
		FAscentACFAssistant::ClassifyClaudeAuth(TEXT("{\"loggedIn\":false,\"authMethod\":\"none\"}")) == EClaudeAuth::SignedOut);
	TestTrue(TEXT("A Console login is accepted as API billing"),
		FAscentACFAssistant::ClassifyClaudeAuth(TEXT("{\"loggedIn\":true,\"authMethod\":\"console\"}")) == EClaudeAuth::ApiBilling);
	// Both shapes below were captured from Claude Code 2.1.280.
	const FString ClaudeBedrock = TEXT("{\"loggedIn\":true,\"authMethod\":\"third_party\",\"apiProvider\":\"bedrock\"}");
	const FString ClaudeApiKey = TEXT("{\"loggedIn\":true,\"authMethod\":\"claude.ai\",\"apiProvider\":\"firstParty\",\"apiKeySource\":\"ANTHROPIC_API_KEY\",\"email\":null}");
	TestTrue(TEXT("A Bedrock switch is accepted as API billing"), FAscentACFAssistant::ClassifyClaudeAuth(ClaudeBedrock) == EClaudeAuth::ApiBilling);
	TestTrue(TEXT("An API key beside a claude.ai login is accepted as API billing"), FAscentACFAssistant::ClassifyClaudeAuth(ClaudeApiKey) == EClaudeAuth::ApiBilling);
	TestEqual(TEXT("A cloud provider is named as the payer"), FAscentACFAssistant::ClaudeAccountLabel(ClaudeBedrock), FString(TEXT("bedrock")));
	TestEqual(TEXT("An API key is named as the payer"), FAscentACFAssistant::ClaudeAccountLabel(ClaudeApiKey), FString(TEXT("API key from ANTHROPIC_API_KEY")));
	TestTrue(TEXT("Substring guessing is not used for Claude auth"),
		FAscentACFAssistant::ClassifyClaudeAuth(TEXT("Not logged in. Buy a Claude subscription.")) == EClaudeAuth::Unreadable);
	TestTrue(TEXT("An expired OAuth sign-in is recognised"),
		FAscentACFAssistant::IsClaudeAuthFailure(TEXT("OAuth token has expired. Please obtain a new token or refresh your existing token.")));
	TestFalse(TEXT("An ordinary tool error is not a sign-in failure"),
		FAscentACFAssistant::IsClaudeAuthFailure(TEXT("ue-editor tool call failed: asset not found")));
	TestFalse(TEXT("The user's API key reaches a claude child"), FAscentACFAssistant::IsClaudeNestingEnvironmentEntry(TEXT("ANTHROPIC_API_KEY=sk-x")));
	TestFalse(TEXT("The user's endpoint override reaches a claude child"), FAscentACFAssistant::IsClaudeNestingEnvironmentEntry(TEXT("anthropic_base_url=https://proxy")));
	TestFalse(TEXT("The user's cloud-provider switch reaches a claude child"), FAscentACFAssistant::IsClaudeNestingEnvironmentEntry(TEXT("CLAUDE_CODE_USE_BEDROCK=1")));
	TestTrue(TEXT("The nested-session marker never reaches a claude child"), FAscentACFAssistant::IsClaudeNestingEnvironmentEntry(TEXT("CLAUDECODE=1")));
	TestTrue(TEXT("The nested-session entrypoint never reaches a claude child"), FAscentACFAssistant::IsClaudeNestingEnvironmentEntry(TEXT("CLAUDE_CODE_ENTRYPOINT=cli")));
	TestFalse(TEXT("The machine's subscription token passes"), FAscentACFAssistant::IsClaudeNestingEnvironmentEntry(TEXT("CLAUDE_CODE_OAUTH_TOKEN=abc")));
	TestFalse(TEXT("The machine's Claude config location passes"), FAscentACFAssistant::IsClaudeNestingEnvironmentEntry(TEXT("CLAUDE_CONFIG_DIR=D:\\claude")));
	TestFalse(TEXT("Per-drive working directories pass"), FAscentACFAssistant::IsClaudeNestingEnvironmentEntry(TEXT("=C:=C:\\Work")));
	TestTrue(TEXT("Current Claude OAuth host is accepted"),
		FAscentACFAssistant::IsClaudeLoginUrl(TEXT("https://claude.com/cai/oauth/authorize?code=true")));
	TestTrue(TEXT("Claude.ai OAuth host is still accepted"),
		FAscentACFAssistant::IsClaudeLoginUrl(TEXT("https://claude.ai/oauth/authorize")));
	TestEqual(TEXT("Login URL is extracted from Claude CLI output"),
		FAscentACFAssistant::ExtractClaudeLoginUrl(TEXT("Opening browser...\nIf the browser didn't open, visit: https://claude.com/cai/oauth/authorize?code=true&client_id=abc\nPaste code here")),
		FString(TEXT("https://claude.com/cai/oauth/authorize?code=true&client_id=abc")));
	TestTrue(TEXT("Unrelated https URLs are ignored"),
		FAscentACFAssistant::ExtractClaudeLoginUrl(TEXT("see https://example.com/login then https://claude.com/cai/oauth/authorize")).StartsWith(TEXT("https://claude.com/")));
	TestEqual(TEXT("Plan keeps Claude out of plan mode so Editor reads work"),
		FString(FAscentACFAssistant::ClaudePermissionModeName(EACFAssistantWorkMode::Plan)), FString(TEXT("default")));
	TestEqual(TEXT("Approve-for-me maps to Claude bypass"),
		FString(FAscentACFAssistant::ClaudePermissionModeName(EACFAssistantWorkMode::ApproveForMe)), FString(TEXT("bypassPermissions")));
	TestEqual(TEXT("A plan with no email still names the subscription"),
		FAscentACFAssistant::ClaudeAccountLabel(ClaudeMax), FString(TEXT("max")));
	TestEqual(TEXT("Email and plan are combined"),
		FAscentACFAssistant::ClaudeAccountLabel(TEXT("{\"loggedIn\":true,\"authMethod\":\"claude.ai\",\"email\":\"a@b.c\",\"subscriptionType\":\"max\"}")),
		FString(TEXT("a@b.c (max)")));

	FACFAssistantSession StreamSession;
	auto Line = [](const FString& Json)
	{
		TSharedPtr<FJsonObject> Object;
		FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Object);
		return Object;
	};
	FString StreamError;
	TestFalse(TEXT("Text deltas do not finish the turn"), FAscentACFAssistant::HandleClaudeStreamLine(StreamSession,
		Line(TEXT("{\"type\":\"stream_event\",\"event\":{\"type\":\"content_block_delta\",\"delta\":{\"type\":\"text_delta\",\"text\":\"Hello\"}}}")), StreamError));
	TestTrue(TEXT("Streamed assistant text is visible"), StreamSession.Messages.ContainsByPredicate([](const FACFAssistantMessage& Message)
	{
		return Message.Role == TEXT("Assistant") && Message.Text == TEXT("Hello");
	}));
	TestFalse(TEXT("Tool start does not finish the turn"), FAscentACFAssistant::HandleClaudeStreamLine(StreamSession,
		Line(TEXT("{\"type\":\"stream_event\",\"event\":{\"type\":\"content_block_start\",\"content_block\":{\"type\":\"tool_use\",\"name\":\"mcp__ue-editor__find_actors\"}}}")), StreamError));
	// The arguments are still streaming at block start, so the real tool is not known: only the status line moves.
	TestFalse(TEXT("Tool start adds no Tool message before the tool is named"), StreamSession.Messages.ContainsByPredicate([](const FACFAssistantMessage& Message)
	{
		return Message.Role == TEXT("Tool");
	}));
	TestTrue(TEXT("Tool start announces the step while the arguments stream"), StreamSession.Status.EndsWith(TEXT("Preparing a tool call")));
	TestFalse(TEXT("A completed tool call does not finish the turn"), FAscentACFAssistant::HandleClaudeStreamLine(StreamSession,
		Line(TEXT("{\"type\":\"assistant\",\"message\":{\"id\":\"msg_1\",\"content\":[{\"type\":\"tool_use\",\"name\":\"mcp__ue-editor__find_actors\",\"input\":{}}]}}")), StreamError));
	TestTrue(TEXT("A completed tool call raises no error"), StreamError.IsEmpty());
	TestTrue(TEXT("MCP tool names are shown"), StreamSession.Messages.ContainsByPredicate([](const FACFAssistantMessage& Message)
	{
		return Message.Role == TEXT("Tool") && Message.Text.Contains(TEXT("find_actors"));
	}));
	TestTrue(TEXT("A successful result finishes the turn"), FAscentACFAssistant::HandleClaudeStreamLine(StreamSession,
		Line(TEXT("{\"type\":\"result\",\"subtype\":\"success\",\"is_error\":false,\"result\":\"Hello\",\"usage\":{\"input_tokens\":10,\"output_tokens\":5},")
		TEXT("\"modelUsage\":{\"claude-sonnet-5-5\":{\"inputTokens\":10,\"outputTokens\":5,\"cacheReadInputTokens\":0,\"cacheCreationInputTokens\":0,\"contextWindow\":1000000}}}")), StreamError));
	TestTrue(TEXT("Successful result has no error text"), StreamError.IsEmpty());
	TestEqual(TEXT("A result's per-turn usage and modelUsage never become the context size"), StreamSession.UsedContextTokens, (int64)0);

	// The Editor tools arrive through a relay, so the panel must name the tool that ran, not the relay "call_tool".
	const TSharedPtr<FJsonObject> Relayed = Line(TEXT("{\"type\":\"mcpToolCall\",\"server\":\"ue-editor\",\"tool\":\"call_tool\",")
		TEXT("\"arguments\":{\"toolset_name\":\"editor_toolset.toolsets.asset.AssetTools\",\"tool_name\":\"find_assets\",")
		TEXT("\"arguments\":{\"folder_path\":\"\",\"name\":\"EffectsManager\"}}}"));
	TestEqual(TEXT("A relayed Editor tool names the real tool and its subject"),
		FAscentACFAssistant::ToolActivityLabel(Relayed), FString(TEXT("find_assets  EffectsManager")));
	TestEqual(TEXT("An MCP prefix is dropped but underscores in the tool name are kept"),
		FAscentACFAssistant::ToolActivityLabel(Line(TEXT("{\"name\":\"mcp__ue-editor__find_assets\",\"input\":{\"name\":\"DA_Goblin\"}}"))),
		FString(TEXT("find_assets  DA_Goblin")));
	TestEqual(TEXT("An Ascent dynamic name keeps only the tool"),
		FAscentACFAssistant::ToolActivityLabel(Line(TEXT("{\"tool\":\"AscentAIToolsEditor_ACFDataAssetToolset_DescribeClass\"}"))),
		FString(TEXT("DescribeClass")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAscentACFAssistantPlanModeTest, "Ascent.ACFAssistant.PlanModeTools",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAscentACFAssistantPlanModeTest::RunTest(const FString& Parameters)
{
	// SetWorkMode persists the user's preference, so the test pins its own starting value and puts the real one back.
	UACFAssistantSettings& Settings = UACFAssistantSettings::Get();
	const EACFAssistantWorkMode SavedWorkMode = Settings.WorkMode;
	Settings.WorkMode = EACFAssistantWorkMode::Plan;
	{
		FAscentACFAssistant Client;
		TestTrue(TEXT("Work mode starts as Plan"), Client.GetWorkMode() == EACFAssistantWorkMode::Plan);
		TestTrue(TEXT("Plan blocks mutations"), Client.IsPlanMode());
		auto Inspect = MakeShared<FJsonObject>();
		Inspect->SetStringField(TEXT("toolset_name"), TEXT("AscentAIAgent_AscentDataToolset"));
		Inspect->SetStringField(TEXT("tool_name"), TEXT("DataAssetInspect"));
		auto Create = MakeShared<FJsonObject>();
		Create->SetStringField(TEXT("toolset_name"), TEXT("AscentAIToolsEditor_ACFItemToolset"));
		Create->SetStringField(TEXT("tool_name"), TEXT("SaveItem"));
		TestTrue(TEXT("Plan lets a read-only Editor tool through"), Client.JudgeEditorTool(TEXT("call_tool"), Inspect).Verdict == EACFAssistantToolVerdict::Allow);
		TestTrue(TEXT("Plan blocks a mutating Editor tool at the gate"), Client.JudgeEditorTool(TEXT("call_tool"), Create).Verdict == EACFAssistantToolVerdict::Block);

		Client.SetWorkMode(EACFAssistantWorkMode::AskForApproval);
		TestFalse(TEXT("Ask for approval allows mutations after approval"), Client.IsPlanMode());
		TestFalse(TEXT("Ask for approval is not auto-approve"), Client.IsApproveForMe());
		TestTrue(TEXT("Ask for approval holds a mutating Editor tool"), Client.JudgeEditorTool(TEXT("call_tool"), Create).Verdict == EACFAssistantToolVerdict::Ask);
		TestTrue(TEXT("Ask for approval keeps Codex read-only until approved"), Client.CodexSandboxPolicy(Client.GetActiveSession())->GetStringField(TEXT("type")) == TEXT("readOnly"));
		TestEqual(TEXT("Ask for approval turns on Codex approval requests"), FString(Client.CodexApprovalPolicy(Client.GetActiveSession())), FString(TEXT("on-request")));
		TestEqual(TEXT("Ask for approval keeps Claude prompting"), FString(FAscentACFAssistant::ClaudePermissionModeName(EACFAssistantWorkMode::AskForApproval)), FString(TEXT("default")));

		// A held change is answered exactly once, with the button that was pressed.
		int32 Decisions = 0;
		bool bLastDecision = false;
		Client.RequestApproval(Client.GetActiveSession(), TEXT("editor-tool"), TEXT("CreateItem"), nullptr, [&](bool bApproved) { ++Decisions; bLastDecision = bApproved; });
		TestTrue(TEXT("The banner shows the held change"), Client.IsWaitingForApproval());
		Client.ResolveApproval(true);
		TestEqual(TEXT("Approve answers once"), Decisions, 1);
		TestTrue(TEXT("Approve answers yes"), bLastDecision);
		TestFalse(TEXT("The banner clears after the decision"), Client.IsWaitingForApproval());
		Client.RequestApproval(Client.GetActiveSession(), TEXT("editor-tool"), TEXT("CreateItem"), nullptr, [&](bool bApproved) { ++Decisions; bLastDecision = bApproved; });
		Client.ResolveApproval(false);
		TestEqual(TEXT("Deny answers once"), Decisions, 2);
		TestFalse(TEXT("Deny answers no"), bLastDecision);

		// Approve all runs what is waiting and stops the conversation from asking, until the work mode changes.
		Client.RequestApproval(Client.GetActiveSession(), TEXT("editor-tool"), TEXT("CreateItem"), nullptr, [&](bool bApproved) { ++Decisions; bLastDecision = bApproved; });
		Client.ApproveConversation();
		TestEqual(TEXT("Approve all answers the held change"), Decisions, 3);
		TestTrue(TEXT("Approve all answers yes"), bLastDecision);
		TestTrue(TEXT("Approved conversations no longer ask"), Client.JudgeEditorTool(TEXT("call_tool"), Create).Verdict == EACFAssistantToolVerdict::Allow);
		TestTrue(TEXT("Approved conversations lift the Codex sandbox"), Client.CodexSandboxPolicy(Client.GetActiveSession())->GetStringField(TEXT("type")) == TEXT("dangerFullAccess"));
		Client.SetWorkMode(EACFAssistantWorkMode::Ask);
		Client.SetWorkMode(EACFAssistantWorkMode::AskForApproval);
		TestTrue(TEXT("Changing the work mode forgets Approve all"), Client.JudgeEditorTool(TEXT("call_tool"), Create).Verdict == EACFAssistantToolVerdict::Ask);

		// Approve all must not silently run a second, unseen Live Coding request.
		int32 OrdinaryDecisions = 0;
		int32 MandatoryDecisions = 0;
		bool bMandatoryApproved = false;
		Client.RequestApproval(Client.GetActiveSession(), TEXT("editor-tool"), TEXT("SaveItem"), nullptr,
			[&](bool bApproved) { if (bApproved) { ++OrdinaryDecisions; } });
		Client.RequestApproval(Client.GetActiveSession(), TEXT("editor-tool"), TEXT("LiveCoding.Compile"), nullptr,
			[&](bool bApproved) { ++MandatoryDecisions; bMandatoryApproved = bApproved; }, true);
		Client.ApproveConversation();
		TestEqual(TEXT("Approve all runs the visible ordinary change"), OrdinaryDecisions, 1);
		TestEqual(TEXT("Approve all does not run hidden Live Coding"), MandatoryDecisions, 0);
		TestTrue(TEXT("Hidden Live Coding remains on the approval banner"), Client.IsWaitingForApproval());
		Client.ResolveApproval(true);
		TestEqual(TEXT("Individual approval runs Live Coding exactly once"), MandatoryDecisions, 1);
		TestTrue(TEXT("Individual approval accepts Live Coding"), bMandatoryApproved);
		TestFalse(TEXT("The banner clears once Live Coding is decided"), Client.IsWaitingForApproval());
		Client.RequestApproval(Client.GetActiveSession(), TEXT("editor-tool"), TEXT("LiveCoding.Compile"), nullptr,
			[&](bool bApproved) { ++MandatoryDecisions; bMandatoryApproved = bApproved; }, true);
		TestEqual(TEXT("Conversation approval does not auto-run later Live Coding"), MandatoryDecisions, 1);
		TestTrue(TEXT("Later Live Coding still asks"), Client.IsWaitingForApproval());
		Client.ResolveApproval(false);
		TestEqual(TEXT("Later mandatory decision runs exactly once"), MandatoryDecisions, 2);
		TestFalse(TEXT("Later Live Coding can be denied"), bMandatoryApproved);
		Client.SetWorkMode(EACFAssistantWorkMode::Ask);
		Client.SetWorkMode(EACFAssistantWorkMode::AskForApproval);
		Client.RequestApproval(Client.GetActiveSession(), TEXT("editor-tool"), TEXT("LiveCoding.Compile"), nullptr,
			[&](bool bApproved) { ++MandatoryDecisions; bMandatoryApproved = bApproved; }, true);
		Client.ApproveConversation();
		TestEqual(TEXT("Approve all explicitly accepts a visible Live Coding request"), MandatoryDecisions, 3);
		TestTrue(TEXT("Visible Live Coding was approved"), bMandatoryApproved);
		Client.RequestApproval(Client.GetActiveSession(), TEXT("editor-tool"), TEXT("LiveCoding.Compile"), nullptr,
			[&](bool bApproved) { ++MandatoryDecisions; bMandatoryApproved = bApproved; }, true);
		TestEqual(TEXT("Later mandatory requests remain held after visible approval"), MandatoryDecisions, 3);
		Client.ResolveApproval(false);

		Client.SetWorkMode(EACFAssistantWorkMode::ApproveForMe);
		TestTrue(TEXT("Approve for me auto-approves"), Client.IsApproveForMe());
		TestTrue(TEXT("Approve for me runs mutating Editor tools"), Client.JudgeEditorTool(TEXT("call_tool"), Create).Verdict == EACFAssistantToolVerdict::Allow);

		// The dedicated tool and the shortcut always ask; reads that only mention Live Coding do not.
		auto MakeCall = [](const TCHAR* Toolset, const TCHAR* Tool, const TCHAR* Key, const TCHAR* Value)
		{
			auto Call = MakeShared<FJsonObject>();
			Call->SetStringField(TEXT("toolset_name"), Toolset);
			Call->SetStringField(TEXT("tool_name"), Tool);
			auto Arguments = MakeShared<FJsonObject>();
			if (Key) { Arguments->SetStringField(Key, Value); }
			Call->SetObjectField(TEXT("arguments"), Arguments);
			return Call;
		};
		const FACFAssistantToolVerdict Trigger = Client.JudgeEditorTool(TEXT("call_tool"), MakeCall(TEXT("AscentAIAgent.AscentSystemToolset"), TEXT("TriggerLiveCoding"), nullptr, nullptr));
		TestTrue(TEXT("TriggerLiveCoding asks individually in Approve for me"), Trigger.Verdict == EACFAssistantToolVerdict::Ask && Trigger.bAlwaysAsk);
		const FACFAssistantToolVerdict Shortcut = Client.JudgeEditorTool(TEXT("call_tool"), MakeCall(TEXT("SlateInspectorToolset.SlateInspectorToolset"), TEXT("PressKey"), TEXT("key"), TEXT("Ctrl+Alt+F11")));
		TestTrue(TEXT("The Ctrl+Alt+F11 key press asks individually"), Shortcut.Verdict == EACFAssistantToolVerdict::Ask && Shortcut.bAlwaysAsk);
		TestTrue(TEXT("GetLiveCodingStatus is a plain read"), Client.JudgeEditorTool(TEXT("call_tool"), MakeCall(TEXT("AscentAIAgent.AscentSystemToolset"), TEXT("GetLiveCodingStatus"), nullptr, nullptr)).Verdict == EACFAssistantToolVerdict::Allow);
		TestTrue(TEXT("A log search mentioning Live Coding is a plain read"), Client.JudgeEditorTool(TEXT("call_tool"), MakeCall(TEXT("EditorToolset.LogsToolset"), TEXT("GetLogEntries"), TEXT("pattern"), TEXT("LiveCoding|Compile Complete"))).Verdict == EACFAssistantToolVerdict::Allow);
		auto Describe = MakeShared<FJsonObject>();
		Describe->SetStringField(TEXT("toolset_name"), TEXT("LiveCodingToolset.LiveCodingToolset"));
		TestTrue(TEXT("Describing a toolset named for Live Coding is a plain read"), Client.JudgeEditorTool(TEXT("describe_toolset"), Describe).Verdict == EACFAssistantToolVerdict::Allow);

		// A computed command is still arbitrary execution: the label and read_only mode cannot grant permission.
		auto LiveCoding = MakeShared<FJsonObject>();
		LiveCoding->SetStringField(TEXT("toolset_name"), TEXT("AscentAIAgent.AscentSystemToolset"));
		LiveCoding->SetStringField(TEXT("tool_name"), TEXT("PythonRun"));
		auto LiveCodingArguments = MakeShared<FJsonObject>();
		LiveCodingArguments->SetStringField(TEXT("code"), TEXT("command = 'Live' + 'Coding.Compile'\nprint(command)"));
		LiveCodingArguments->SetStringField(TEXT("mode"), TEXT("read_only"));
		LiveCoding->SetObjectField(TEXT("arguments"), LiveCodingArguments);
		const FACFAssistantToolVerdict LiveVerdict = Client.JudgeEditorTool(TEXT("call_tool"), LiveCoding);
		TestTrue(TEXT("Approve for me individually asks before read_only Python"), LiveVerdict.Verdict == EACFAssistantToolVerdict::Ask && LiveVerdict.bAlwaysAsk);
		Decisions = 0;
		Client.RequestApproval(Client.GetActiveSession(), TEXT("editor-tool"), TEXT("PythonRun"), nullptr, [&](bool bApproved) { ++Decisions; bLastDecision = bApproved; }, true);
		TestEqual(TEXT("A Python request is held, not auto-approved"), Decisions, 0);
		Client.ResolveApproval(false);
		TestEqual(TEXT("Denied Python receives one callback"), Decisions, 1);
		TestFalse(TEXT("Denied Python callback refuses execution"), bLastDecision);
		Client.SetWorkMode(EACFAssistantWorkMode::AskForApproval);
		const FACFAssistantToolVerdict EditingVerdict = Client.JudgeEditorTool(TEXT("call_tool"), LiveCoding);
		TestTrue(TEXT("Editing mode individually asks before read_only Python"), EditingVerdict.Verdict == EACFAssistantToolVerdict::Ask && EditingVerdict.bAlwaysAsk);
		Client.ApproveConversation();
		const FACFAssistantToolVerdict ApprovedVerdict = Client.JudgeEditorTool(TEXT("call_tool"), LiveCoding);
		TestTrue(TEXT("Conversation approval cannot silently run later Python"), ApprovedVerdict.Verdict == EACFAssistantToolVerdict::Ask && ApprovedVerdict.bAlwaysAsk);
		Client.SetWorkMode(EACFAssistantWorkMode::Plan);
		TestTrue(TEXT("Plan blocks read_only Python with computed command"), Client.JudgeEditorTool(TEXT("call_tool"), LiveCoding).Verdict == EACFAssistantToolVerdict::Block);

		Client.SetWorkMode(EACFAssistantWorkMode::Ask);
		TestTrue(TEXT("Ask blocks mutations"), Client.IsPlanMode());
		TestTrue(TEXT("Ask blocks read_only Python with computed command"), Client.JudgeEditorTool(TEXT("call_tool"), LiveCoding).Verdict == EACFAssistantToolVerdict::Block);
		// A change requested of a read-only conversation is refused immediately instead of hanging.
		Decisions = 0;
		Client.RequestApproval(Client.GetActiveSession(), TEXT("editor-tool"), TEXT("CreateItem"), nullptr, [&](bool bApproved) { ++Decisions; bLastDecision = bApproved; });
		TestEqual(TEXT("Read-only conversations refuse at once"), Decisions, 1);
		TestFalse(TEXT("Read-only conversations refuse with no"), bLastDecision);
	}
	Settings.WorkMode = SavedWorkMode;
	Settings.SaveConfig();
	return true;
}

/**
 * Drives the work-mode gate over real HTTP, the way Codex and Claude do: initialize, a read-only tool, a mutating tool in Plan (rejected
 * before it reaches the Editor), and a mutating tool in Ask for approval (held on the banner, then refused from the Deny button).
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAscentACFAssistantToolGateTest, "Ascent.ACFAssistant.ToolGateRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
	struct FGateProbe
	{
		FString SessionId;
		int32 LastCode = 0;
		FString LastBody;
		bool bDone = false;
		bool bFailed = false;

		void Post(const FString& Body)
		{
			bDone = false;
			bFailed = false;
			LastCode = 0;
			LastBody.Reset();
			TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
			Request->SetURL(ACFAssistantToolGate::Url());
			Request->SetVerb(TEXT("POST"));
			Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
			Request->SetHeader(TEXT("Accept"), TEXT("application/json, text/event-stream"));
			if (!SessionId.IsEmpty()) { Request->SetHeader(TEXT("Mcp-Session-Id"), SessionId); }
			Request->SetContentAsString(Body);
			Request->OnProcessRequestComplete().BindLambda([this](FHttpRequestPtr, FHttpResponsePtr Response, bool bOk)
			{
				bFailed = !bOk || !Response.IsValid();
				if (Response.IsValid())
				{
					LastCode = Response->GetResponseCode();
					LastBody = Response->GetContentAsString();
					const FString Header = Response->GetHeader(TEXT("Mcp-Session-Id"));
					if (!Header.IsEmpty()) { SessionId = Header; }
				}
				bDone = true;
			});
			Request->ProcessRequest();
		}

		TSharedPtr<FJsonObject> Json() const
		{
			TSharedPtr<FJsonObject> Parsed;
			FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(LastBody), Parsed);
			return Parsed;
		}

		// True when the last reply is an MCP tool result flagged isError whose text contains Needle.
		bool ToolErrorContains(const TCHAR* Needle) const
		{
			const TSharedPtr<FJsonObject> Root = Json();
			if (!Root.IsValid()) { return false; }
			const TSharedPtr<FJsonObject>* Result = nullptr;
			if (!Root->TryGetObjectField(TEXT("result"), Result) || !Result) { return false; }
			bool bIsError = false;
			(*Result)->TryGetBoolField(TEXT("isError"), bIsError);
			return bIsError && LastBody.Contains(Needle);
		}
	};

	FString CallToolBody(int32 Id, const TCHAR* Toolset, const TCHAR* Tool)
	{
		return FString::Printf(TEXT("{\"jsonrpc\":\"2.0\",\"id\":%d,\"method\":\"tools/call\",\"params\":{\"name\":\"call_tool\",\"arguments\":{\"toolset_name\":\"%s\",\"tool_name\":\"%s\",\"arguments\":{}}}}"), Id, Toolset, Tool);
	}
}

bool FAscentACFAssistantToolGateTest::RunTest(const FString& Parameters)
{
	if (IsRunningCommandlet() || FApp::IsUnattended() || !FApp::CanEverRender())
	{
		// The Editor tool server refuses to start headless; the gate has nothing to relay to. Run this test from a rendering Editor.
		AddWarning(TEXT("Skipped: the Editor tool server does not start in unattended or null-RHI sessions."));
		return true;
	}
	if (!AscentEpicMCP::Ensure() || !ACFAssistantToolGate::Ensure())
	{
		AddError(TEXT("The Editor tool server or the gate could not start."));
		return false;
	}
	const TSharedPtr<FAscentACFAssistant> Client = GetSharedACFAssistant(true);
	const EACFAssistantWorkMode SavedMode = Client->GetWorkMode();
	Client->SetWorkMode(EACFAssistantWorkMode::Plan);
	TSharedPtr<FGateProbe> Probe = MakeShared<FGateProbe>();
	TSharedPtr<int32> Step = MakeShared<int32>(0);
	TSharedPtr<double> Started = MakeShared<double>(FPlatformTime::Seconds());

	Probe->Post(TEXT("{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\",\"params\":{\"protocolVersion\":\"2025-06-18\",\"capabilities\":{},\"clientInfo\":{\"name\":\"gate-test\",\"version\":\"1\"}}}"));
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, Client, Probe, Step, Started, SavedMode]() -> bool
	{
		auto Finish = [&]()
		{
			Client->SetWorkMode(SavedMode);
			return true;
		};
		if (FPlatformTime::Seconds() - *Started > 60.0)
		{
			AddError(FString::Printf(TEXT("Gate round trip timed out at step %d"), *Step));
			return Finish();
		}
		if (!Probe->bDone) { return false; }
		if (Probe->bFailed)
		{
			AddError(FString::Printf(TEXT("HTTP request failed at step %d"), *Step));
			return Finish();
		}
		switch (*Step)
		{
		case 0:
			TestEqual(TEXT("initialize is relayed"), Probe->LastCode, 200);
			TestFalse(TEXT("initialize returns a session id"), Probe->SessionId.IsEmpty());
			*Step = 1;
			Probe->Post(TEXT("{\"jsonrpc\":\"2.0\",\"method\":\"notifications/initialized\"}"));
			return false;
		case 1:
			TestTrue(TEXT("notifications are relayed"), Probe->LastCode == 202 || Probe->LastCode == 200);
			*Step = 2;
			Probe->Post(TEXT("{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"tools/call\",\"params\":{\"name\":\"list_toolsets\",\"arguments\":{}}}"));
			return false;
		case 2:
			TestEqual(TEXT("a read-only tool passes in Plan"), Probe->LastCode, 200);
			TestFalse(TEXT("a read-only tool is not blocked"), Probe->ToolErrorContains(TEXT("Blocked")));
			TestTrue(TEXT("a read-only tool reaches the Editor"), Probe->LastBody.Contains(TEXT("\"result\"")));
			*Step = 3;
			Probe->Post(CallToolBody(3, TEXT("AscentAIToolsEditor_ACFDataAssetToolset"), TEXT("SaveDataAsset")));
			return false;
		case 3:
			TestEqual(TEXT("a blocked tool still answers 200 with a tool error"), Probe->LastCode, 200);
			TestTrue(TEXT("Plan blocks a mutating tool at the gate"), Probe->ToolErrorContains(TEXT("Blocked by the Yarin work mode")));
			Client->SetWorkMode(EACFAssistantWorkMode::AskForApproval);
			// The call is held: the reply must not arrive until the user decides. The next stage plays the user.
			Probe->Post(CallToolBody(4, TEXT("AscentAIToolsEditor_ACFDataAssetToolset"), TEXT("SaveDataAsset")));
			return true;
		default:
			break;
		}
		return Finish();
	}));
	// Second latent stage: wait for the banner, deny, and read the refusal the model gets.
	TSharedPtr<bool> Denied = MakeShared<bool>(false);
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, Client, Probe, Started, SavedMode, Denied]() -> bool
	{
		if (FPlatformTime::Seconds() - *Started > 90.0)
		{
			AddError(TEXT("Approval stage timed out."));
			Client->SetWorkMode(SavedMode);
			return true;
		}
		if (!*Denied)
		{
			if (!Client->IsWaitingForApproval()) { return false; }
			TestFalse(TEXT("a held tool has not answered yet"), Probe->bDone);
			const TSharedPtr<FJsonObject> Preview = Client->GetApprovalPreview();
			FString Tool;
			if (Preview.IsValid()) { Preview->TryGetStringField(TEXT("tool"), Tool); }
			TestTrue(TEXT("the banner names the real tool"), Tool.Contains(TEXT("SaveDataAsset")));
			Client->ResolveApproval(false);
			*Denied = true;
			return false;
		}
		if (!Probe->bDone) { return false; }
		TestTrue(TEXT("Deny answers the held call with a refusal"), Probe->ToolErrorContains(TEXT("denied")));
		TestFalse(TEXT("the banner clears after Deny"), Client->IsWaitingForApproval());
		Client->SetWorkMode(SavedMode);
		return true;
	}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAscentACFAssistantToolPolicyTest, "Ascent.ACFAssistant.ToolPolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAscentACFAssistantToolPolicyTest::RunTest(const FString& Parameters)
{
	using namespace ACFAssistantToolPolicy;
	// Ascent toolset names, as the registry spells them.
	for (const TCHAR* ReadOnly : {TEXT("DataAssetInspect"), TEXT("TagList"), TEXT("TagSearch"), TEXT("DescribeClass"), TEXT("ValidateDataAsset"), TEXT("Diff"), TEXT("Summary"),
		TEXT("GetSelectedNodes"), TEXT("ListOverridableFunctions"), TEXT("EstimateComplexity"), TEXT("EnumInspect"), TEXT("ObjectiveListClasses"), TEXT("VoiceSuggest"),
		TEXT("list_toolsets"), TEXT("describe_toolset"), TEXT("get_blueprint_info"), TEXT("find_assets")})
	{
		TestTrue(FString::Printf(TEXT("%s is read-only"), ReadOnly), IsReadOnlyByName(ReadOnly));
	}
	for (const TCHAR* Mutating : {TEXT("CreateItem"), TEXT("DataAssetSetArrayProperty"), TEXT("SetComponentTransform"), TEXT("AddNodes"), TEXT("RemoveComponents"),
		TEXT("ImplementInterface"), TEXT("GenerateAudio"), TEXT("BuildGraph"), TEXT("EnumRenameEntry"), TEXT("compile_blueprint"), TEXT("save_asset"), TEXT("spawn_actor"),
		TEXT("Frobnicate"), TEXT("GetAndDeleteAsset")})
	{
		TestFalse(FString::Printf(TEXT("%s is mutating"), Mutating), IsReadOnlyByName(Mutating));
	}

	// The tool-search relay is unwrapped so the verdict applies to the real tool.
	auto Relay = MakeShared<FJsonObject>();
	Relay->SetStringField(TEXT("toolset_name"), TEXT("AscentAIAgent_AscentDataToolset"));
	Relay->SetStringField(TEXT("tool_name"), TEXT("DataAssetInspect"));
	auto Inner = MakeShared<FJsonObject>();
	Inner->SetStringField(TEXT("asset"), TEXT("/Game/DA_Goblin"));
	Relay->SetObjectField(TEXT("arguments"), Inner);
	const FToolCall Unwrapped = Unwrap(TEXT("call_tool"), Relay);
	TestEqual(TEXT("Relay toolset"), Unwrapped.Toolset, FString(TEXT("AscentAIAgent_AscentDataToolset")));
	TestEqual(TEXT("Relay tool"), Unwrapped.Tool, FString(TEXT("DataAssetInspect")));
	TestTrue(TEXT("Relay inner arguments"), Unwrapped.Arguments.IsValid() && Unwrapped.Arguments->HasField(TEXT("asset")));
	const FToolCall Eager = Unwrap(TEXT("BlueprintTools.CompileBlueprint"), nullptr);
	TestEqual(TEXT("Eager toolset"), Eager.Toolset, FString(TEXT("BlueprintTools")));
	TestEqual(TEXT("Eager tool"), Eager.Tool, FString(TEXT("CompileBlueprint")));

	// An executor remains a change even when its caller declares read_only.
	auto ReadOnlyPython = MakeShared<FJsonObject>();
	ReadOnlyPython->SetStringField(TEXT("mode"), TEXT("read_only"));
	FToolCall Python;
	Python.Toolset = TEXT("AscentAIAgent_AscentSystemToolset");
	Python.Tool = TEXT("PythonRun");
	Python.Arguments = ReadOnlyPython;
	TestFalse(TEXT("read_only Python remains arbitrary execution"), IsReadOnly(Python));
	Python.Tool = TEXT("py_run");
	TestFalse(TEXT("py_run remains arbitrary execution"), IsReadOnly(Python));
	Python.Tool = TEXT("execute_tool_script");
	TestFalse(TEXT("Tool scripts remain arbitrary execution"), IsReadOnly(Python));
	{
		UACFAssistantSettings& Settings = UACFAssistantSettings::Get();
		const TArray<FString> SavedReadOnlyTools = Settings.ReadOnlyTools;
		ON_SCOPE_EXIT { Settings.ReadOnlyTools = SavedReadOnlyTools; };
		Settings.ReadOnlyTools.Add(TEXT("AscentSystemToolset.*"));
		Python.Tool = TEXT("PythonRun");
		TestFalse(TEXT("A settings read-only override cannot permit arbitrary execution"), IsReadOnly(Python));
	}

	// Settings patterns.
	TestTrue(TEXT("Toolset.Tool pattern"), MatchesPattern(TEXT("AscentDataToolset.DataAssetInspect"), TEXT("AscentAIAgent_AscentDataToolset"), TEXT("DataAssetInspect")));
	TestTrue(TEXT("Toolset.* pattern"), MatchesPattern(TEXT("AscentAIAgent_AscentDataToolset.*"), TEXT("AscentAIAgent_AscentDataToolset"), TEXT("Anything")));
	TestTrue(TEXT("Bare tool pattern"), MatchesPattern(TEXT("frobnicate"), TEXT("Whatever"), TEXT("Frobnicate")));
	TestFalse(TEXT("Different toolset does not match"), MatchesPattern(TEXT("OtherToolset.DataAssetInspect"), TEXT("AscentAIAgent_AscentDataToolset"), TEXT("DataAssetInspect")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAscentACFAssistantExportPermissionsTest, "Ascent.ACFAssistant.ExportPermissions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAscentACFAssistantExportPermissionsTest::RunTest(const FString& Parameters)
{
	// Exercise the policy against names supplied by the live registry, not guessed class qualifiers.
	const TMap<FString, bool> Expected = {
		{TEXT("ExportAbility"), true}, {TEXT("ExportAbilitySet"), true},
		{TEXT("ExportCharacter"), true}, {TEXT("ExportDataAsset"), true},
		{TEXT("ExportItem"), true}, {TEXT("ExportText"), true},
		{TEXT("ExportQuest"), true}, {TEXT("GetVoiceCasting"), false}
	};
	TArray<TSharedPtr<FJsonValue>> Toolsets;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(UToolsetRegistry::GetAllToolsetJsonSchemas()), Toolsets))
	{
		AddError(TEXT("Cannot inspect the registered tool catalog"));
		return false;
	}
	TSet<FString> Checked;
	for (const TSharedPtr<FJsonValue>& Value : Toolsets)
	{
		const TSharedPtr<FJsonObject> Toolset = Value.IsValid() ? Value->AsObject() : nullptr;
		if (!Toolset.IsValid()) { continue; }
		FString ToolsetName;
		if (!Toolset->TryGetStringField(TEXT("name"), ToolsetName)) { continue; }
		const TArray<TSharedPtr<FJsonValue>>* Tools = nullptr;
		if (!Toolset->TryGetArrayField(TEXT("tools"), Tools) || !Tools) { continue; }
		for (const TSharedPtr<FJsonValue>& ToolValue : *Tools)
		{
			const TSharedPtr<FJsonObject> Tool = ToolValue.IsValid() ? ToolValue->AsObject() : nullptr;
			FString QualifiedName;
			if (!Tool.IsValid() || !Tool->TryGetStringField(TEXT("name"), QualifiedName)) { continue; }
			const FString Prefix = ToolsetName + TEXT(".");
			if (!QualifiedName.StartsWith(Prefix)) { continue; }
			const FString FunctionName = QualifiedName.RightChop(Prefix.Len());
			const bool* ExpectedReadOnly = Expected.Find(FunctionName);
			if (!ExpectedReadOnly) { continue; }
			ACFAssistantToolPolicy::FToolCall Call = ACFAssistantToolPolicy::Unwrap(QualifiedName, nullptr);
			TestEqual(FString::Printf(TEXT("%s has the intended effective permission"), *QualifiedName),
				ACFAssistantToolPolicy::IsReadOnly(Call), *ExpectedReadOnly);
			Checked.Add(FunctionName);
		}
	}
	// The Export*/voice tools come from the Ascent domain toolsets, which only some installs have (the standalone ships none). With none
	// registered there is nothing to classify; with any registered, every one of them must be present and correctly classified.
	if (Checked.IsEmpty())
	{
		AddInfo(TEXT("No Ascent domain toolsets are registered in this install, so no domain tool permission to check"));
		return true;
	}
	for (const TPair<FString, bool>& Entry : Expected)
	{
		TestTrue(FString::Printf(TEXT("%s is available in the registered tool catalog"), *Entry.Key), Checked.Contains(Entry.Key));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAscentACFAssistantEpicContractTest, "Ascent.ACFAssistant.EpicToolContracts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAscentACFAssistantEpicContractTest::RunTest(const FString& Parameters)
{
	const TSharedPtr<IPlugin> RegistryPlugin = IPluginManager::Get().FindPlugin(TEXT("ToolsetRegistry"));
	const TSharedPtr<IPlugin> AllToolsetsPlugin = IPluginManager::Get().FindPlugin(TEXT("AllToolsets"));
	TestTrue(TEXT("ToolsetRegistry dependency is enabled"), RegistryPlugin.IsValid() && RegistryPlugin->IsEnabled());
	TestTrue(TEXT("AllToolsets dependency is enabled"), AllToolsetsPlugin.IsValid() && AllToolsetsPlugin->IsEnabled());

	TArray<TSharedPtr<FJsonValue>> Toolsets;
	FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(UToolsetRegistry::GetAllToolsetJsonSchemas()), Toolsets);
	TestFalse(TEXT("Epic toolset catalog is available"), Toolsets.IsEmpty());
	TSet<FString> SeenToolsets;
	bool bFoundSceneTools = false;
	bool bFoundCurrentLevel = false;
	bool bFoundActors = false;
	bool bFoundAscentTools = false;
	for (const TSharedPtr<FJsonValue>& Value : Toolsets)
	{
		const TSharedPtr<FJsonObject>* Toolset = nullptr;
		if (!Value->TryGetObject(Toolset) || !Toolset) { AddError(TEXT("Catalog entry is not a toolset object")); continue; }
		const FString ToolsetName = (*Toolset)->GetStringField(TEXT("name"));
		TestFalse(TEXT("Toolset names are unique"), SeenToolsets.Contains(ToolsetName));
		SeenToolsets.Add(ToolsetName);
		bFoundAscentTools |= ToolsetName.StartsWith(TEXT("Ascent"));
		const TArray<TSharedPtr<FJsonValue>>* Tools = nullptr;
		if (!(*Toolset)->TryGetArrayField(TEXT("tools"), Tools) || !Tools)
		{
			AddError(TEXT("Toolset has no tools array: ") + ToolsetName);
			continue;
		}
		TSet<FString> SeenTools;
		for (const TSharedPtr<FJsonValue>& ToolValue : *Tools)
		{
			const TSharedPtr<FJsonObject>* Tool = nullptr;
			if (!ToolValue->TryGetObject(Tool) || !Tool) { AddError(TEXT("Invalid tool schema in ") + ToolsetName); continue; }
			const FString ToolName = (*Tool)->GetStringField(TEXT("name"));
			TestTrue(TEXT("Epic schemas expose qualified tool names"), ToolName.StartsWith(ToolsetName + TEXT(".")));
			TestFalse(TEXT("Tool names are unique within a toolset"), SeenTools.Contains(ToolName));
			SeenTools.Add(ToolName);
			TestTrue(TEXT("Every tool has an input schema"), (*Tool)->HasTypedField<EJson::Object>(TEXT("inputSchema")));
			if (ToolsetName.EndsWith(TEXT("SceneTools")))
			{
				bFoundSceneTools = true;
				bFoundCurrentLevel |= ToolName.EndsWith(TEXT(".get_current_level"));
				bFoundActors |= ToolName.EndsWith(TEXT(".find_actors"));
			}
		}
	}
	TestTrue(TEXT("SceneTools is registered by AllToolsets"), bFoundSceneTools);
	TestTrue(TEXT("SceneTools exposes get_current_level"), bFoundCurrentLevel);
	TestTrue(TEXT("SceneTools exposes find_actors"), bFoundActors);
	TestTrue(TEXT("Ascent toolsets share the Epic registry"), bFoundAscentTools);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAscentACFAssistantMarkdownTest, "Ascent.ACFAssistant.Markdown",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAscentACFAssistantMarkdownTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Bold markers become a Bold run"), ACFAssistantMarkdown::ToRichText(TEXT("**bold**")), FString(TEXT("<Bold>bold</>")));
	TestEqual(TEXT("Inline code markers become a Code run"), ACFAssistantMarkdown::ToRichText(TEXT("`AACFCharacter`")), FString(TEXT("<Code>AACFCharacter</>")));
	TestEqual(TEXT("ATX headings become a Heading run"), ACFAssistantMarkdown::ToRichText(TEXT("### Per creare un nemico o NPC")), FString(TEXT("<Heading3>Per creare un nemico o NPC</>")));
	TestEqual(TEXT("Rich-text special characters are escaped"), ACFAssistantMarkdown::ToRichText(TEXT("5 < 7 & 9")), FString(TEXT("5 &lt; 7 &amp; 9")));
	TestEqual(TEXT("Code spans keep literal asterisks"), ACFAssistantMarkdown::ToRichText(TEXT("`**keep**`")), FString(TEXT("<Code>**keep**</>")));

	const FString Sample = TEXT("### Per creare\nIl metodo e **riutilizzare** un `AACFCharacter`.");
	const FString Rich = ACFAssistantMarkdown::ToRichText(Sample);
	TestFalse(TEXT("Heading hashes are consumed"), Rich.Contains(TEXT("###")));
	TestFalse(TEXT("Bold asterisks are consumed outside code"), Rich.Contains(TEXT("**riutilizzare**")));
	TestTrue(TEXT("Heading style is applied"), Rich.Contains(TEXT("<Heading3>Per creare</>")));
	TestTrue(TEXT("Bold style is applied"), Rich.Contains(TEXT("<Bold>riutilizzare</>")));
	TestTrue(TEXT("Inline code style is applied"), Rich.Contains(TEXT("<Code>AACFCharacter</>")));
	TestEqual(TEXT("Italic markers become an Italic run"), ACFAssistantMarkdown::ToRichText(TEXT("*italic*")), FString(TEXT("<Italic>italic</>")));
	FString BulletLine;
	const TCHAR Bullet = 0x2022;
	BulletLine.AppendChar(Bullet);
	BulletLine += TEXT(" item");
	TestEqual(TEXT("Dash lists become a bullet"), ACFAssistantMarkdown::ToRichText(TEXT("- item")), BulletLine);
	TestEqual(TEXT("Star lists stay lists instead of italic"), ACFAssistantMarkdown::ToRichText(TEXT("* item")), BulletLine);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAscentACFAssistantSelectableTablesTest, "Ascent.ACFAssistant.SelectableTables",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAscentACFAssistantSelectableTablesTest::RunTest(const FString& Parameters)
{
	auto Collect = [](const TSharedRef<SWidget>& Root, TArray<FString>& Cells, TArray<FString>& Prose)
	{
		TFunction<void(const TSharedRef<SWidget>&, bool)> Visit;
		Visit = [&](const TSharedRef<SWidget>& Widget, bool bInTable)
		{
			bInTable |= Widget->GetType() == FName(TEXT("SGridPanel"));
			if (Widget->GetType() == FName(TEXT("SMultiLineEditableText")))
			{
				const FString Text = StaticCastSharedRef<SMultiLineEditableText>(Widget)->GetText().ToString();
				(bInTable ? Cells : Prose).Add(Text);
			}
			FChildren* Children = Widget->GetChildren();
			for (int32 Index = 0; Index < Children->Num(); ++Index)
			{
				Visit(Children->GetChildAt(Index), bInTable);
			}
		};
		Visit(Root, false);
	};
	const auto Body = ACFAssistantMarkdown::CreateSelectableBody(TEXT("| Name | Value |\n| --- | --- |\n| a\\|b | `x|y` |\n\nAfter table"));
	TArray<FString> Cells;
	TArray<FString> Prose;
	Collect(Body, Cells, Prose);
	TestEqual(TEXT("Escaped and code pipes stay inside two selectable data cells"), FString::Join(Cells, TEXT("\n")),
		FString(TEXT("Name\nValue\na|b\n<Code>x|y</>")));
	TestEqual(TEXT("Paragraph following the table remains selectable"), FString::Join(Prose, TEXT("\n")).TrimStartAndEnd(), FString(TEXT("After table")));
	ACFAssistantMarkdown::SetBodyText(Body, TEXT("| Name | Value |\n| --- | --- |\n| a\\|b | `x|y` |\n| next | 42 |\n\nAfter table grows"));
	Cells.Reset();
	Prose.Reset();
	Collect(Body, Cells, Prose);
	TestEqual(TEXT("Streaming adds the new row without retaining stale cells"), FString::Join(Cells, TEXT("\n")),
		FString(TEXT("Name\nValue\na|b\n<Code>x|y</>\nnext\n42")));
	TestEqual(TEXT("Streaming replaces the following paragraph"), FString::Join(Prose, TEXT("\n")).TrimStartAndEnd(), FString(TEXT("After table grows")));
	const TCHAR* NonTables[] = {
		TEXT("```\n| Name | Value |\n| --- | --- |\n| a | b |\n```"),
		TEXT("~~~\n| Name | Value |\n| --- | --- |\n| a | b |\n~~~"),
		TEXT("| Name | Value |\n| -- | --- |\n| a | b |"),
		TEXT("| Name | Value |\n| --- |\n| a | b |")
	};
	for (const TCHAR* Markdown : NonTables)
	{
		ACFAssistantMarkdown::SetBodyText(Body, Markdown);
		Cells.Reset();
		Prose.Reset();
		Collect(Body, Cells, Prose);
		TestTrue(TEXT("Fenced or malformed separators do not manufacture table cells"), Cells.IsEmpty());
		TestTrue(TEXT("Non-table pipe content remains visible and selectable"), FString::Join(Prose, TEXT("\n")).Contains(TEXT("| Name | Value |")));
	}
	ACFAssistantMarkdown::SetBodyText(Body, TEXT("| Name | Value |\n| --- | --- |\n| a | b |\n| malformed |\n\nStill prose"));
	Cells.Reset();
	Prose.Reset();
	Collect(Body, Cells, Prose);
	TestEqual(TEXT("A malformed data row does not leak into the preceding table"), FString::Join(Cells, TEXT("\n")), FString(TEXT("Name\nValue\na\nb")));
	TestEqual(TEXT("A malformed row and following paragraph remain selectable prose"), FString::Join(Prose, TEXT("\n")).TrimStartAndEnd(),
		FString(TEXT("| malformed |\n\nStill prose")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAscentACFAssistantPasteImageTest, "Ascent.ACFAssistant.PasteImage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAscentACFAssistantPasteImageTest::RunTest(const FString& Parameters)
{
	FAscentACFAssistant Client;
	const FString Outside = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("ACFAssistant/Automation/paste_probe.bin"));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Outside), true);
	TestTrue(TEXT("Wrote a probe file outside the Codex workspace"), FFileHelper::SaveStringToFile(TEXT("probe"), *Outside));
	Client.AttachImage(Outside);
	TestEqual(TEXT("The probe is queued as one attachment"), Client.GetPendingImages().Num(), 1);
	if (Client.GetPendingImages().Num() == 1)
	{
		const FString Attached = Client.GetPendingImages()[0];
		TestTrue(TEXT("The queued path exists on disk"), FPaths::FileExists(Attached));
		TestFalse(TEXT("Codex receives an absolute path"), FPaths::IsRelative(Attached));
		FString Workspace = FPaths::ConvertRelativePathToFull(Client.WorkingDirectory);
		FPaths::NormalizeDirectoryName(Workspace);
		TestTrue(TEXT("The copy lives inside the isolated Codex working folder"), FPaths::IsUnderDirectory(Attached, Workspace));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAscentACFAssistantGeneratedImageTest, "Ascent.ACFAssistant.GeneratedImageInChat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAscentACFAssistantGeneratedImageTest::RunTest(const FString& Parameters)
{
	FAscentACFAssistant Client;
	const FString ImagePath = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("ACFAssistant/Automation/generated_preview.png"));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(ImagePath), true);
	TestTrue(TEXT("Wrote a generated-image probe"), FFileHelper::SaveStringToFile(TEXT("png"), *ImagePath));

	const TSharedPtr<FACFAssistantSession> Session = Client.GetActiveSession();
	Session->ThreadId = TEXT("thread-generated-image");
	auto Notify = [&Client, &Session](const FString& Method, const TSharedPtr<FJsonObject>& Item)
	{
		TSharedPtr<FJsonObject> Params = MakeShared<FJsonObject>();
		Params->SetStringField(TEXT("threadId"), Session->ThreadId);
		Params->SetObjectField(TEXT("item"), Item);
		TSharedPtr<FJsonObject> Message = MakeShared<FJsonObject>();
		Message->SetStringField(TEXT("method"), Method);
		Message->SetObjectField(TEXT("params"), Params);
		Client.Handle(Message);
	};

	TSharedPtr<FJsonObject> Generated = MakeShared<FJsonObject>();
	Generated->SetStringField(TEXT("type"), TEXT("imageGeneration"));
	Generated->SetStringField(TEXT("id"), TEXT("img_1"));
	Generated->SetStringField(TEXT("savedPath"), ImagePath);
	Notify(TEXT("item/completed"), Generated);
	FString ExpectedPath = ImagePath;
	FPaths::NormalizeFilename(ExpectedPath);
	FPaths::MakePlatformFilename(ExpectedPath);
	TestTrue(TEXT("Codex imageGeneration appears in chat"), Client.GetMessages().ContainsByPredicate([&](const FACFAssistantMessage& Message)
	{
		return Message.Role == TEXT("Image") && Message.Text.Equals(ExpectedPath, ESearchCase::IgnoreCase);
	}));

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("image_file"), ImagePath);
	TSharedPtr<FJsonObject> Content = MakeShared<FJsonObject>();
	Content->SetStringField(TEXT("type"), TEXT("text"));
	Content->SetStringField(TEXT("text"), ACFAssistantTools::Encode(Result));
	TArray<TSharedPtr<FJsonValue>> ContentItems;
	ContentItems.Add(MakeShared<FJsonValueObject>(Content));
	TSharedPtr<FJsonObject> ToolResult = MakeShared<FJsonObject>();
	ToolResult->SetArrayField(TEXT("content"), ContentItems);
	TSharedPtr<FJsonObject> ToolItem = MakeShared<FJsonObject>();
	ToolItem->SetStringField(TEXT("type"), TEXT("mcpToolCall"));
	ToolItem->SetStringField(TEXT("tool"), TEXT("ImportImageFile"));
	ToolItem->SetObjectField(TEXT("result"), ToolResult);
	const int32 CountBeforeImport = Client.GetMessages().Num();
	Notify(TEXT("item/completed"), ToolItem);
	int32 ImageCount = 0;
	for (const FACFAssistantMessage& Message : Client.GetMessages())
	{
		if (Message.Role == TEXT("Image")) { ++ImageCount; }
	}
	TestEqual(TEXT("The same generated file is not shown twice"), ImageCount, 1);
	TestEqual(TEXT("ImportImageFile does not append a duplicate image row"), Client.GetMessages().Num(), CountBeforeImport);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAscentACFAssistantParallelSessionsTest, "Ascent.ACFAssistant.ParallelConversations",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAscentACFAssistantParallelSessionsTest::RunTest(const FString& Parameters)
{
	// SetWorkMode below persists the user's preference; restore it so this test leaves no trace.
	UACFAssistantSettings& Settings = UACFAssistantSettings::Get();
	const EACFAssistantWorkMode SavedWorkMode = Settings.WorkMode;
	ON_SCOPE_EXIT
	{
		Settings.WorkMode = SavedWorkMode;
		Settings.SaveConfig();
	};

	FAscentACFAssistant Client;

	const TSharedPtr<FACFAssistantSession> First = Client.GetActiveSession();
	First->ThreadId = TEXT("thread-a");
	First->bBusy = true;
	First->Messages.Add({TEXT("You"), TEXT("build me a sword")});

	// A conversation that is answering must not block opening another one.
	Client.NewConversation();
	TestEqual(TEXT("A second conversation opens while the first is answering"), Client.GetSessions().Num(), 2);
	const TSharedPtr<FACFAssistantSession> Second = Client.GetActiveSession();
	TestNotEqual(TEXT("The new conversation is a different session"), Second->LocalId, First->LocalId);
	TestTrue(TEXT("The first conversation is still answering"), First->bBusy);
	TestFalse(TEXT("The active conversation is idle"), Client.IsBusy());
	TestTrue(TEXT("The client still reports work in flight"), Client.IsAnyBusy());
	TestTrue(TEXT("The second conversation starts empty"), Client.GetMessages().IsEmpty());

	// Work mode belongs to the conversation, not to the client.
	const EACFAssistantWorkMode FirstWorkMode = First->Permissions.WorkMode;
	Second->Permissions.WorkMode = EACFAssistantWorkMode::Plan;
	Client.SetWorkMode(EACFAssistantWorkMode::ApproveForMe);
	TestTrue(TEXT("Work mode changes the active conversation"), Client.IsApproveForMe());
	TestEqual(TEXT("Work mode leaves the other conversation alone"), First->Permissions.WorkMode, FirstWorkMode);

	// Streaming is routed by thread id, so a background conversation keeps filling up while another tab is on screen.
	TSharedPtr<FJsonObject> DeltaParams = MakeShared<FJsonObject>();
	DeltaParams->SetStringField(TEXT("threadId"), TEXT("thread-a"));
	DeltaParams->SetStringField(TEXT("itemId"), TEXT("item-a"));
	DeltaParams->SetStringField(TEXT("delta"), TEXT("forging"));
	TSharedPtr<FJsonObject> Delta = MakeShared<FJsonObject>();
	Delta->SetStringField(TEXT("method"), TEXT("item/agentMessage/delta"));
	Delta->SetObjectField(TEXT("params"), DeltaParams);
	Client.Handle(Delta);
	TestEqual(TEXT("The answer reaches the background conversation"), First->Messages.Num(), 2);
	TestTrue(TEXT("The visible conversation is untouched"), Client.GetMessages().IsEmpty());

	Client.ActivateSession(First->LocalId);
	TestEqual(TEXT("Switching shows the other transcript"), Client.GetMessages().Num(), 2);
	TestTrue(TEXT("Switching does not interrupt the conversation"), First->bBusy);

	// One conversation failing must not stop the others.
	Client.FailSession(Second, TEXT("Something went wrong."));
	TestTrue(TEXT("The still-running conversation survives a failure elsewhere"), First->bBusy);

	const int32 SecondId = Second->LocalId;
	Client.CloseSession(SecondId);
	TestEqual(TEXT("Closing removes only that conversation"), Client.GetSessions().Num(), 1);
	TestEqual(TEXT("The surviving conversation stays open"), Client.GetSessions()[0]->LocalId, First->LocalId);

	Client.CloseSession(First->LocalId);
	TestEqual(TEXT("Closing the last conversation leaves a fresh empty one"), Client.GetSessions().Num(), 1);
	TestTrue(TEXT("The fresh conversation has no thread yet"), Client.GetSessions()[0]->ThreadId.IsEmpty());
	const FString SavedModel = Settings.ModelId;
	const FString SavedReasoning = Settings.ReasoningEffort;
	const FString FixtureDirectory = FPaths::ProjectSavedDir() / TEXT("ACFAssistant/Automation") / FGuid::NewGuid().ToString();
	ON_SCOPE_EXIT
	{
		Settings.ModelId = SavedModel;
		Settings.ReasoningEffort = SavedReasoning;
		Settings.SaveConfig();
		IFileManager::Get().DeleteDirectory(*FixtureDirectory, false, true);
	};
	for (int32 ProtectedCase = -1; ProtectedCase < 6; ++ProtectedCase)
	{
		FAscentACFAssistant Picker;
		Picker.ProfileDirectory = FixtureDirectory;
		Picker.PreferencesPath = FixtureDirectory / TEXT("preferences.json");
		IFileManager::Get().MakeDirectory(*FixtureDirectory, true);
		Picker.Sessions.Reset();
		Picker.History.Reset();
		Picker.Models.Reset();
		for (const EACFAssistantProvider Provider : {EACFAssistantProvider::Codex, EACFAssistantProvider::ClaudeCode})
		{
			auto Model = MakeShared<FACFAssistantModel>();
			Model->Provider = Provider;
			Model->Id = Provider == EACFAssistantProvider::Codex ? TEXT("test-codex") : TEXT("test-claude");
			Picker.Models.Add(Model);
		}
		Picker.ModelId = TEXT("test-codex");
		Settings.ModelId = Picker.ModelId;
		for (int32 Index = 0; Index < FAscentACFAssistant::MaxOpenSessions; ++Index)
		{
			const auto Session = Picker.OpenSession();
			Session->ThreadId = FString::Printf(TEXT("test-thread-%d"), Index);
			Session->ModelId = TEXT("test-codex");
			Session->Provider = EACFAssistantProvider::Codex;
			Session->Messages.Add({TEXT("You"), FString::Printf(TEXT("history-%d"), Index)});
			Session->HistoryEntry = MakeShared<FACFAssistantHistory>();
			Session->HistoryEntry->Id = Session->ThreadId;
			Picker.History.Add(Session->HistoryEntry);
			if (Index > 1) { Session->bBusy = true; }
		}
		const auto Selected = Picker.GetSessions()[0];
		const auto Candidate = Picker.GetSessions()[1];
		switch (ProtectedCase)
		{
		case 0: Candidate->bBusy = true; break;
		case 1: Candidate->PendingImages.Add(TEXT("unsent.png")); break;
		case 2: Candidate->PendingText = TEXT("unsent message"); break;
		case 3: Candidate->SteerText = TEXT("queued redirect"); break;
		case 4: Candidate->Approvals.Add(MakeShared<FACFAssistantApproval>()); break;
		case 5: Candidate->UserInputRequestId = MakeShared<FJsonValueNumber>(7); break;
		}
		Picker.ActivateSession(Selected->LocalId);
		const FString Case = FString::Printf(TEXT("Cross-provider cap protection %d"), ProtectedCase);
		Picker.SelectModel(TEXT("test-claude"));
		TestEqual(*(Case + TEXT(" keeps exactly eight open conversations")), Picker.GetSessions().Num(), FAscentACFAssistant::MaxOpenSessions);
		TestTrue(*(Case + TEXT(" keeps the selected saved conversation open")), Picker.GetSessions().Contains(Selected));
		TestEqual(*(Case + TEXT(" preserves selected history content")), Selected->Messages[0].Text, FString(TEXT("history-0")));
		TestEqual(*(Case + TEXT(" does not convert the original provider thread")), Selected->ModelId, FString(TEXT("test-codex")));
		TestEqual(*(Case + TEXT(" preserves history entries")), Picker.History.Num(), FAscentACFAssistant::MaxOpenSessions);
		TestTrue(*(Case + TEXT(" retains the retired or protected history entry")), Picker.History.Contains(Candidate->HistoryEntry));
		if (ProtectedCase < 0)
		{
			TestFalse(TEXT("The other idle saved view is retired"), Picker.GetSessions().Contains(Candidate));
			TestNotEqual(TEXT("The new provider conversation is selected"), Picker.GetActiveSessionId(), Selected->LocalId);
			TestEqual(TEXT("The actual active model is the accepted model"), Picker.GetModelId(), FString(TEXT("test-claude")));
			TestTrue(TEXT("The accepted conversation belongs to Claude"), Picker.GetActiveSession()->Provider == EACFAssistantProvider::ClaudeCode);
			TestTrue(TEXT("The new provider does not inherit Codex history"), Picker.GetMessages().IsEmpty());
			TestEqual(TEXT("The accepted model becomes the saved preference"), Settings.ModelId, FString(TEXT("test-claude")));
		}
		else
		{
			TestTrue(*(Case + TEXT(" keeps the protected session open")), Picker.GetSessions().Contains(Candidate));
			TestEqual(*(Case + TEXT(" keeps the active conversation")), Picker.GetActiveSessionId(), Selected->LocalId);
			TestEqual(*(Case + TEXT(" keeps the actual model")), Picker.GetModelId(), FString(TEXT("test-codex")));
			TestEqual(*(Case + TEXT(" keeps the new-conversation preference")), Picker.ModelId, FString(TEXT("test-codex")));
			TestEqual(*(Case + TEXT(" keeps the saved preference")), Settings.ModelId, FString(TEXT("test-codex")));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAscentACFAssistantSuggestedRepliesTest, "Ascent.ACFAssistant.SuggestedReplies",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAscentACFAssistantSuggestedRepliesTest::RunTest(const FString& Parameters)
{
	FAscentACFAssistant Client;
	const TSharedPtr<FACFAssistantSession> Session = Client.GetActiveSession();
	Session->ThreadId = TEXT("thread-suggest");
	Session->bBusy = true;

	auto Option = [](const TCHAR* Label, const TCHAR* Description)
	{
		TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("label"), Label);
		Json->SetStringField(TEXT("description"), Description);
		return Json;
	};
	auto Question = [&Option](const TCHAR* Id, const TCHAR* Text, const TCHAR* First, const TCHAR* Second)
	{
		TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("id"), Id);
		Json->SetStringField(TEXT("header"), Id);
		Json->SetStringField(TEXT("question"), Text);
		Json->SetArrayField(TEXT("options"), {
			MakeShared<FJsonValueObject>(Option(First, First)),
			MakeShared<FJsonValueObject>(Option(Second, Second))
		});
		return Json;
	};

	TSharedPtr<FJsonObject> Params = MakeShared<FJsonObject>();
	Params->SetStringField(TEXT("threadId"), Session->ThreadId);
	Params->SetStringField(TEXT("turnId"), TEXT("turn-suggest"));
	Params->SetStringField(TEXT("itemId"), TEXT("call-suggest"));
	Params->SetArrayField(TEXT("questions"), {
		MakeShared<FJsonValueObject>(Question(TEXT("kind"), TEXT("Che personaggio?"), TEXT("Giocatore"), TEXT("Nemico"))),
		MakeShared<FJsonValueObject>(Question(TEXT("scope"), TEXT("Quanto completo?"), TEXT("Prova"), TEXT("Giocabile")))
	});
	TSharedPtr<FJsonObject> Message = MakeShared<FJsonObject>();
	Message->SetNumberField(TEXT("id"), 7);
	Message->SetStringField(TEXT("method"), TEXT("item/tool/requestUserInput"));
	Message->SetObjectField(TEXT("params"), Params);
	Client.Handle(Message);

	TestTrue(TEXT("Codex questions become clickable suggestions"), Client.HasSuggestedReplies());
	TestEqual(TEXT("Both questions are kept"), Client.GetSuggestedQuestions().Num(), 2);
	TestEqual(TEXT("The first question offers two answers"), Client.GetSuggestedQuestions()[0].Options.Num(), 2);
	TestTrue(TEXT("The question is shown in the transcript"), Client.GetMessages().ContainsByPredicate([](const FACFAssistantMessage& Row)
	{
		return Row.Role == TEXT("Assistant") && Row.Text.Contains(TEXT("Che personaggio?"));
	}));

	Client.AnswerSuggestedReply(TEXT("kind"), TEXT("Nemico"));
	TestTrue(TEXT("One answer does not release Codex yet"), Client.HasSuggestedReplies());
	TestTrue(TEXT("The clicked answer is recorded as the user's line"), Client.GetMessages().ContainsByPredicate([](const FACFAssistantMessage& Row)
	{
		return Row.Role == TEXT("You") && Row.Text == TEXT("Nemico");
	}));

	Client.AnswerSuggestedReply(TEXT("scope"), TEXT("Prova"));
	TestFalse(TEXT("The last click answers the request"), Client.HasSuggestedReplies());
	TestFalse(TEXT("Answered questions are gone"), Client.IsSuggestedAnswered(TEXT("kind")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAscentACFAssistantYarinTest, "Ascent.ACFAssistant.YarinProvider",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAscentACFAssistantYarinTest::RunTest(const FString& Parameters)
{
	using EProvider = EACFAssistantProvider;
	const FString ModelsJson = TEXT("{\"data\":[{\"id\":\"yarin-balanced\"},{\"id\":\"deepseek-v4-flash\"},")
		TEXT("{\"id\":\"yarin-image\"},{\"id\":\"yarin-pro\"},{\"id\":\"yarin-fast\"},{\"id\":\"yarin-free\"}]}");
	const FString InfoJson = TEXT("{\"data\":[")
		TEXT("{\"model_name\":\"yarin-balanced\",\"model_info\":{\"mode\":\"chat\",\"max_input_tokens\":1048576}},")
		TEXT("{\"model_name\":\"deepseek-v4-flash\",\"model_info\":{\"mode\":\"chat\",\"max_input_tokens\":1310720}},")
		TEXT("{\"model_name\":\"yarin-image\",\"model_info\":{\"mode\":\"image_generation\",\"max_input_tokens\":131072}},")
		TEXT("{\"model_name\":\"yarin-pro\",\"model_info\":{\"mode\":\"chat\",\"max_input_tokens\":1048576}},")
		TEXT("{\"model_name\":\"yarin-fast\",\"model_info\":{\"mode\":\"chat\",\"max_input_tokens\":1048576}},")
		TEXT("{\"model_name\":\"yarin-free\",\"model_info\":{\"mode\":\"chat\",\"max_input_tokens\":262144}},")
		TEXT("{\"model_name\":\"yarin-free\",\"model_info\":{\"mode\":\"chat\",\"max_input_tokens\":1000000}},")
		TEXT("{\"model_name\":\"yarin-free\",\"model_info\":{\"mode\":\"chat\",\"max_input_tokens\":256000}},")
		// An entitlement group the gateway reports only in /model/info: a chat-mode row must still never become a picker entry.
		TEXT("{\"model_name\":\"tier-indie\",\"model_info\":{\"mode\":\"chat\",\"max_input_tokens\":1048576}}]}");
	const TArray<TSharedPtr<FACFAssistantModel>> Catalog = FAscentACFAssistant::ParseYarinModels(ModelsJson, InfoJson);
	const TCHAR* const ExpectedIds[] = {TEXT("yarin-balanced"), TEXT("yarin-pro"), TEXT("yarin-fast"), TEXT("yarin-free"), TEXT("deepseek-v4-flash")};
	const TCHAR* const ExpectedLabels[] = {TEXT("Yarin Balanced"), TEXT("Yarin Pro"), TEXT("Yarin Fast"), TEXT("Yarin Free"), TEXT("deepseek-v4-flash")};
	const int64 ExpectedWindows[] = {1048576, 1048576, 1048576, 256000, 1310720};
	TestEqual(TEXT("Only callable gateway models are listed, not image models or entitlement groups"), Catalog.Num(), static_cast<int32>(UE_ARRAY_COUNT(ExpectedIds)));
	for (int32 Index = 0; Index < Catalog.Num() && Index < UE_ARRAY_COUNT(ExpectedIds); ++Index)
	{
		const FString Case = FString::Printf(TEXT("Gateway model %d"), Index);
		TestEqual(*(Case + TEXT(" picker order and collision-proof id")), Catalog[Index]->Id, FString(TEXT("yarin/")) + ExpectedIds[Index]);
		TestEqual(*(Case + TEXT(" bare wire id")), Catalog[Index]->ApiId, FString(ExpectedIds[Index]));
		TestEqual(*(Case + TEXT(" user-facing label")), Catalog[Index]->Label, FString(ExpectedLabels[Index]));
		TestEqual(*(Case + TEXT(" safe context window")), Catalog[Index]->ContextWindowTokens, ExpectedWindows[Index]);
		TestTrue(*(Case + TEXT(" uses the Yarin account")), Catalog[Index]->Provider == EProvider::Yarin);
	}
	TestFalse(TEXT("A name that only /model/info reports never becomes a model"), Catalog.ContainsByPredicate([](const TSharedPtr<FACFAssistantModel>& Model)
	{
		return Model->ApiId == TEXT("tier-indie");
	}));
	const TArray<TSharedPtr<FACFAssistantModel>> WithoutInfo = FAscentACFAssistant::ParseYarinModels(ModelsJson, FString());
	TestEqual(TEXT("An unavailable model-info endpoint does not hide callable models"), WithoutInfo.Num(), 6);
	for (const TSharedPtr<FACFAssistantModel>& Model : WithoutInfo)
	{
		TestEqual(TEXT("An unknown context window is not fabricated"), Model->ContextWindowTokens, int64(0));
	}

	// Built by concatenation: a NUL inside a string literal would end the C string, and a NUL in a decoded JSON string is exactly the hostile case.
	const FString NulUrl = FString(TEXT("https://gateway.example.invalid/v1")) + FString::ChrN(1, TEXT('\0')) + TEXT("x");
	for (const FString& InvalidUrl : {FString(TEXT("https://gateway.example.invalid/\"bad")), FString(TEXT("https://gateway.example.invalid/\nunsafe")),
		FString(TEXT("https://gateway.example.invalid/\\unsafe")), FString(TEXT("https://gateway.example.invalid/\runsafe")), NulUrl, FString(TEXT("http://gateway.example.invalid/v1"))})
	{
		TestFalse(TEXT("An unsafe gateway URL is refused"), FAscentACFAssistant::IsUsableGatewayUrl(InvalidUrl));
	}
	// One rule for every place a gateway address or key is accepted (cloud answer, stored session, config.toml, Authorization header, process environment).
	TestTrue(TEXT("The real gateway address is usable"), FAscentACFAssistant::IsUsableGatewayUrl(TEXT("https://yarin.darktowerinteractive.com/litellm/v1")));
	TestTrue(TEXT("A loopback development gateway is usable"), FAscentACFAssistant::IsUsableGatewayUrl(TEXT("http://127.0.0.1:4000/v1")));
	TestFalse(TEXT("A gateway address with a space is refused"), FAscentACFAssistant::IsUsableGatewayUrl(TEXT("https://gateway.example.invalid/a b")));
	TestFalse(TEXT("A gateway address with non-ASCII is refused"), FAscentACFAssistant::IsUsableGatewayUrl(TEXT("https://gateway.example.invalid/\u00e9")));
	TestFalse(TEXT("An embedded NUL is refused in a gateway address"), FAscentACFAssistant::IsUsableGatewayUrl(NulUrl));
	TestTrue(TEXT("A LiteLLM key is usable"), FAscentACFAssistant::IsUsableGatewayKey(TEXT("sk-AbC_123-xyz")));
	const FString NulKey = FString(TEXT("sk-ok")) + FString::ChrN(1, TEXT('\0')) + TEXT("HTTPS_PROXY=http://evil");
	TestFalse(TEXT("A NUL cannot smuggle extra environment entries behind a key"), FAscentACFAssistant::IsUsableGatewayKey(NulKey));
	for (const TCHAR* BadKey : {TEXT(""), TEXT("sk bad"), TEXT("sk-bad\r\nX-Evil: 1"), TEXT("sk-b\u00e9d")})
	{
		TestFalse(*(FString(TEXT("A key that could break out of a header or the environment is refused: ")) + BadKey), FAscentACFAssistant::IsUsableGatewayKey(BadKey));
	}
	TestFalse(TEXT("An absurdly long key is refused"), FAscentACFAssistant::IsUsableGatewayKey(FString::ChrN(513, TEXT('k'))));

	FAscentACFAssistant Client;
	auto Model = [](const TCHAR* Id, const TCHAR* ApiId, EProvider Provider, int64 Window = 0)
	{
		TSharedPtr<FACFAssistantModel> Result = MakeShared<FACFAssistantModel>();
		Result->Id = Id;
		Result->ApiId = ApiId;
		Result->Provider = Provider;
		Result->ContextWindowTokens = Window;
		return Result;
	};
	Client.Models.Add(Model(TEXT("yarin/yarin-fast"), TEXT("yarin-fast"), EProvider::Yarin, 262144));
	Client.Models.Add(Model(TEXT("gpt-5"), TEXT("gpt-5"), EProvider::Codex));
	const TSharedPtr<FACFAssistantSession> Session = Client.GetActiveSession();
	Session->ModelId = TEXT("gpt-5");
	Session->Provider = EProvider::Codex;
	const TSharedPtr<FJsonObject> CodexThread = Client.ThreadParameters(Session);
	TestEqual(TEXT("A Codex thread retains its own model"), CodexThread->GetStringField(TEXT("model")), FString(TEXT("gpt-5")));
	TestFalse(TEXT("A Codex thread never inherits the Yarin provider"), CodexThread->HasField(TEXT("modelProvider")));
	TestFalse(TEXT("A Codex thread never inherits the Yarin config"), CodexThread->HasField(TEXT("config")));
	TestEqual(TEXT("A temporarily unlisted Yarin model still loses the picker prefix"), Client.WireModelId(TEXT("yarin/not-listed")), FString(TEXT("not-listed")));

	const FString Cloud = TEXT("https://yarin.darktowerinteractive.com");
	const FString DeviceUrl = Cloud + TEXT("/auth/device?code=ABCD-EFGH");
	TestTrue(TEXT("The configured cloud device page is trusted"), FAscentACFAssistant::IsYarinSignInUrl(DeviceUrl, Cloud));
	TestTrue(TEXT("A trailing slash on the configured cloud is harmless"), FAscentACFAssistant::IsYarinSignInUrl(DeviceUrl, Cloud + TEXT("/")));
	TestFalse(TEXT("A look-alike hostname cannot receive a login code"), FAscentACFAssistant::IsYarinSignInUrl(TEXT("https://yarin.darktowerinteractive.com.evil.example/auth/device?code=X"), Cloud));
	TestFalse(TEXT("Non-loopback HTTP cannot receive a login code"), FAscentACFAssistant::IsYarinSignInUrl(TEXT("http://yarin.darktowerinteractive.com/auth/device?code=X"), TEXT("http://yarin.darktowerinteractive.com")));
	TestFalse(TEXT("Other cloud pages cannot receive a login code"), FAscentACFAssistant::IsYarinSignInUrl(Cloud + TEXT("/auth/other?code=X"), Cloud));
	TestFalse(TEXT("An absent trusted cloud cannot approve a URL"), FAscentACFAssistant::IsYarinSignInUrl(DeviceUrl, FString()));
	TestFalse(TEXT("A host that merely starts with localhost is not loopback"), FAscentACFAssistant::IsYarinSignInUrl(TEXT("http://localhost.evil.example/auth/device?code=X"), TEXT("http://localhost.evil.example")));
	TestFalse(TEXT("A host that merely starts with 127.0.0.1 is not loopback"), FAscentACFAssistant::IsYarinSignInUrl(TEXT("http://127.0.0.1.evil.example/auth/device?code=X"), TEXT("http://127.0.0.1.evil.example")));
	TestTrue(TEXT("A real loopback development cloud is accepted"), FAscentACFAssistant::IsYarinSignInUrl(TEXT("http://localhost:3005/auth/device?code=X"), TEXT("http://localhost:3005")));

#if PLATFORM_WINDOWS
	const FString Secret = TEXT("clé \"雪\" gateway");
	TArray<uint8> Protected;
	if (TestTrue(TEXT("The current Windows user can protect a gateway secret"), FAscentACFAssistant::ProtectSecret(Secret, Protected)))
	{
		FString Recovered;
		TestTrue(TEXT("The current Windows user can decrypt the protected secret"), FAscentACFAssistant::UnprotectSecret(Protected, Recovered));
		TestEqual(TEXT("DPAPI preserves unicode and quotes exactly"), Recovered, Secret);
		const FTCHARToUTF8 Utf8(*Secret);
		bool bContainsPlaintext = false;
		for (int32 Offset = 0; Offset + Utf8.Length() <= Protected.Num(); ++Offset)
		{
			if (FMemory::Memcmp(Protected.GetData() + Offset, Utf8.Get(), Utf8.Length()) == 0) { bContainsPlaintext = true; break; }
		}
		TestFalse(TEXT("Encrypted bytes do not disclose UTF-8 plaintext"), bContainsPlaintext);
	}
	FString RecoveredGarbage = TEXT("stale plaintext");
	TestFalse(TEXT("Invalid ciphertext is rejected"), FAscentACFAssistant::UnprotectSecret({0x01, 0x02, 0x03}, RecoveredGarbage));
	TestTrue(TEXT("Invalid ciphertext clears previously recovered plaintext"), RecoveredGarbage.IsEmpty());
#endif

	Client.SetProviderModels(EProvider::ClaudeCode, {Model(TEXT("claude-sonnet"), TEXT("sonnet"), EProvider::ClaudeCode)});
	Client.SetProviderModels(EProvider::Codex, {Model(TEXT("gpt-5"), TEXT("gpt-5"), EProvider::Codex)});
	Client.SetProviderModels(EProvider::Yarin, {Model(TEXT("yarin/yarin-fast"), TEXT("yarin-fast"), EProvider::Yarin)});
	TestEqual(TEXT("A disabled gateway catalog cannot enter the subscription picker"), Client.Models.Num(), 2);
	TestFalse(TEXT("The disabled gateway model is not selectable"), Client.FindModel(TEXT("yarin/yarin-fast")).IsValid());
	TestTrue(TEXT("Both subscription catalogs remain selectable"), Client.FindModel(TEXT("gpt-5")).IsValid() && Client.FindModel(TEXT("claude-sonnet")).IsValid());
	Client.EnterSignedOut(EProvider::Codex, TEXT("Signed out"));
	TestEqual(TEXT("Signing out of ChatGPT leaves only Claude's catalog"), Client.Models.Num(), 1);
	TestFalse(TEXT("A ChatGPT model is removed at sign-out"), Client.FindModel(TEXT("gpt-5")).IsValid());
	TestTrue(TEXT("A Claude model survives ChatGPT sign-out"), Client.FindModel(TEXT("claude-sonnet")).IsValid());
	Session->Provider = EProvider::Yarin;
	Session->bBusy = true;
	TestTrue(TEXT("A Yarin turn keeps its shared Codex process busy"), Client.IsAnyBusy(EProvider::Codex));
	TestFalse(TEXT("A Yarin turn does not block the Claude process"), Client.IsAnyBusy(EProvider::ClaudeCode));

	Client.SignIn(EProvider::Yarin);
	TestTrue(TEXT("Gateway sign-in is blocked before any cached account exists"), Client.GetConnectionState(EProvider::Yarin) == FAscentACFAssistant::EConnectionState::Disconnected);
	Client.Connect(EProvider::Yarin, false);
	TestTrue(TEXT("Gateway connect cannot start a cloud or runtime connection"), Client.GetConnectionState(EProvider::Yarin) == FAscentACFAssistant::EConnectionState::Disconnected);
	Client.YarinCredential.DeviceToken = TEXT("fake-device-token");
	Client.YarinCredential.GatewayKey = TEXT("sk-fake-test-key");
	Client.YarinCredential.GatewayBaseUrl = TEXT("https://gateway.example.invalid/v1");
	Client.Yarin.State = FAscentACFAssistant::EConnectionState::Ready;
	Client.Yarin.bAuthenticated = true;
	Client.Yarin.bInitialized = true;
	TestFalse(TEXT("Cached credentials and Ready state cannot activate the disabled gateway"), Client.IsProviderReady(EProvider::Yarin));
	Client.SignIn(EProvider::Yarin);
	Client.Connect(EProvider::Yarin, true);
	TestTrue(TEXT("Blocked gateway account actions do not start a connection"), Client.GetConnectionState(EProvider::Yarin) == FAscentACFAssistant::EConnectionState::Ready);
	TestFalse(TEXT("Blocked account actions cannot make the gateway usable"), Client.IsProviderReady(EProvider::Yarin));
	Client.Codex.State = FAscentACFAssistant::EConnectionState::Ready;
	Client.Codex.bAuthenticated = true;
	Client.Codex.bInitialized = true;
	Client.Claude.State = FAscentACFAssistant::EConnectionState::Ready;
	Client.Claude.bAuthenticated = true;
	Client.Claude.bInitialized = true;
	TestTrue(TEXT("ChatGPT remains usable alongside disabled cached gateway state"), Client.IsProviderReady(EProvider::Codex));
	TestTrue(TEXT("Claude remains usable alongside disabled cached gateway state"), Client.IsProviderReady(EProvider::ClaudeCode));
	Session->ThreadId = TEXT("thread-yarin-steer");
	Session->Provider = EProvider::Yarin;
	Session->ModelId = TEXT("yarin/yarin-fast");
	Session->bBusy = true;
	Session->bCancelRequested = true;
	Session->SteerText = TEXT("go left instead");
	TSharedPtr<FJsonObject> Completed = MakeShared<FJsonObject>();
	Completed->SetStringField(TEXT("method"), TEXT("turn/completed"));
	TSharedPtr<FJsonObject> CompletedParams = MakeShared<FJsonObject>();
	CompletedParams->SetStringField(TEXT("threadId"), TEXT("thread-yarin-steer"));
	TSharedPtr<FJsonObject> CompletedTurn = MakeShared<FJsonObject>();
	CompletedTurn->SetStringField(TEXT("status"), TEXT("interrupted"));
	CompletedParams->SetObjectField(TEXT("turn"), CompletedTurn);
	Completed->SetObjectField(TEXT("params"), CompletedParams);
	Client.Handle(Completed);
	TestTrue(TEXT("A disabled gateway cannot dispatch a queued redirect even with cached Ready state"), !Session->bBusy && Session->SteerText.IsEmpty());

	// A redirect that cannot go out is dropped with a reason; it never waits to fire on some later turn.
	Client.Yarin.State = FAscentACFAssistant::EConnectionState::SignedOut;
	Client.Yarin.bAuthenticated = false;
	Session->bBusy = true;
	Session->bCancelRequested = true;
	Session->SteerText = TEXT("stale redirect");
	Client.Handle(Completed);
	TestTrue(TEXT("A redirect for a signed-out provider is discarded"), !Session->bBusy && Session->SteerText.IsEmpty());
	TestTrue(TEXT("The user is told the redirect was not sent"), Session->Status.Contains(TEXT("not sent")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAscentACFAssistantClaudeCatalogTest, "Ascent.ACFAssistant.ClaudeCatalog",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAscentACFAssistantClaudeCatalogTest::RunTest(const FString& Parameters)
{
	const FString EffortLevels = TEXT("\"supportsEffort\":true,\"supportedEffortLevels\":[\"low\",\"medium\",\"high\",\"xhigh\",\"max\"]");
	auto Entry = [&EffortLevels](const TCHAR* Value, const TCHAR* Resolved, const TCHAR* DisplayName, bool bSupportsEffort = true)
	{
		return FString::Printf(TEXT("{\"value\":\"%s\",\"resolvedModel\":\"%s\",\"displayName\":\"%s\",%s}"),
			Value, Resolved, DisplayName, bSupportsEffort ? *EffortLevels : TEXT("\"supportsEffort\":false"));
	};
	// What a stream-json session prints: unrelated events first, then the answer to the initialize request.
	auto Output = [](const TArray<FString>& Entries)
	{
		return FString(TEXT("{\"type\":\"system\",\"subtype\":\"init\"}\n"))
			+ TEXT("{\"type\":\"control_response\",\"response\":{\"subtype\":\"success\",\"request_id\":\"yarin-models\",\"response\":{\"models\":[")
			+ FString::Join(Entries, TEXT(","))
			+ TEXT("]}}}\n");
	};

	const TArray<TSharedPtr<FACFAssistantModel>> Models = FAscentACFAssistant::ParseClaudeModels(Output({
		Entry(TEXT("default"), TEXT("claude-opus-5-5"), TEXT("Default (recommended)")),
		Entry(TEXT("opus"), TEXT("claude-opus-5-5"), TEXT("Opus 5.5")),
		Entry(TEXT("claude-fable-5-1"), TEXT("claude-fable-5-1"), TEXT("Fable 5.1")),
		Entry(TEXT("sonnet"), TEXT("claude-sonnet-5-5"), TEXT("Sonnet 5.5")),
		Entry(TEXT("haiku"), TEXT("claude-haiku-4-5-20251001"), TEXT("Haiku 4.5"), false)}));
	TestEqual(TEXT("The recommended-default entry is not listed a second time next to the model it names"), Models.Num(), 4);
	const TCHAR* const ExpectedIds[] = {TEXT("opus"), TEXT("claude-fable-5-1"), TEXT("sonnet"), TEXT("haiku")};
	const TCHAR* const ExpectedLabels[] = {TEXT("Claude Opus 5.5"), TEXT("Claude Fable 5.1"), TEXT("Claude Sonnet 5.5"), TEXT("Claude Haiku 4.5")};
	int32 DefaultCount = 0;
	FString DefaultId;
	for (int32 Index = 0; Index < Models.Num() && Index < UE_ARRAY_COUNT(ExpectedIds); ++Index)
	{
		const FString Case = FString::Printf(TEXT("Claude model %d"), Index);
		TestEqual(*(Case + TEXT(" keeps the catalog order and the id Claude Code accepts")), Models[Index]->Id, FString(ExpectedIds[Index]));
		TestEqual(*(Case + TEXT(" is labelled with the resolved version")), Models[Index]->Label, FString(ExpectedLabels[Index]));
		TestTrue(*(Case + TEXT(" runs on the Claude account")), Models[Index]->Provider == EACFAssistantProvider::ClaudeCode);
		if (Models[Index]->bDefault) { ++DefaultCount; DefaultId = Models[Index]->Id; }
	}
	TestEqual(TEXT("Exactly one model is the default"), DefaultCount, 1);
	TestEqual(TEXT("Background work defaults to Sonnet"), DefaultId, FString(TEXT("sonnet")));
	const TSharedPtr<FACFAssistantModel>* Opus = Models.FindByPredicate([](const TSharedPtr<FACFAssistantModel>& Model) { return Model->Id == TEXT("opus"); });
	const TSharedPtr<FACFAssistantModel>* Haiku = Models.FindByPredicate([](const TSharedPtr<FACFAssistantModel>& Model) { return Model->Id == TEXT("haiku"); });
	if (TestTrue(TEXT("Opus is listed"), Opus != nullptr))
	{
		TestEqual(TEXT("A model that supports effort offers its levels lowest first"), FString::Join((*Opus)->Efforts, TEXT(",")), FString(TEXT("low,medium,high,xhigh,max")));
	}
	if (TestTrue(TEXT("Haiku is listed"), Haiku != nullptr))
	{
		TestEqual(TEXT("A model without effort support offers no reasoning control"), (*Haiku)->Efforts.Num(), 0);
	}

	TestEqual(TEXT("Text that is not a catalog yields no models"), FAscentACFAssistant::ParseClaudeModels(TEXT("not json\n")).Num(), 0);
	TestEqual(TEXT("Empty output yields no models"), FAscentACFAssistant::ParseClaudeModels(FString()).Num(), 0);
	TestEqual(TEXT("A catalog holding only the recommended default lists nothing, so the built-in aliases stay"),
		FAscentACFAssistant::ParseClaudeModels(Output({Entry(TEXT("default"), TEXT("claude-opus-5-5"), TEXT("Default (recommended)"))})).Num(), 0);

	const TArray<TSharedPtr<FACFAssistantModel>> Aliased = FAscentACFAssistant::ParseClaudeModels(Output({
		Entry(TEXT("sonnet"), TEXT("claude-sonnet-5-5"), TEXT("Sonnet 5.5")),
		Entry(TEXT("claude-sonnet-5-5"), TEXT("claude-sonnet-5-5"), TEXT("Sonnet 5.5"))}));
	TestEqual(TEXT("An alias and its full model id are one model"), Aliased.Num(), 1);
	if (Aliased.Num() == 1)
	{
		TestEqual(TEXT("The first entry for a model is the one kept"), Aliased[0]->Id, FString(TEXT("sonnet")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAscentACFAssistantClaudeContextUsageTest, "Ascent.ACFAssistant.ClaudeContextUsage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAscentACFAssistantClaudeContextUsageTest::RunTest(const FString& Parameters)
{
	auto Line = [](const FString& Json)
	{
		TSharedPtr<FJsonObject> Object;
		FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Object);
		return Object;
	};
	FString StreamError;
	auto Feed = [&](FACFAssistantSession& Session, const FString& Json)
	{
		return FAscentACFAssistant::HandleClaudeStreamLine(Session, Line(Json), StreamError);
	};
	// One Claude API call as the stream reports it: the cached prompt sits in the cache_* fields, so input_tokens alone is tiny.
	auto Call = [](int32 Input, int32 CacheRead, int32 CacheCreation, int32 Output)
	{
		return FString::Printf(TEXT("{\"type\":\"assistant\",\"message\":{\"id\":\"msg\",\"usage\":{\"input_tokens\":%d,\"cache_read_input_tokens\":%d,")
			TEXT("\"cache_creation_input_tokens\":%d,\"output_tokens\":%d}}}"), Input, CacheRead, CacheCreation, Output);
	};
	// The turn's result sums every call of the turn; modelUsage names each model it touched. The Sonnet usage and the result's usage are captured from a real two-call turn; Haiku and Opus are constructed.
	const FString SonnetUsage = TEXT("\"claude-sonnet-5-5\":{\"inputTokens\":4,\"outputTokens\":91,\"cacheReadInputTokens\":41512,\"cacheCreationInputTokens\":10507,\"contextWindow\":1000000}");
	const FString HaikuUsage = TEXT("\"claude-haiku-4-5\":{\"inputTokens\":50,\"outputTokens\":8,\"cacheReadInputTokens\":0,\"cacheCreationInputTokens\":0,\"contextWindow\":200000}");
	const FString NoWindowUsage = TEXT("\"claude-opus-5-5\":{\"inputTokens\":9000,\"outputTokens\":40,\"cacheReadInputTokens\":0,\"cacheCreationInputTokens\":0}");
	auto Result = [](const FString& ModelUsage)
	{
		return FString(TEXT("{\"type\":\"result\",\"subtype\":\"success\",\"is_error\":false,"))
			+ TEXT("\"usage\":{\"input_tokens\":4,\"cache_read_input_tokens\":41512,\"cache_creation_input_tokens\":10507,\"output_tokens\":91}")
			+ (ModelUsage.IsEmpty() ? FString() : FString(TEXT(",\"modelUsage\":{")) + ModelUsage + TEXT("}")) + TEXT("}");
	};
	const int64 FirstCallContext = 2 + 15782 + 9948 + 16;
	const int64 LastCallContext = 2 + 25730 + 559 + 3;

	FACFAssistantSession Session;
	Feed(Session, Call(2, 15782, 9948, 16));
	TestEqual(TEXT("A call's context is its fresh input plus the cached prompt it read or wrote plus its answer"), Session.UsedContextTokens, FirstCallContext);
	Feed(Session, Call(2, 25730, 559, 3));
	TestEqual(TEXT("The last call of the turn is the context size, not the first call and not a sum of calls"), Session.UsedContextTokens, LastCallContext);

	TestTrue(TEXT("The result finishes the turn"), Feed(Session, Result(SonnetUsage)));
	TestEqual(TEXT("The result's sum over every call never replaces the last call's context"), Session.UsedContextTokens, LastCallContext);
	TestEqual(TEXT("The result reports the window of the model that carried the context"), Session.ReportedContextWindowTokens, int64(1000000));

	// A result may name several models (a helper model beside the conversation's own); the one that carried the context supplies the window.
	struct FWindowCase { const TCHAR* Name; FString ModelUsage; int64 ExpectedWindow; };
	const FWindowCase WindowCases[] = {
		{TEXT("one model"), SonnetUsage, 1000000},
		{TEXT("the small model listed before the one that carried the context"), HaikuUsage + TEXT(",") + SonnetUsage, 1000000},
		{TEXT("the small model listed after the one that carried the context"), SonnetUsage + TEXT(",") + HaikuUsage, 1000000},
		{TEXT("the heaviest model reporting no window"), NoWindowUsage + TEXT(",") + HaikuUsage, 200000},
	};
	for (const FWindowCase& Case : WindowCases)
	{
		FACFAssistantSession Fresh;
		Feed(Fresh, Result(Case.ModelUsage));
		TestEqual(*FString::Printf(TEXT("Reported window with %s"), Case.Name), Fresh.ReportedContextWindowTokens, Case.ExpectedWindow);
	}

	// Events that carry no figure never erase what the last call reported.
	Feed(Session, TEXT("{\"type\":\"assistant\",\"message\":{\"id\":\"msg\",\"content\":[]}}"));
	TestEqual(TEXT("An assistant event without usage keeps the context"), Session.UsedContextTokens, LastCallContext);
	Feed(Session, TEXT("{\"type\":\"assistant\",\"message\":{\"id\":\"msg\",\"usage\":{}}}"));
	TestEqual(TEXT("An assistant event with an empty usage keeps the context"), Session.UsedContextTokens, LastCallContext);
	TestTrue(TEXT("A result without modelUsage still finishes the turn"), Feed(Session, Result(FString())));
	TestEqual(TEXT("A result without modelUsage keeps the reported window"), Session.ReportedContextWindowTokens, int64(1000000));
	Feed(Session, Result(TEXT("\"claude-sonnet-5-5\":{\"inputTokens\":4,\"cacheReadInputTokens\":41512,\"cacheCreationInputTokens\":10507}")));
	TestEqual(TEXT("A modelUsage entry without a contextWindow keeps the reported window"), Session.ReportedContextWindowTokens, int64(1000000));
	TestEqual(TEXT("Results never touch the context size"), Session.UsedContextTokens, LastCallContext);

	// The context shrinks when Claude compacts the conversation, so the newest call wins even when it is smaller.
	Feed(Session, Call(3, 8000, 1000, 10));
	TestEqual(TEXT("A smaller call after compaction replaces the larger one"), Session.UsedContextTokens, int64(3 + 8000 + 1000 + 10));

	// What the panel's ring shows: used tokens over the window, where the window is whatever the conversation learned, else the model's own.
	const FString Catalog = FString(TEXT("{\"type\":\"control_response\",\"response\":{\"subtype\":\"success\",\"request_id\":\"yarin-models\",\"response\":{\"models\":["))
		+ TEXT("{\"value\":\"sonnet\",\"resolvedModel\":\"claude-sonnet-5-5\",\"displayName\":\"Sonnet 5.5\"},")
		+ TEXT("{\"value\":\"sonnet[1m]\",\"resolvedModel\":\"claude-sonnet-5-5[1m]\",\"displayName\":\"Sonnet 5.5 (1M context)\"}")
		+ TEXT("]}}}\n");
	FAscentACFAssistant Client;
	Client.Models = FAscentACFAssistant::ParseClaudeModels(Catalog);
	const TSharedPtr<FACFAssistantSession> Active = Client.GetActiveSession();
	if (!TestTrue(TEXT("A new client opens a conversation"), Active.IsValid())) { return false; }
	Active->ModelId = TEXT("sonnet");
	Feed(*Active, Call(2, 15782, 9948, 16));
	Feed(*Active, Call(2, 25730, 559, 3));
	TestEqual(TEXT("The client's conversation holds the last call's context"), Active->UsedContextTokens, LastCallContext);

	const double Unknown = Client.GetContextUsageFraction();
	TestTrue(*FString::Printf(TEXT("A window nobody has reported reads 0, not a guess (got %f)"), Unknown), Unknown == 0.0);
	Active->ModelId = TEXT("sonnet[1m]");
	const double OfCatalogWindow = Client.GetContextUsageFraction();
	TestTrue(*FString::Printf(TEXT("A model that names its 1M window gives 26294 / 1000000 (got %f)"), OfCatalogWindow), FMath::Abs(OfCatalogWindow - 0.026294) < 1e-6);
	Feed(*Active, Result(SonnetUsage));
	Active->ModelId = TEXT("sonnet");
	const double OfReportedWindow = Client.GetContextUsageFraction();
	TestTrue(*FString::Printf(TEXT("A window the result reported beats a model with no window (got %f)"), OfReportedWindow), FMath::Abs(OfReportedWindow - 0.026294) < 1e-6);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAscentACFAssistantConversationGateTest, "Ascent.ACFAssistant.ConversationGate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAscentACFAssistantConversationGateTest::RunTest(const FString& Parameters)
{
	FAscentACFAssistant Client;
	const TSharedPtr<FACFAssistantSession> Editing = Client.GetActiveSession();
	if (!TestTrue(TEXT("A new client opens a conversation"), Editing.IsValid())) { return false; }
	Editing->Provider = EACFAssistantProvider::ClaudeCode;
	Editing->Permissions.WorkMode = EACFAssistantWorkMode::AskForApproval;
	Editing->bBusy = true;
	const TSharedPtr<FACFAssistantSession> Planning = Client.OpenSession();
	Planning->Provider = EACFAssistantProvider::ClaudeCode;
	Planning->Permissions.WorkMode = EACFAssistantWorkMode::Plan;
	Planning->bBusy = true;
	const TSharedPtr<FACFAssistantSession> Approving = Client.OpenSession();
	Approving->Provider = EACFAssistantProvider::ClaudeCode;
	Approving->Permissions.WorkMode = EACFAssistantWorkMode::ApproveForMe;
	Approving->bBusy = true;
	const TSharedPtr<FACFAssistantSession> Codex = Client.OpenSession();
	Codex->Provider = EACFAssistantProvider::Codex;
	Codex->Permissions.WorkMode = EACFAssistantWorkMode::AskForApproval;
	Codex->bBusy = true;

	auto Create = MakeShared<FJsonObject>();
	Create->SetStringField(TEXT("toolset_name"), TEXT("AscentAIToolsEditor_ACFItemToolset"));
	Create->SetStringField(TEXT("tool_name"), TEXT("SaveItem"));

	TSet<FString> Tokens;
	for (const TSharedPtr<FACFAssistantSession>& Each : {Editing, Planning, Approving, Codex})
	{
		TestFalse(TEXT("Every conversation has a gate token"), Each->GateToken.IsEmpty());
		Tokens.Add(Each->GateToken);
	}
	TestEqual(TEXT("No two conversations share a gate token"), Tokens.Num(), 4);
	TestTrue(TEXT("A conversation's gate URL carries its token"),
		Client.GateUrlFor(Editing).EndsWith(FString::Printf(TEXT("?%s=%s"), ACFAssistantToolGate::ConversationQueryKey, *Editing->GateToken)));
	TestNotEqual(TEXT("Two conversations never share a gate URL"), Client.GateUrlFor(Editing), Client.GateUrlFor(Planning));

	const FACFAssistantToolVerdict ForEditing = Client.JudgeEditorTool(TEXT("call_tool"), Create, Editing->GateToken);
	TestTrue(TEXT("Ask for approval is held for its own banner while a Plan conversation is answering at the same time"),
		ForEditing.Verdict == EACFAssistantToolVerdict::Ask && ForEditing.Session == Editing);
	const FACFAssistantToolVerdict ForPlanning = Client.JudgeEditorTool(TEXT("call_tool"), Create, Planning->GateToken);
	TestTrue(TEXT("The Plan conversation is blocked by its own mode"),
		ForPlanning.Verdict == EACFAssistantToolVerdict::Block && ForPlanning.Session == Planning);
	const FACFAssistantToolVerdict ForApproving = Client.JudgeEditorTool(TEXT("call_tool"), Create, Approving->GateToken);
	TestTrue(TEXT("Approve for me runs while a Plan conversation is answering at the same time"),
		ForApproving.Verdict == EACFAssistantToolVerdict::Allow && ForApproving.Session == Approving);
	TestTrue(TEXT("The Claude permission prompt is judged by its own conversation too"),
		Client.JudgeMutation(Editing->GateToken).Verdict == EACFAssistantToolVerdict::Ask);
	TestTrue(TEXT("A call from a conversation that is no longer open is refused"),
		Client.JudgeEditorTool(TEXT("call_tool"), Create, TEXT("stale-token")).Verdict == EACFAssistantToolVerdict::Block);

	const FACFAssistantToolVerdict Untagged = Client.JudgeEditorTool(TEXT("call_tool"), Create);
	TestTrue(TEXT("A call with no conversation is judged by the Codex conversations, never by the Claude ones"),
		Untagged.Verdict == EACFAssistantToolVerdict::Ask && Untagged.Session == Codex);
	const TSharedPtr<FACFAssistantSession> CodexPlanning = Client.OpenSession();
	CodexPlanning->Provider = EACFAssistantProvider::Codex;
	CodexPlanning->Permissions.WorkMode = EACFAssistantWorkMode::Plan;
	CodexPlanning->bBusy = true;
	TestTrue(TEXT("Two Codex conversations share one runtime, so the stricter mode applies to an untagged call"),
		Client.JudgeEditorTool(TEXT("call_tool"), Create).Verdict == EACFAssistantToolVerdict::Block);
	CodexPlanning->bBusy = false;

	Client.ActivateSession(Planning->LocalId);
	const FACFAssistantToolVerdict WhilePlanningIsOnScreen = Client.JudgeEditorTool(TEXT("call_tool"), Create, Editing->GateToken);
	TestTrue(TEXT("Which conversation is on screen does not change who owns a call"),
		WhilePlanningIsOnScreen.Verdict == EACFAssistantToolVerdict::Ask && WhilePlanningIsOnScreen.Session == Editing);
	int32 Decisions = 0;
	Client.RequestApproval(WhilePlanningIsOnScreen.Session, TEXT("editor-tool"), TEXT("SaveItem"), nullptr, [&Decisions](bool) { ++Decisions; });
	TestEqual(TEXT("The banner lands on the conversation that asked"), Editing->Approvals.Num(), 1);
	TestEqual(TEXT("The conversation on screen is not asked"), Planning->Approvals.Num(), 0);
	Client.DenyApprovals(Editing, nullptr);
	TestEqual(TEXT("A denied banner answers the call once"), Decisions, 1);

	Planning->bBusy = false;
	TestTrue(TEXT("An idle Plan conversation still blocks its own late call"),
		Client.JudgeEditorTool(TEXT("call_tool"), Create, Planning->GateToken).Verdict == EACFAssistantToolVerdict::Block);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAscentACFAssistantOwnPluginTest, "Ascent.ACFAssistant.OwnPluginResources",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAscentACFAssistantOwnPluginTest::RunTest(const FString& Parameters)
{
	// ACF Ultimate ships this module as AscentCombatFramework, the standalone package as Yarin. The panel's images, the phone page's
	// scripts, the instructions file and the bundled skills all hang off the plugin that owns the module, so it must resolve under either name.
	const TSharedPtr<IPlugin> Owner = ACFAssistantPlugin::Owner();
	if (!TestTrue(TEXT("The module finds the plugin that ships it"), Owner.IsValid())) { return false; }

	const TArray<FString> Resources = {
		TEXT("Resources/YarinLong.png"), TEXT("Resources/YarinIcon.png"), TEXT("Resources/Sidebar.svg"),
		TEXT("Resources/ACFAssistantInstructions.txt"), TEXT("Resources/Phone/marked.min.js"), TEXT("Resources/Phone/markdown.js") };
	for (const FString& Resource : Resources)
	{
		TestTrue(FString::Printf(TEXT("%s is reachable from the owning plugin"), *Resource), FPaths::FileExists(Owner->GetBaseDir() / Resource));
	}

	// The standalone is for people without ACF: it must ship at least one skill, and the instructions every install shares must not
	// claim ACF knowledge or tools (installs that have ACF add that in Resources/AscentExpertiseInstructions.txt, which the standalone omits).
	TArray<FString> Skills;
	IFileManager::Get().FindFiles(Skills, *(Owner->GetBaseDir() / TEXT("Resources/Skills/*")), false, true);
	int32 SkillsWithManifest = 0;
	for (const FString& Skill : Skills)
	{
		SkillsWithManifest += FPaths::FileExists(Owner->GetBaseDir() / TEXT("Resources/Skills") / Skill / TEXT("SKILL.md")) ? 1 : 0;
	}
	TestTrue(TEXT("At least one skill ships with a SKILL.md"), SkillsWithManifest > 0);

	FString Shared;
	FFileHelper::LoadFileToString(Shared, *(Owner->GetBaseDir() / TEXT("Resources/ACFAssistantInstructions.txt")));
	TestFalse(TEXT("The shared instructions do not claim Ascent Combat Framework expertise"), Shared.Contains(TEXT("Combat Framework")));
	TestFalse(TEXT("The shared instructions do not name ACF"), Shared.Contains(TEXT("ACF")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAscentACFAssistantMeshResponseTest, "Ascent.ACFAssistant.MeshCardResponse",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAscentACFAssistantMeshResponseTest::RunTest(const FString& Parameters)
{
	using namespace ACFAssistantMesh;
	TestTrue(TEXT("The text tool is a mesh tool"), IsMeshGenerationTool(TEXT("GenerateMeshFromText")));
	TestTrue(TEXT("The image tool is a mesh tool, whatever its case"), IsMeshGenerationTool(TEXT("generatemeshfromimage")));
	TestFalse(TEXT("Other generation tools get no mesh card"), IsMeshGenerationTool(TEXT("GenerateImage")));
	TestFalse(TEXT("An empty tool name gets no mesh card"), IsMeshGenerationTool(FString()));

	// The answer of tools/call as the gate sees it: a JSON-RPC object holding the tool's own JSON as a text block.
	auto Rpc = [](const FString& ToolText, bool bIsError)
	{
		TSharedRef<FJsonObject> Content = MakeShared<FJsonObject>();
		Content->SetStringField(TEXT("type"), TEXT("text"));
		Content->SetStringField(TEXT("text"), ToolText);
		TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
		Result->SetArrayField(TEXT("content"), {MakeShared<FJsonValueObject>(Content)});
		Result->SetBoolField(TEXT("isError"), bIsError);
		TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetStringField(TEXT("jsonrpc"), TEXT("2.0"));
		Root->SetNumberField(TEXT("id"), 1);
		Root->SetObjectField(TEXT("result"), Result);
		return ACFAssistantTools::Encode(Root);
	};
	const FString Path = TEXT("/Game/Yarin/Chest/SM_Chest.SM_Chest");
	const FString Done = FString::Printf(TEXT("{\"success\":true,\"asset_path\":\"%s\"}"), *Path);

	FToolResult Plain = ParseToolResponse(Rpc(Done, false));
	TestTrue(TEXT("A finished mesh is a success"), Plain.bOk);
	TestEqual(TEXT("A finished mesh carries its object path"), Plain.AssetPath, Path);

	FToolResult Streamed = ParseToolResponse(TEXT("event: message\ndata: ") + Rpc(Done, false) + TEXT("\n\n"));
	TestTrue(TEXT("A server sent event body is read like plain JSON"), Streamed.bOk && Streamed.AssetPath == Path);

	// Epic's tool host wraps the tool's JSON as { "returnValue": "<json string>" }. This is the shape a live run produced.
	auto Wrap = [](const FString& Inner)
	{
		TSharedRef<FJsonObject> Wrapper = MakeShared<FJsonObject>();
		Wrapper->SetStringField(TEXT("returnValue"), Inner);
		return ACFAssistantTools::Encode(Wrapper);
	};
	FToolResult Wrapped = ParseToolResponse(Rpc(Wrap(Done), false));
	TestTrue(TEXT("A result inside Epic's returnValue wrapper is a success"), Wrapped.bOk && Wrapped.AssetPath == Path);
	FToolResult WrappedFailure = ParseToolResponse(Rpc(Wrap(TEXT("{\"success\":false,\"error\":\"No credit left\"}")), false));
	TestFalse(TEXT("A wrapped success false is a failure"), WrappedFailure.bOk);
	TestEqual(TEXT("A wrapped failure keeps the tool's error"), WrappedFailure.Error, FString(TEXT("No credit left")));
	FToolResult WrappedText = ParseToolResponse(Rpc(Wrap(TEXT("The Tripo API key is missing.")), false));
	TestFalse(TEXT("A wrapped plain message is a failure"), WrappedText.bOk);
	TestTrue(TEXT("A wrapped plain message reaches the card"), WrappedText.Error.Contains(TEXT("Tripo API key is missing")));

	FToolResult ToolFailure = ParseToolResponse(Rpc(TEXT("The Tripo API key is missing."), true));
	TestFalse(TEXT("A tool error is a failure"), ToolFailure.bOk);
	TestTrue(TEXT("A tool error keeps its message for the card"), ToolFailure.Error.Contains(TEXT("Tripo API key is missing")));

	FToolResult SoftFailure = ParseToolResponse(Rpc(TEXT("{\"success\":false,\"error\":\"No credit left\"}"), false));
	TestFalse(TEXT("success false is a failure even without isError"), SoftFailure.bOk);
	TestEqual(TEXT("success false keeps the tool's error"), SoftFailure.Error, FString(TEXT("No credit left")));

	TestFalse(TEXT("A success without an asset path cannot show a mesh"), ParseToolResponse(Rpc(TEXT("{\"success\":true}"), false)).bOk);
	TestFalse(TEXT("A plain text answer is not a mesh"), ParseToolResponse(Rpc(TEXT("done"), false)).bOk);

	FToolResult RpcError = ParseToolResponse(TEXT("{\"jsonrpc\":\"2.0\",\"id\":1,\"error\":{\"code\":-32000,\"message\":\"The Editor tool server did not answer.\"}}"));
	TestFalse(TEXT("A JSON-RPC error is a failure"), RpcError.bOk);
	TestTrue(TEXT("A JSON-RPC error keeps its message"), RpcError.Error.Contains(TEXT("did not answer")));

	FToolResult Nothing = ParseToolResponse(FString());
	TestFalse(TEXT("No answer at all is a failure"), Nothing.bOk);
	TestFalse(TEXT("No answer at all still explains itself"), Nothing.Error.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAscentACFAssistantMeshCardTest, "Ascent.ACFAssistant.MeshCardLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAscentACFAssistantMeshCardTest::RunTest(const FString& Parameters)
{
	using namespace ACFAssistantMesh;
	FAscentACFAssistant Client;
	const TSharedPtr<FACFAssistantSession> Session = Client.GetActiveSession();
	if (!TestTrue(TEXT("A new client has a conversation"), Session.IsValid())) { return false; }

	const int32 Base = Session->Messages.Num();
	const int32 First = Client.BeginMeshCard(Session);
	TestEqual(TEXT("The card is the newest row"), First, Base);
	TestEqual(TEXT("A new card is pending"), Session->Messages[First].Role, FString(RolePending));

	Client.FinishMeshCard(Session, First, true, TEXT("/Game/Yarin/Chest/SM_Chest.SM_Chest"));
	TestEqual(TEXT("A finished card shows the mesh"), Session->Messages[First].Role, FString(RoleReady));
	TestEqual(TEXT("A finished card keeps the object path"), Session->Messages[First].Text, FString(TEXT("/Game/Yarin/Chest/SM_Chest.SM_Chest")));

	Client.FinishMeshCard(Session, First, false, TEXT("a late failure"));
	TestEqual(TEXT("A finished card cannot be rewritten by a late result"), Session->Messages[First].Role, FString(RoleReady));

	const int32 Second = Client.BeginMeshCard(Session);
	Client.FinishMeshCard(Session, Second, false, TEXT("No credit left"));
	TestEqual(TEXT("A failed card shows the failure"), Session->Messages[Second].Role, FString(RoleFailed));
	TestEqual(TEXT("A failed card keeps the reason"), Session->Messages[Second].Text, FString(TEXT("No credit left")));
	TestEqual(TEXT("Each generation gets its own card"), Second, First + 1);

	const int32 Count = Session->Messages.Num();
	Client.FinishMeshCard(Session, 9999, true, TEXT("/Game/X.X"));
	Client.FinishMeshCard(nullptr, 0, true, TEXT("/Game/X.X"));
	TestEqual(TEXT("A stale index or a missing conversation changes nothing"), Session->Messages.Num(), Count);
	TestEqual(TEXT("A missing conversation cannot start a card"), Client.BeginMeshCard(nullptr), static_cast<int32>(INDEX_NONE));

	TestTrue(TEXT("Every mesh role is recognised"), IsMeshRole(RolePending) && IsMeshRole(RoleReady) && IsMeshRole(RoleFailed));
	TestFalse(TEXT("Chat and image rows are not mesh rows"), IsMeshRole(TEXT("Assistant")) || IsMeshRole(TEXT("Image")));
	TestTrue(TEXT("The phone gets a readable line for a finished mesh"), DescribeForPhone(RoleReady, TEXT("/Game/Yarin/Chest/SM_Chest.SM_Chest")).Contains(TEXT("SM_Chest")));
	TestTrue(TEXT("The phone gets the reason for a failure"), DescribeForPhone(RoleFailed, TEXT("No credit left")).Contains(TEXT("No credit left")));
	return true;
}
