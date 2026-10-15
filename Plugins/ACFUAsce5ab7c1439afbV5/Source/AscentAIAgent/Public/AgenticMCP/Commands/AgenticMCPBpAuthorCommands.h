// Copyright Insodimension Studios. All Rights Reserved.
//
// AgenticMCPBpAuthorCommands
// Atomic Blueprint authoring surface for the Agentic UE Bridge.
// Wire-format command names: "bp_actions_search", "bp_author_graph"
// (Python tool layer adds the agentic_ prefix).
//
// Backed by UE 5.7's FBlueprintActionDatabase — the same registry that
// powers the BP editor's right-click "Add Node" menu. The LLM doesn't
// memorize K2Node class names + per-class config keys; it queries the
// registry for handles + pin signatures and authors entire graphs in
// one declarative spec per call.

#pragma once

#include "CoreMinimal.h"
#include "Json.h"

class AGENTICMCP_API FAgenticMCPBpAuthorCommands
{
public:
	FAgenticMCPBpAuthorCommands() = default;

	TSharedPtr<FJsonObject> HandleCommand(const FString& InCommandType, const TSharedPtr<FJsonObject>& InParams);

private:
	// ---- Phase A — Discovery ----
	// Read-only walk of FBlueprintActionDatabase. Returns the top N spawners
	// matching a query string with full pin signatures and synthesized handle
	// strings (fn.<Lib>.<Func>, var.<Name>, ev.<Name>, flow.<Verb>, etc.).
	TSharedPtr<FJsonObject> HandleActionsSearch(const TSharedPtr<FJsonObject>& InParams);

	// ---- Phase B — Atomic graph authoring ----
	// Builds an entire BP graph from a single declarative spec. Pre-flight
	// validates the full payload, auto-repairs known LLM mistakes, then
	// spawns nodes via UBlueprintNodeSpawner::Invoke (the canonical editor
	// path). One transaction, one MarkBlueprintAsModified at end.
	TSharedPtr<FJsonObject> HandleAuthorGraph(const TSharedPtr<FJsonObject>& InParams);
};
