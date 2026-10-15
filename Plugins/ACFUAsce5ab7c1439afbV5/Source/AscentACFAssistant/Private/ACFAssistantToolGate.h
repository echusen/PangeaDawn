// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Yarin's work-mode gate: a loopback MCP endpoint that Codex and Claude Code are pointed at instead of the Editor tool server.
 * Every JSON-RPC message is relayed to Yarin's private Epic MCP server, except that tools/call is judged first:
 * read-only tools pass, mutating tools are rejected in Ask and Plan, held for the user in Ask for approval, and run in Approve for me.
 * Only Yarin's own CLI processes know this URL, so the shared tool registry and any other MCP client on this machine are untouched.
 */
namespace ACFAssistantToolGate
{
	/** Starts the gate on a loopback port next to the Editor tool server. Safe to call repeatedly. Game thread only. */
	bool Ensure();

	/** URL the CLIs use, e.g. http://127.0.0.1:8900/mcp. Valid after Ensure() succeeded. */
	FString Url();

	bool IsRunning();

	/** Stops the gate and answers every held tool call with a refusal. */
	void Shutdown();

	// Name of the MCP tool Claude Code calls for its own permission prompts (Bash, Edit, Write) in Ask for approval. Served by the gate itself.
	inline constexpr TCHAR ClaudePermissionToolName[] = TEXT("yarin_permission");
	inline constexpr TCHAR ConversationQueryKey[] = TEXT("conversation");
}
