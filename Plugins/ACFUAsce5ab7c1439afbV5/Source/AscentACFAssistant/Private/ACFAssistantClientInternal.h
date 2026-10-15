// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ACFAssistantCompletion.h"
#include "ACFAssistantSettings.h"
#include "Containers/Ticker.h"
#include "Async/Future.h"
#include "Containers/Queue.h"
#include "HAL/ThreadSafeBool.h"
#include "Dom/JsonObject.h"
#include <atomic>

struct FACFAssistantMessage
{
	FString Role;
	FString Text;
};

// One clickable answer Codex offers through item/tool/requestUserInput.
struct FACFAssistantChoice
{
	FString Label;
	FString Description;
};

// A clarifying question whose options are rendered as buttons under the composer.
struct FACFAssistantQuestion
{
	FString Id;
	FString Header;
	FString Question;
	TArray<FACFAssistantChoice> Options;
};

struct FACFAssistantModel
{
	FString Id;
	FString Label;
	// The model's own default reasoning level; empty lets the provider decide.
	FString Effort;
	// Reasoning levels the model accepts, lowest first. Empty means it has no reasoning control.
	TArray<FString> Efforts;
	// Context window size in tokens as reported by the model catalog; 0 when unknown.
	int64 ContextWindowTokens = 0;
	bool bDefault = false;
	// All providers stay connected at once and share one model list; picking a model picks the provider that runs it.
	EACFAssistantProvider Provider = EACFAssistantProvider::Codex;
	// What the Codex runtime is told to run. Empty means Id. Yarin models are listed as "yarin/<gateway id>" so they can never collide with a ChatGPT
	// model id and silently change who pays; the runtime itself receives the bare gateway id.
	FString ApiId;
};

struct FACFAssistantHistory
{
	FString Id;
	FString Title;
	// A saved conversation can only continue on the provider whose thread store holds it.
	EACFAssistantProvider Provider = EACFAssistantProvider::Codex;
	// Unix seconds of the last message, so both providers' conversations share one most-recent-first list.
	int64 Updated = 0;
};

/** What the work-mode gate decided for one Editor tool call arriving on Yarin's MCP endpoint. */
enum class EACFAssistantToolVerdict : uint8
{
	// Read-only tool, or an editing mode that does not ask: run it.
	Allow,
	// Ask or Plan: the call is rejected before it reaches the Editor.
	Block,
	// Ask for approval: hold the call until the user decides.
	Ask
};

struct FACFAssistantToolVerdict
{
	EACFAssistantToolVerdict Verdict = EACFAssistantToolVerdict::Block;
	// Plain-language reason returned to the model when the call is blocked or denied.
	FString Reason;
	// Conversation the decision was made for. The one that asks when Verdict is Ask.
	TSharedPtr<struct FACFAssistantSession> Session;
	// Ask even in Approve for me or after Approve all (Live Coding compiles).
	bool bAlwaysAsk = false;
};

/**
 * One change waiting for the user's decision: an Editor tool call held by the gate, a Codex command or file change, or a Claude tool
 * permission prompt. Approve runs it, Deny returns a refusal to the model, Approve all also stops the conversation from asking again.
 */
struct FACFAssistantApproval
{
	int32 Id = 0;
	// "editor-tool", "command", "file-change", "permissions", or "claude-tool".
	FString Kind;
	// Short label shown on the banner and the phone, e.g. "AscentDataToolset.CreateItem" or "apply_patch".
	FString Title;
	// Details for the UI: tool, kind, arguments or command text.
	TSharedPtr<FJsonObject> Preview;
	double Requested = 0.0;
	// Requires an individual decision; Approve all must not approve unseen mandatory prompts.
	bool bAlwaysAsk = false;
	// Called exactly once, on the game thread, with the user's decision.
	TFunction<void(bool bApproved)> Decide;
};

struct FACFAssistantPermissions
{
	EACFAssistantWorkMode WorkMode = EACFAssistantWorkMode::Plan;
	// Set by Approve all: every later change in this conversation runs without asking, until the work mode changes.
	bool bConversationApproved = false;

	bool AsksForApproval() const
	{
		return WorkMode == EACFAssistantWorkMode::AskForApproval && !bConversationApproved;
	}

	bool BlocksMutations() const
	{
		return WorkMode == EACFAssistantWorkMode::Ask || WorkMode == EACFAssistantWorkMode::Plan;
	}

	bool AutoApproves() const
	{
		return WorkMode == EACFAssistantWorkMode::ApproveForMe;
	}

	bool CanWrite() const
	{
		return !BlocksMutations() && (AutoApproves() || bConversationApproved);
	}

	static FString Label(EACFAssistantWorkMode Mode)
	{
		switch (Mode)
		{
		case EACFAssistantWorkMode::Ask: return TEXT("Ask");
		case EACFAssistantWorkMode::Plan: return TEXT("Plan");
		case EACFAssistantWorkMode::AskForApproval: return TEXT("Ask for approval");
		case EACFAssistantWorkMode::ApproveForMe: return TEXT("Approve for me");
		default: return TEXT("Plan");
		}
	}
};

/**
 * One open conversation. Every session runs on the shared provider process as its own Codex thread, so a session can keep
 * answering and calling Editor tools while the user reads, writes in, or opens another one.
 */
struct FACFAssistantSession
{
	// The panel needs a stable handle from the moment a conversation is opened; ThreadId only exists after the first message.
	int32 LocalId = 0;
	FString GateToken;
	FString ThreadId;
	FString TurnId;
	TArray<FACFAssistantMessage> Messages;
	TArray<FString> PendingImages;
	FString PendingText;
	// Steering: a message sent while this conversation runs interrupts its turn and is dispatched as the next one, keeping the thread context.
	FString SteerText;
	FString Status;
	FString LastAssistantItem;
	FString LastReasoningItem;
	// Separate short-lived thread used only to generate a concise title for this conversation after its first answer.
	FString PendingTitleSource;
	FString TitleThreadId;
	FString TitleTurnId;
	FString TitleText;
	int32 TitleGeneration = 0;
	FACFAssistantPermissions Permissions;
	// Model picked for this conversation. Empty or no longer offered falls back to the client default.
	FString ModelId;
	// Provider that owns this conversation's thread. Fixed by the first message; before it, the picked model decides.
	EACFAssistantProvider Provider = EACFAssistantProvider::Codex;
	// Changes waiting for the user, oldest first. The banner and the phone show the first one.
	TArray<TSharedPtr<FACFAssistantApproval>> Approvals;
	// Codex blocks the turn on item/tool/requestUserInput until the client replies. The id is the JSON-RPC id to answer with.
	TSharedPtr<FJsonValue> UserInputRequestId;
	TArray<FACFAssistantQuestion> UserInputQuestions;
	// Question id -> chosen label. The reply is sent only once every question has an answer.
	TMap<FString, FString> UserInputAnswers;
	// Shared with the History list so a generated or renamed title reaches the tab without a second lookup.
	TSharedPtr<FACFAssistantHistory> HistoryEntry;
	bool bBusy = false;
	bool bCancelRequested = false;
	bool bNeedsThreadResume = false;
	bool bApprovalDeniedThisTurn = false;
	// Numbers the steps in the status line. With work rows hidden, that line is the only sign Yarin is alive.
	int32 WorkStepCount = 0;
	// Token usage reported by the server for this thread; the window falls back to the selected model's catalog value.
	int64 UsedContextTokens = 0;
	int64 ReportedContextWindowTokens = 0;
	// Claude Code runs one CLI process per turn instead of a long-lived app-server. Empty when this conversation is idle.
	void* ClaudeProcess = nullptr;
	void* ClaudeProcessThread = nullptr;
	void* ClaudeOutputRead = nullptr;
	void* ClaudeJob = nullptr;
	TArray<uint8> ClaudeIncoming;
	bool bClaudeGotResult = false;
	bool bClaudeHasStarted = false;
	// Standing rules live in the system prompt. A turn carries a note only when this conversation has not been told, or the mode changed.
	bool bContextAnnounced = false;
	EACFAssistantWorkMode AnnouncedWorkMode = EACFAssistantWorkMode::Plan;

	FString GetTitle() const
	{
		return HistoryEntry.IsValid() && !HistoryEntry->Title.IsEmpty() ? HistoryEntry->Title : TEXT("(new conversation)");
	}
};

// One `claude auth login` console, owned by the thread that waits for it. Defined in ACFAssistantTransport.cpp.
struct FClaudeLoginProcess;

/** Isolated Codex / Claude Code process shared by every conversation open in the ACF Assistant panel. */
class FAscentACFAssistant : public TSharedFromThis<FAscentACFAssistant>
{
public:
	FAscentACFAssistant();
	~FAscentACFAssistant();

	/**
	 * Where one provider's account stands. Codex and Claude Code each have their own and stay connected side by side.
	 * There is exactly one account action per state: Disconnected / SignedOut / Failed -> SignIn, SigningIn -> CancelSignIn, Ready -> SignOut.
	 */
	enum class EConnectionState : uint8 { Disconnected, Connecting, SignedOut, SigningIn, Ready, Failed };
	// What `claude auth status` says about who would pay for a turn.
	// ApiBilling covers an API key, a Console login and a cloud provider: usable, billed to that account instead of a Claude plan.
	enum class EClaudeAuth : uint8 { Unreadable, SignedOut, Subscription, ApiBilling };

	// How many conversations may stay open at once. Each one holds a live Codex thread, so the panel refuses to stack them without limit.
	static constexpr int32 MaxOpenSessions = 8;

	// Silent check of every provider on panel open and phone access: Ready when already signed in, SignedOut otherwise. Never opens a browser.
	void AutoConnect();
	// The one account action per provider. Starts it when needed and opens its browser sign-in only when the account is signed out.
	void SignIn(EACFAssistantProvider InProvider);
	void CancelSignIn(EACFAssistantProvider InProvider);
	// Codex: account/logout inside Yarin's private CODEX_HOME. Claude: `claude auth logout`, which signs Claude Code out on this computer.
	void SignOut(EACFAssistantProvider InProvider);
	void Disconnect();
	// Stops every provider process while the engine is still alive. After engine exit the writer thread is already gone and
	// the log is torn down, so the destructor only does this if nothing did it earlier.
	void Shutdown();
	void Send(const FString& Text);
	void Cancel();
	// Hard stop from the Stop button: unlike steering, it also discards any queued redirection.
	void StopTurn();
	void NewConversation();
	void Resume(const FString& Id);
	void SavePreferences();
	// LocalId 0 targets the conversation on screen. The phone passes the one it shows, which may differ once the Editor switched tabs.
	// A model of the other provider cannot continue a started conversation, so it opens a new one instead.
	void SelectModel(const FString& Id, int32 LocalId = 0);
	void SetWorkMode(EACFAssistantWorkMode InMode, int32 LocalId = 0);
	// Decides the oldest pending change of the conversation on screen.
	void ResolveApproval(bool bApproved);
	// Approve all: runs every pending change of the conversation on screen and stops it from asking until the work mode changes.
	void ApproveConversation();
	int32 GetPendingApprovalCount() const;
	FACFAssistantToolVerdict JudgeEditorTool(const FString& McpToolName, const TSharedPtr<FJsonObject>& McpArguments, const FString& ConversationToken = FString()) const;
	/**
	 * Queues a change for the user. Decide runs exactly once on the game thread: from Approve / Deny / Approve all, or with false when the
	 * turn stops, the conversation closes, or the client disconnects.
	 */
	TSharedPtr<FACFAssistantApproval> RequestApproval(const TSharedPtr<FACFAssistantSession>& Session, const FString& Kind, const FString& Title,
		const TSharedPtr<FJsonObject>& Preview, TFunction<void(bool bApproved)> Decide, bool bAlwaysAsk = false);
	/**
	 * Adds a progress card for a 3D generation that just started in this conversation and returns its message index, or INDEX_NONE when the
	 * conversation is gone. The tool gate calls this before it forwards a mesh tool call, so it works for every provider.
	 */
	int32 BeginMeshCard(const TSharedPtr<FACFAssistantSession>& Session);
	/** Turns the card at Index into the finished mesh (bOk, Detail is its object path) or into the failure (Detail is the reason). A stale index or a card that already finished is ignored. */
	void FinishMeshCard(const TSharedPtr<FACFAssistantSession>& Session, int32 Index, bool bOk, const FString& Detail);
	// Clickable choices for the conversation on screen. Empty when Codex is not waiting on a question.
	bool HasSuggestedReplies() const;
	const TArray<FACFAssistantQuestion>& GetSuggestedQuestions() const;
	bool IsSuggestedAnswered(const FString& QuestionId) const;
	// Records one clicked or typed answer. QuestionId may be empty; then the first unanswered question is used.
	void AnswerSuggestedReply(const FString& QuestionId, const FString& Label);
	void ReloadExecutablesFromSettings();
	void AttachImage(const FString& Path);
	void RemoveImage(int32 Index);
	// Codex localImage and its sandbox both require a real folder on disk. Pasted screenshots are written here, inside the isolated working folder.
	FString GetPastedImagesDirectory() const;
	// Syncs the plugin's bundled ACF skills into a provider skills folder. Runs on every Connect for both providers; the plugin-version marker makes repeat calls a no-op.
	void SyncSkills(const FString& DestinationSkillsDirectory);

	// Open conversations, in tab order. Switching between them never interrupts the ones left running.
	const TArray<TSharedPtr<FACFAssistantSession>>& GetSessions() const { return Sessions; }
	TSharedPtr<FACFAssistantSession> GetActiveSession() const;
	int32 GetActiveSessionId() const { return ActiveSessionId; }
	// Brings a conversation to the front. Allowed at any time, including while that conversation or another one is answering.
	void ActivateSession(int32 LocalId);
	// Stops and unsubscribes one conversation. The others keep running.
	void CloseSession(int32 LocalId);
	bool CanOpenAnotherConversation() const { return Sessions.Num() < MaxOpenSessions; }
	bool MakeRoomForConversation(int32 ProtectedSessionId = 0);
	bool IsSessionOpen(const FString& InThreadId) const;

	const TArray<FACFAssistantMessage>& GetMessages() const;
	const TArray<FString>& GetPendingImages() const;
	FString GetStatus() const;
	// Short human label for one tool call, unwrapping the MCP relay so the panel names the real tool instead of "call_tool".
	static FString ToolActivityLabel(const TSharedPtr<FJsonObject>& Item);

	EACFAssistantWorkMode GetWorkMode() const;
	// Model of the conversation on screen. Turns always use their own conversation's model, never the one on screen.
	FString GetModelId() const { return SessionModelId(GetActiveSession()); }
	// Preferred reasoning level for every conversation; empty is the model's default. Applies from the next turn.
	const FString& GetReasoningEffort() const { return ReasoningEffort; }
	void SetReasoningEffort(const FString& InEffort);
	// The level a turn on this model actually sends: the preference when the model supports it, otherwise the model's default.
	FString EffortFor(const FString& InModelId) const;
	FString SessionModelId(const TSharedPtr<FACFAssistantSession>& Session) const;
	// The provider a conversation runs on: fixed once it has started, otherwise the provider of its model.
	EACFAssistantProvider SessionProvider(const TSharedPtr<FACFAssistantSession>& Session) const;
	TSharedPtr<FACFAssistantModel> FindModel(const FString& Id) const;
	bool IsPlanMode() const;
	bool IsApproveForMe() const;
	bool HasConversationApproval() const;
	bool IsWaitingForApproval() const;
	TSharedPtr<FJsonObject> GetApprovalPreview() const;
	// True when the conversation on screen is answering. Use IsAnyBusy for whole-client decisions such as reconnecting.
	bool IsBusy() const;
	bool IsAnyBusy() const;
	bool IsAnyBusy(EACFAssistantProvider InProvider) const;
	static constexpr bool IsProviderAvailable(EACFAssistantProvider InProvider) { return InProvider != EACFAssistantProvider::Yarin; }
	// True when at least one provider is signed in and loaded, so Yarin can chat.
	bool IsReady() const { return IsProviderReady(EACFAssistantProvider::Codex) || IsProviderReady(EACFAssistantProvider::ClaudeCode) || IsProviderReady(EACFAssistantProvider::Yarin); }
	bool IsProviderReady(EACFAssistantProvider InProvider) const { return IsProviderAvailable(InProvider) && Link(InProvider).IsReady(); }
	EConnectionState GetConnectionState(EACFAssistantProvider InProvider) const { return Link(InProvider).State; }
	// "email (plan)" of the signed-in account, empty when the provider does not report one.
	const FString& GetAccountLabel(EACFAssistantProvider InProvider) const { return Link(InProvider).AccountLabel; }
	// Plain-language account state for one provider's row in the settings window.
	FString GetConnectionSummary(EACFAssistantProvider InProvider) const;
	static const TCHAR* ProviderName(EACFAssistantProvider InProvider);
	FString GetActiveConversationTitle() const;
	// Fraction of the model context window used by the active conversation, 0 when unknown. Fed by the token-count events of the app-server.
	double GetContextUsageFraction() const;
	FString GetContextUsageText() const;

	// Preferred model for new conversations. Kept as saved even while its provider is offline; SessionModelId resolves it against what is loaded.
	FString ModelId;
	FString ReasoningEffort;
	TArray<TSharedPtr<FACFAssistantModel>> Models;
	// Bumped whenever Models or a model label changes, so the panel can rebuild its dropdown only then.
	int32 ModelsRevision = 0;
	TArray<TSharedPtr<FACFAssistantHistory>> History;
	FSimpleMulticastDelegate Changed;

	static bool IsSupportedCodexVersion(const FString& Version);
	// GatewayBaseUrl non-empty adds the `yarin` model provider (OpenAI Responses wire) whose key arrives only through the YARIN_GATEWAY_API_KEY environment variable.
	static FString CodexRuntimeConfiguration(const FString& GatewayBaseUrl = FString());
	static FString FindCodexExecutable();
	static FString FindClaudeExecutable();
	static bool DecodeMessage(TArray<uint8>& Buffer, TSharedPtr<FJsonObject>& Message, bool& bInvalid, FString* OutLine = nullptr);
	static bool IsChatGPTSubscriptionAccount(const TSharedPtr<FJsonObject>& Account);

	/**
	 * One-shot text completion with no tools, MCP, or shell, on the default model's provider when it is ready, otherwise on the other one.
	 * The model must answer with plain text only. Used by AscentLocalizationEditor and similar editor tools.
	 */
	bool RequestOneShotCompletion(const FString& SystemInstructions, const FString& UserMessage, FACFAssistantCompletionResult OnComplete);
	// "Claude Opus 5.5" from a resolved Claude model id such as claude-opus-5-5 or claude-haiku-4-5-20251001. Empty when the id has no family.
	static FString ClaudeModelLabel(const FString& ResolvedId);
	// The models Claude Code itself offers this account, from the `initialize` answer of a stream-json session. Empty when the output holds no catalog.
	static TArray<TSharedPtr<FACFAssistantModel>> ParseClaudeModels(const FString& CatalogOutput);
	// Classifies `claude auth status` JSON. OutProblem explains an Unreadable verdict in words the panel can show.
	static EClaudeAuth ClassifyClaudeAuth(const FString& AuthStatusJson, FString* OutProblem = nullptr);
	// "email (plan)" or "email (API key from ...)" from `claude auth status`, so the panel names the account and who pays.
	static FString ClaudeAccountLabel(const FString& AuthStatusJson);
	// True when a failed Claude turn failed because its sign-in expired or was revoked. `auth status` only reads local credentials, so this is where that surfaces.
	static bool IsClaudeAuthFailure(const FString& Error);
	// True for a NAME=VALUE environment entry that would make a claude child believe it runs nested inside another Claude Code session.
	static bool IsClaudeNestingEnvironmentEntry(const FString& Entry);
	static bool IsClaudeLoginUrl(const FString& Url);
	static FString ExtractClaudeLoginUrl(const FString& Text);
	static const TCHAR* ClaudePermissionModeName(EACFAssistantWorkMode Mode);
	// Applies one Claude stream-json line to the conversation. Returns true when the turn has finished.
	static bool HandleClaudeStreamLine(FACFAssistantSession& Session, const TSharedPtr<FJsonObject>& Message, FString& OutError);

	// Yarin runs on the Codex runtime; every process-level decision (spawn, respawn, busy, watchdog) is taken on that one process.
	static EACFAssistantProvider HarnessOf(EACFAssistantProvider InProvider) { return InProvider == EACFAssistantProvider::ClaudeCode ? EACFAssistantProvider::ClaudeCode : EACFAssistantProvider::Codex; }
	// The callable chat models of a Yarin gateway: GET /models joined with GET /model/info (either may be empty). Entitlement groups such as
	// `tier-indie` never appear in /models and image models are dropped; every entry is a "yarin/<id>" model on the Yarin provider.
	static TArray<TSharedPtr<FACFAssistantModel>> ParseYarinModels(const FString& ModelsJson, const FString& ModelInfoJson);
	static FString YarinModelLabel(const FString& GatewayModelId);
	// The browser address the Yarin cloud handed out for approving this sign-in. Only the configured Yarin cloud's /auth/device page is ever opened.
	static bool IsYarinSignInUrl(const FString& Url, const FString& CloudBaseUrl);
	// What may be written into the Codex profile and sent a bearer key: an https (or loopback http) address of printable ASCII with no quote or backslash.
	// The one rule for the credential store, the cloud's answer and config.toml, so no input is accepted by one and dropped by another.
	static bool IsUsableGatewayUrl(const FString& Url);
	// A gateway key becomes a process environment entry and an Authorization header: printable ASCII only, so no NUL or CR/LF can add entries or headers.
	static bool IsUsableGatewayKey(const FString& Key);
	// Windows DPAPI, current user. Yarin's cloud session is stored only in this form; the plaintext never touches the disk.
	static bool ProtectSecret(const FString& Plain, TArray<uint8>& OutProtected);
	static bool UnprotectSecret(const TArray<uint8>& Protected, FString& OutPlain);
	// MD5 of a gateway key, or empty for none: which key a Codex process holds without keeping a second copy of the key.
	static FString GatewayKeyHash(const FString& Key);

private:
	friend class FAscentACFAssistantAuthTest;
	friend class FAscentACFAssistantPlanModeTest;
	friend class FAscentACFAssistantPasteImageTest;
	friend class FAscentACFAssistantGeneratedImageTest;
	friend class FAscentACFAssistantParallelSessionsTest;
	friend class FAscentACFAssistantSuggestedRepliesTest;
	friend class FAscentACFAssistantYarinTest;
	friend class FAscentACFAssistantConversationGateTest;
	// One provider's connection. Codex adds its app-server process below; Claude runs a CLI process per turn.
	struct FProviderLink
	{
		EConnectionState State = EConnectionState::Disconnected;
		bool bInitialized = false;
		bool bAuthenticated = false;
		// Set by SignIn: when the connection finds the account signed out, it continues straight into the browser sign-in.
		bool bSignInIfSignedOut = false;
		// Connection-level line: sign-in progress and failures. Per-conversation progress lives on the session.
		FString Status;
		FString Failure;
		FString AccountLabel;
		// Bumped on every disconnect so an answer that arrives for an older connection is ignored.
		int32 Generation = 0;
		// Stamped by requests and incoming bytes; the 5-minute watchdog measures silence during a turn from it.
		double LastActivity = 0;
		FTSTicker::FDelegateHandle Ticker;
		bool IsReady() const { return bAuthenticated && bInitialized && State == EConnectionState::Ready; }
	};
	FProviderLink Codex;
	FProviderLink Claude;
	// The Yarin account. It has no process of its own: bInitialized means "the Codex runtime is up carrying this account's gateway key".
	FProviderLink Yarin;
	FProviderLink& Link(EACFAssistantProvider InProvider) { return InProvider == EACFAssistantProvider::ClaudeCode ? Claude : InProvider == EACFAssistantProvider::Yarin ? Yarin : Codex; }
	const FProviderLink& Link(EACFAssistantProvider InProvider) const { return InProvider == EACFAssistantProvider::ClaudeCode ? Claude : InProvider == EACFAssistantProvider::Yarin ? Yarin : Codex; }
	// The link whose process answers for a provider: what the watchdog stamps and what a restart replaces.
	FProviderLink& HarnessLink(EACFAssistantProvider InProvider) { return Link(HarnessOf(InProvider)); }
	struct FPending
	{
		FString Method;
		double Started = 0;
		bool bCritical = true;
		// Conversation that issued the request, 0 for connection-level calls. A failure is reported there instead of taking the whole client down.
		int32 SessionId = 0;
		TFunction<void(TSharedPtr<FJsonObject>)> Callback;
	};
	void* Process = nullptr;
	void* ProcessThread = nullptr;
	void* ProcessJob = nullptr;
	void* OutputRead = nullptr;
	void* InputWrite = nullptr;
	TArray<uint8> Incoming;
	TQueue<TArray<uint8>, EQueueMode::Spsc> Outgoing;
	TFuture<void> Writer;
	// Set by the writer thread itself, so Disconnect can CancelSynchronousIo a WriteFile stuck on a pipe nobody reads.
	std::atomic<void*> WriterThread{nullptr};
	bool bShutDown = false;
	FThreadSafeBool bStopWriter = false;
	FThreadSafeBool bWriteFailed = false;
	TMap<int32, FPending> Pending;
	int32 NextRequest = 1;
	FString ProfileDirectory;
	FString WorkingDirectory;
	FString PreferencesPath;
	TArray<TSharedPtr<FACFAssistantSession>> Sessions;
	int32 ActiveSessionId = 0;
	int32 NextSessionLocalId = 1;
	FString LoginId;

	TSharedPtr<FACFAssistantSession> OpenSession();
	TSharedPtr<FACFAssistantSession> FindSession(int32 LocalId) const;
	TSharedPtr<FACFAssistantSession> FindSessionByThread(const FString& InThreadId) const;
	TSharedPtr<FACFAssistantSession> FindSessionByTitleThread(const FString& InThreadId) const;
	void Unsubscribe(const FString& InThreadId, bool bCritical);

	bool TickCodex(float DeltaSeconds);
	bool TickClaude(float DeltaSeconds);
	void Write(const TSharedPtr<FJsonObject>& Message);
	void Request(const FString& Method, const TSharedPtr<FJsonObject>& Params, TFunction<void(TSharedPtr<FJsonObject>)> Callback = {},
		bool bCritical = true, int32 SessionId = 0);
	void Handle(const TSharedPtr<FJsonObject>& Message);
	void OfferSuggestedReplies(const TSharedPtr<FJsonObject>& Message, const TSharedPtr<FJsonObject>& Params);
	void SubmitSuggestedReplies(const TSharedPtr<FACFAssistantSession>& Session);
	// Unblocks a turn that is waiting on a question, with an empty answer, then clears the buttons.
	void DismissSuggestedReplies(const TSharedPtr<FACFAssistantSession>& Session);
	void HandleSessionEvent(const TSharedPtr<FACFAssistantSession>& Session, const FString& Method, const TSharedPtr<FJsonObject>& Params);
	void HandleToolCall(const TSharedPtr<FJsonObject>& Message);
	void CompleteToolCall(const TSharedPtr<FJsonObject>& Message, const TSharedPtr<FJsonObject>& Result);
	void StartTurn(const TSharedPtr<FACFAssistantSession>& Session);
	void StartTitleGeneration(const TSharedPtr<FACFAssistantSession>& Session);
	void ApplyGeneratedTitle(const TSharedPtr<FACFAssistantSession>& Session);
	void Cancel(const TSharedPtr<FACFAssistantSession>& Session);
	// Answers every pending change of the conversation with a refusal. Used when a turn stops or a conversation goes away.
	void DenyApprovals(const TSharedPtr<FACFAssistantSession>& Session, const TCHAR* StatusText);
	void SettleApproval(const TSharedPtr<FACFAssistantSession>& Session, const TSharedPtr<FACFAssistantApproval>& Approval, bool bApproved);
	// Codex asks before running a command or writing files while the turn's approval policy is on-request. Ask for approval queues it; other modes decline.
	void HandleCodexApprovalRequest(const TSharedPtr<FJsonObject>& Message, const FString& Method, const TSharedPtr<FJsonObject>& Params);
	int32 NextApprovalId = 1;

	struct FOneShotCompletion
	{
		int32 Id = 0;
		FString ThreadId;
		FString TurnId;
		FString AccumulatedText;
		FACFAssistantCompletionResult Callback;
		double Started = 0;
		EACFAssistantProvider Provider = EACFAssistantProvider::Codex;
		bool bClaudeSawText = false;
		void* ClaudeProcess = nullptr;
		void* ClaudeProcessThread = nullptr;
		void* ClaudeOutputRead = nullptr;
		void* ClaudeJob = nullptr;
		TArray<uint8> ClaudeIncoming;
	};
	TArray<TSharedPtr<FOneShotCompletion>> OneShotCompletions;
	int32 NextOneShotId = 1;
	TSharedPtr<FOneShotCompletion> FindOneShotByThread(const FString& InThreadId) const;
	void CompleteOneShot(const TSharedPtr<FOneShotCompletion>& Shot, bool bSuccess, const FString& TextOrError);
	// Fails the one-shots running on one provider; the other provider's keep going.
	void FailOneShots(EACFAssistantProvider InProvider, const FString& Reason);
	void StopOneShotProcess(const TSharedPtr<FOneShotCompletion>& Shot);
	bool StartClaudeOneShot(const TSharedPtr<FOneShotCompletion>& Shot, const FString& SystemInstructions, const FString& UserMessage);
	void TickClaudeOneShots(double Now);
	void AppendChatImage(const TSharedPtr<FACFAssistantSession>& Session, const FString& Path);
	void UpdateTokenUsage(const TSharedPtr<FACFAssistantSession>& Session, const TSharedPtr<FJsonObject>& Params);
	void RefreshModels(const FString& Cursor = FString());
	void ReadAccount();
	// Replaces one provider's entries in the shared model list, keeping Codex models before Claude ones.
	void SetProviderModels(EACFAssistantProvider InProvider, TArray<TSharedPtr<FACFAssistantModel>>&& InModels);
	// The model background work (titles, translations) uses on a provider: the preferred model when it belongs there, else its default.
	FString DefaultModelFor(EACFAssistantProvider InProvider) const;
	// Connection-level failure of one provider: its conversations stop waiting and its settings row shows the reason. The other provider is untouched.
	void Fail(EACFAssistantProvider InProvider, const FString& Message);
	// Failure of one conversation. The provider process and the other conversations are left untouched.
	void FailSession(const TSharedPtr<FACFAssistantSession>& Session, const FString& Message);
	// Queues one protocol line for <ProfileDirectory>/protocol.log so silent connect-but-no-answer sessions can be diagnosed after the fact.
	// Lines are written by FlushProtocolLog once per tick rather than one synchronous append each: with several conversations streaming at
	// once, a file append per message was the only game-thread cost that grew with the number of open conversations. Capped at 4 megabytes.
	void LogProtocol(const TCHAR* Direction, const FString& Line);
	// Writes the queued lines. Called at the end of every tick and immediately on any failure, so a crash loses at most one tick of log.
	void FlushProtocolLog();
	FString ProtocolLogPending;
	// Counted in memory because the log is cleared once per editor run, not per connect: two providers connecting would erase each other's lines.
	// Approximate for non-ASCII text, which only matters for the cap.
	int64 ProtocolLogBytes = 0;
	bool bProtocolLogCleared = false;
	// Codex model/list pages collect here and replace Codex's models in one step once the last page arrives.
	TArray<TSharedPtr<FACFAssistantModel>> PendingCodexModels;
	// Claude alias (sonnet, opus...) -> the exact model id the CLI last resolved it to. Saved beside the preferences so labels show versions at startup.
	TMap<FString, FString> ClaudeResolvedModels;
	FString ClaudeModelsPath() const;
	void NoteClaudeResolvedModel(const FString& Alias, const FString& ResolvedId);

	// Yarin cloud session: the device token proves this computer to the Yarin cloud, the gateway key authorizes model traffic.
	struct FYarinCredential
	{
		FString DeviceToken;
		FString GatewayKey;
		FString GatewayBaseUrl;
		FString AccountLabel;
		bool IsComplete() const { return !DeviceToken.IsEmpty() && !GatewayKey.IsEmpty() && !GatewayBaseUrl.IsEmpty(); }
	};
	FYarinCredential YarinCredential;
	// The catalog of the signed-in gateway. Kept while the Codex runtime restarts so its models can be republished without another request.
	TArray<TSharedPtr<FACFAssistantModel>> YarinModels;
	// Hash of the gateway key the running Codex process was started with, empty when it has none. A different key needs a new process.
	FString CodexGatewayKeyHash;
	FString YarinLoginCode;
	double YarinLoginDeadline = 0;
	bool bYarinPollInFlight = false;
	FTSTicker::FDelegateHandle YarinPollTicker;

	void ConnectYarin();
	void StartYarinLogin();
	void StopYarinLogin();
	bool PollYarinLogin(float DeltaSeconds);
	void CompleteYarinLogin();
	// Mints (or re-reads) this account's gateway key with the device token, then loads the gateway catalog.
	void RefreshYarinGateway();
	void LoadYarinCatalog();
	// Brings the Codex runtime up with the current key, and marks Yarin ready once it is.
	void FinishYarinConnect();
	void SignOutYarin();
	void ForgetYarinCredential();
	// Replaces the Codex process when it still carries a gateway key this computer no longer has.
	void DropCodexGatewayKey();
	// True while the shared Codex process carries work a restart would destroy: a running turn, a ChatGPT browser sign-in, or a one-shot.
	bool IsCodexRuntimeInUse() const;
	FString YarinCredentialPath() const;
	void LoadYarinCredential();
	void SaveYarinCredential() const;
	static FString YarinCloudUrl();
	static FString YarinDeviceFingerprint();
	// Done runs only while Yarin.Generation is still what it was when the request was issued; bumping it drops every answer in flight.
	void YarinHttp(const TCHAR* Verb, const FString& Url, const FString& BearerToken, const FString& Body, TFunction<void(int32, const FString&)> Done);
	// Adds the Yarin route to a Codex thread/start or thread/resume: the custom provider, no hosted web search (the gateway answers it with a
	// misleading 429), and the model's real context window.
	void ApplyYarinRoute(const TSharedPtr<FJsonObject>& Params, const FString& ModelId) const;
	FString WireModelId(const FString& Id) const;
	void SaveHistory();
	void LoadHistory();
	void SaveSession(const TSharedPtr<FACFAssistantSession>& Session);
	// Codex keeps conversations.json so existing threads stay findable; Claude keeps conversations_claude.json. History shows both.
	FString ConversationHistoryPath(EACFAssistantProvider InProvider) const;
	FString SessionTranscriptPath(const FString& InThreadId) const;
	FString Instructions() const;
	TSharedPtr<FJsonObject> ThreadParameters(const TSharedPtr<FACFAssistantSession>& Session) const;
	void Connect(EACFAssistantProvider InProvider, bool bThenSignIn);
	// Stops one provider. Its conversations stay open and resume on the next connect; the other provider keeps running.
	void Disconnect(EACFAssistantProvider InProvider);
	void EnterSignedOut(EACFAssistantProvider InProvider, const FString& Message);
	void ConnectCodex();
	// The rest of connecting once the off-thread `codex --version` check is back: app-server process, initialize, account.
	void StartCodexProcess(int32 CheckGeneration, const FString& Program, int32 ReturnCode, const FString& Version, const FString& ErrorText);
	void StartCodexLogin();
	// Cancels the browser sign-in Codex is waiting on, if any. Without it a started-but-abandoned login blocked every later attempt.
	void CancelCodexLogin();
	void ConnectClaude();
	// Runs `claude auth status` (after `claude auth logout` when bSignOutFirst) off the game thread, then FinishClaudeAuthCheck.
	void StartClaudeAuthCheck(bool bSignOutFirst);
	void FinishClaudeAuthCheck(int32 CheckGeneration, const FString& AuthStatus, bool bSignedOutByUser);
	// The part of connecting that follows a confirmed subscription: MCP, working files, skills, models, Ready.
	void CompleteClaudeConnect(const FString& AuthStatus);
	void LoginClaude();
	void FinishClaudeLogin(const TSharedPtr<FClaudeLoginProcess>& Login, const FString& AuthStatus, bool bTimedOut);
	void StopClaudeLogin();
	FString ClaudeProgram() const;
	// Yarin's own Claude Code plugin folder, loaded per turn with --plugin-dir. Never the user's ~/.claude.
	FString ClaudePluginDirectory() const;
	void RemoveLegacyClaudeSkills() const;
	// The built-in alias list: shown at once on connect, and kept when the CLI cannot answer its catalog.
	void LoadClaudeModels();
	// Asks the CLI which models it offers this account (off the game thread) and swaps them in for the alias list. A failed answer keeps the aliases.
	void RefreshClaudeModels();
	static FString ClaudeMcpConfiguration(const FString& GateUrl);
	FString WorkModeContextText(const TSharedPtr<FACFAssistantSession>& Session) const;
	static FString CodexProjectDirectory();
	// Read-only in Ask, Plan and Ask for approval (Codex then asks before writing); full access in Approve for me and approved conversations.
	TSharedPtr<FJsonObject> CodexSandboxPolicy(const TSharedPtr<FACFAssistantSession>& Session) const;
	// "on-request" while the conversation asks for approval, so Codex raises approval requests instead of failing writes; "never" otherwise.
	const TCHAR* CodexApprovalPolicy(const TSharedPtr<FACFAssistantSession>& Session) const;
public:
	FACFAssistantToolVerdict JudgeMutation(const FString& ConversationToken = FString()) const;
	FString GateUrlFor(const TSharedPtr<FACFAssistantSession>& Session) const;
private:
	FString TurnContextDelta(const TSharedPtr<FACFAssistantSession>& Session) const;
	void NoteContextAnnounced(const TSharedPtr<FACFAssistantSession>& Session) const;
	FString ClaudeTurnPrompt(const TSharedPtr<FACFAssistantSession>& Session) const;
	void StartClaudeTurn(const TSharedPtr<FACFAssistantSession>& Session);
	void StopClaudeProcess(const TSharedPtr<FACFAssistantSession>& Session);
	void CompleteClaudeTurn(const TSharedPtr<FACFAssistantSession>& Session, const FString& Error);
	// Per-conversation choices saved in the transcript: editing approval, model and work mode.
	static void RestoreSessionSettings(FACFAssistantSession& Session, const TSharedPtr<FJsonObject>& Conversation);
	// The Claude sign-in console Yarin is waiting on. Empty unless the state is SigningIn with Claude.
	TSharedPtr<FClaudeLoginProcess> ClaudeLogin;
};

TSharedPtr<FAscentACFAssistant> GetSharedACFAssistant(bool bCreateIfMissing = true);
// Called on engine pre-exit and module shutdown: the shared client must not live until static teardown.
void ShutdownSharedACFAssistant();
