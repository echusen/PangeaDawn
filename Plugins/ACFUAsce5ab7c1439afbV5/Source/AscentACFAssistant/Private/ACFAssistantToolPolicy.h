// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

/**
 * Decides whether an Editor tool call read from the MCP wire only inspects the project or changes it.
 * The verdict feeds the work-mode gate: Ask and Plan block mutating tools, Ask for approval holds them for the user, Approve for me lets them run.
 */
namespace ACFAssistantToolPolicy
{
	struct FToolCall
	{
		// Toolset qualifier as the registry names it, e.g. "AscentAIAgent_AscentDataToolset" or "BlueprintTools". Empty for top-level MCP tools.
		FString Toolset;
		// Tool without the toolset prefix, e.g. "DataAssetInspect" or "list_toolsets".
		FString Tool;
		TSharedPtr<FJsonObject> Arguments;

		// Human label for the approval banner and the phone page.
		FString Label() const;
	};

	/**
	 * Unwraps an MCP tools/call into the real tool. Epic's tool-search relay (call_tool with toolset_name / tool_name / arguments) is
	 * unwrapped so the verdict applies to the tool that will actually run, not to the relay.
	 */
	FToolCall Unwrap(const FString& McpToolName, const TSharedPtr<FJsonObject>& McpArguments);

	/**
	 * True when the call only reads Editor state. Arbitrary-code tools are always mutating. Otherwise resolution checks the
	 * Read Only Tools and Mutating Tools lists in Project Settings, then ReadOnlyTool / MutatingTool metadata, then verb words.
	 * Unknown names count as mutating.
	 */
	bool IsReadOnly(const FToolCall& Call);

	/**
	 * True for arbitrary-code execution or an explicit Live Coding request: a mutating tool named for it (TriggerLiveCoding), a
	 * console command naming it, or the Ctrl+Alt+F11 key press. Read-only calls that merely mention it pass. Scripts can construct
	 * compile commands dynamically, so their declared mode cannot waive the individual approval. Checked before conversation approval.
	 */
	bool RequiresIndividualApproval(const FToolCall& Call);

	// Word-level heuristic on its own, exposed for tests. Names are split on `_`, `.`, `-` and PascalCase boundaries.
	bool IsReadOnlyByName(const FString& ToolName);

	// Pattern syntax of the settings lists: "Toolset.Tool", "Toolset.*", or a bare tool name. Case-insensitive.
	bool MatchesPattern(const FString& Pattern, const FString& Toolset, const FString& Tool);
}
