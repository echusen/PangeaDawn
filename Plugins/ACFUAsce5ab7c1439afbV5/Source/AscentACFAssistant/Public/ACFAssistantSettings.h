// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Fonts/SlateFontInfo.h"
#include "ACFAssistantSettings.generated.h"

/**
 * Who answers and bills. Codex (ChatGPT subscription) and Claude Code are the two vendor CLIs. Yarin is the Yarin cloud gateway:
 * its models run on the same Codex runtime, but sign in with a Yarin account and bill Yarin credits, never a ChatGPT subscription.
 */
UENUM()
enum class EACFAssistantProvider : uint8
{
	Codex UMETA(DisplayName = "Codex"),
	ClaudeCode UMETA(DisplayName = "Claude Code"),
	Yarin UMETA(DisplayName = "Yarin")
};

/** How Yarin may use its tools for the current conversation. */
UENUM()
enum class EACFAssistantWorkMode : uint8
{
	Ask UMETA(DisplayName = "Ask"),
	Plan UMETA(DisplayName = "Plan"),
	AskForApproval UMETA(DisplayName = "Ask for approval"),
	ApproveForMe UMETA(DisplayName = "Approve for me")
};

/** How the phone reaches this Editor while the tunnel is open. */
UENUM()
enum class EACFAssistantPhoneTransport : uint8
{
	Cloudflare UMETA(DisplayName = "Cloudflare (public HTTPS link)"),
	Iroh UMETA(DisplayName = "Iroh (end-to-end encrypted)")
};

/**
 * Yarin settings.
 * Edit > Project Settings > Engine > Ascent Yarin.
 * Codex and Claude Code are found on PATH and in their standard install folders.
 * Image and 3D keys live on Plugins > Ascent AI Tools.
 */
UCLASS(config = EditorPerProjectUserSettings, meta = (DisplayName = "Ascent Yarin"))
class ASCENTACFASSISTANT_API UACFAssistantSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	static UACFAssistantSettings& Get();
	UACFAssistantSettings();

	virtual FName GetContainerName() const override { return TEXT("Project"); }
	virtual FName GetCategoryName() const override { return TEXT("Engine"); }
	virtual FName GetSectionName() const override { return TEXT("Ascent Yarin"); }

	FSlateFontInfo ResolvedChatFont() const;
	FSlateFontInfo ResolvedWorkFont() const;
	static FSimpleMulticastDelegate ChatDisplayChanged;
	static FSimpleMulticastDelegate PhoneAccessChanged;

	/** Optional override. Empty uses PATH and the standard Codex install folders. Not shown in settings. */
	UPROPERTY(config)
	FString CodexExecutable;

	/** Optional override. Empty uses PATH. Not shown in settings. */
	UPROPERTY(config)
	FString ClaudeExecutable;

	/** Optional override of the Yarin cloud address used for sign-in. Empty uses https://yarin.darktowerinteractive.com. Not shown in settings. */
	UPROPERTY(config)
	FString YarinCloudUrl;

	/** Preferred model for new conversations. Yarin, Codex and Claude Code stay connected together; the model decides which one answers. */
	UPROPERTY(EditAnywhere, config, Category = "Connection", meta = (DisplayName = "Model"))
	FString ModelId;

	/**
	 * How long the model thinks before answering: low, medium, high, xhigh, max (Codex models may offer minimal or none).
	 * Lower is faster. Empty, or a level the picked model does not support, uses the model's own default.
	 */
	UPROPERTY(EditAnywhere, config, Category = "Connection", meta = (DisplayName = "Reasoning"))
	FString ReasoningEffort;

	/**
	 * Ask and Plan block every Editor tool that changes the project; the gate rejects the call before it runs.
	 * Ask for approval holds each change (Editor tool, file edit, shell command) until you press Approve here or on the phone.
	 * Approve for me runs changes without asking.
	 */
	UPROPERTY(EditAnywhere, config, Category = "Editing", meta = (DisplayName = "Work Mode"))
	EACFAssistantWorkMode WorkMode = EACFAssistantWorkMode::Plan;

	/**
	 * Editor tools that only read the project and therefore never need approval, in addition to what Yarin infers from the tool name.
	 * Patterns: "Toolset.Tool", "Toolset.*", or a bare tool name. Case-insensitive. Example: "AscentDataToolset.DataAssetInspect".
	 */
	UPROPERTY(EditAnywhere, config, Category = "Editing", meta = (DisplayName = "Read Only Tools"))
	TArray<FString> ReadOnlyTools;

	/** Editor tools that must always count as changes, overriding the name heuristic. Same pattern syntax as Read Only Tools. */
	UPROPERTY(EditAnywhere, config, Category = "Editing", meta = (DisplayName = "Mutating Tools"))
	TArray<FString> MutatingTools;

	/** When on, Claude Code can edit project files, including C++ under Source/. Turn off to disable Claude's Edit and Write tools. Applied on the next message. */
	UPROPERTY(EditAnywhere, config, Category = "Editing", meta = (DisplayName = "Allow Source File Edits"))
	bool bAllowSourceFileEdits = true;

	/** When on, Claude Code can run Bash and PowerShell. Applied on the next message. */
	UPROPERTY(EditAnywhere, config, Category = "Editing", meta = (DisplayName = "Allow Shell"))
	bool bAllowShell = true;

	/** When on, Claude Code can use WebSearch and WebFetch. Applied on the next message. */
	UPROPERTY(EditAnywhere, config, Category = "Editing", meta = (DisplayName = "Allow Web"))
	bool bAllowWeb = true;

	/** Size of You and Yarin message text. */
	UPROPERTY(EditAnywhere, config, Category = "Chat",
		meta = (DisplayName = "Chat Font Size", ClampMin = "6", ClampMax = "48", UIMin = "8", UIMax = "28"))
	int32 ChatFontSize = 10;

	/** Size of the collapsed Work row and tool-call lines. Keep this smaller than Chat Font Size. */
	UPROPERTY(EditAnywhere, config, Category = "Chat",
		meta = (DisplayName = "Work Font Size", ClampMin = "6", ClampMax = "48", UIMin = "6", UIMax = "20"))
	int32 WorkFontSize = 7;

	/** When on, thinking notes and tool calls appear in chat as one collapsed row with an arrow. Turn off to hide that work entirely. */
	UPROPERTY(EditAnywhere, config, Category = "Chat", meta = (DisplayName = "Show Work In Chat"))
	bool bShowWorkInChat = true;

	/** Typeface of You and Yarin message text. Size is Chat Font Size above. */
	UPROPERTY(EditAnywhere, config, Category = "Chat", meta = (DisplayName = "Chat Font"))
	FSlateFontInfo ChatFont;

	/** Typeface of the collapsed Work row and the tool-call lines inside it. Size is Work Font Size above. */
	UPROPERTY(EditAnywhere, config, Category = "Chat", meta = (DisplayName = "Work Font"))
	FSlateFontInfo WorkFont;

	/** New assets the assistant creates are saved under this content folder unless your request names another location. Keeps generated content separated from the rest of the project. */
	UPROPERTY(EditAnywhere, config, Category = "Editing", meta = (DisplayName = "Default Content Folder"))
	FString DefaultContentFolder = TEXT("/Game/Yarin");

	/** Default Content Folder from these settings, with a trailing slash removed. Empty when the setting is empty. */
	FString ResolvedContentFolder() const;

	// Extra TOML appended to the isolated Codex profile, typically for external MCP servers.
	// Unreal's ue-editor MCP server is configured automatically and must not be repeated here.
	UPROPERTY(EditAnywhere, config, Category = "Connection", meta = (DisplayName = "Extra Codex Configuration (TOML)", MultiLine = true))
	FString ExtraCodexConfigurationToml;

	/** Lets Codex run commands inside its own sandbox, confined to the assistant's isolated working folder. Required for ChatGPT image generation without an API key. Applied on the next Connect. */
	UPROPERTY(EditAnywhere, config, Category = "Connection", meta = (DisplayName = "Codex Sandboxed Shell"))
	bool bEnableCodexShell = true;

	/** First loopback port of Yarin's private Editor tool server. If it is busy, the next 9 ports are tried. Applied on the next Connect. Never Unreal's shared port 8000. */
	UPROPERTY(EditAnywhere, config, Category = "Connection", meta = (ClampMin = "1024", ClampMax = "65535", DisplayName = "MCP Server Port"))
	int32 McpServerPort = 8890;

	/**
	 * Off by default. Turn this on only if you accept the risk.
	 * Open tunnel in the phone window opens a connection from the internet to this Editor: a public HTTPS link (Cloudflare)
	 * or an encrypted Iroh connection, as chosen under Phone Transport.
	 * Anyone who has the link can send prompts and approve Editor changes, including edits to this project.
	 * Nothing is published until this is on. After that, with Iroh (the default) and Open Phone Tunnel At Editor Startup on (also the default), the paired link reopens by itself whenever the Editor starts. Close tunnel turns it off until the next Editor start or Open tunnel; quitting the Editor also turns it off.
	 */
	UPROPERTY(EditAnywhere, config, Category = "Phone", meta = (DisplayName = "Allow Phone Access From The Internet"))
	bool bAllowPhoneAccessFromTheInternet = false;

	/**
	 * Cloudflare: a public https link through Cloudflare's free quick tunnel. Opens in any phone browser, but Cloudflare carries
	 * the traffic and can read it.
	 * Iroh (default): an end-to-end encrypted connection that goes straight to this PC when the network allows, otherwise through a relay
	 * that only sees encrypted bytes. Needs the Yarin Android app.
	 * Changing this while the tunnel is open restarts it. Cloudflare issues a new link each time. Iroh keeps one link per project, so a phone scans it once; Forget Paired Phone in the phone window replaces it.
	 */
	UPROPERTY(EditAnywhere, config, Category = "Phone", meta = (DisplayName = "Phone Transport", EditCondition = "bAllowPhoneAccessFromTheInternet"))
	EACFAssistantPhoneTransport PhoneTransport = EACFAssistantPhoneTransport::Iroh;

	/** Local port the public phone link forwards to. Change it only if another program already owns 3010. */
	UPROPERTY(EditAnywhere, config, Category = "Phone", meta = (ClampMin = "1024", ClampMax = "65535", DisplayName = "Phone Access Port", EditCondition = "bAllowPhoneAccessFromTheInternet"))
	int32 PhoneAccessPort = 3010;

	/**
	 * Iroh only. On by default. Opens the phone tunnel by itself when this project's Editor starts, so the paired phone reconnects with no click on the PC. Turn it off to open the tunnel only with Open tunnel in the phone window.
	 * Nothing opens until Allow Phone Access From The Internet is on. While the tunnel is open, anyone holding the paired phone's link can send prompts and approve Editor changes.
	 */
	UPROPERTY(EditAnywhere, config, Category = "Phone", meta = (DisplayName = "Open Phone Tunnel At Editor Startup", EditCondition = "bAllowPhoneAccessFromTheInternet && PhoneTransport == EACFAssistantPhoneTransport::Iroh"))
	bool bOpenPhoneTunnelAtStartup = true;

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
};
