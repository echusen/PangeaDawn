// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Result of a one-shot completion through Yarin.
 * On failure, ResponseOrError is a human-readable reason.
 * On success, ResponseOrError is the model text (no tool calls).
 */
DECLARE_DELEGATE_TwoParams(FACFAssistantCompletionResult, bool /*bSuccess*/, const FString& /*ResponseOrError*/);

/**
 * Short completion on the provider Yarin is already signed into: ChatGPT (Codex) or Claude.
 * Spawns no tools, MCP servers, or shell — text in, text out.
 * Sign in through the Yarin panel first; then call Request.
 */
class ASCENTACFASSISTANT_API FACFAssistantCompletion
{
public:
	/** True when Yarin is connected with ChatGPT or Claude. On success, OutReason names which one. */
	static bool IsAvailable(FString* OutReason = nullptr);

	/**
	 * Runs one completion turn. Callback fires on the game thread.
	 * Returns false immediately when IsAvailable() is false (and still invokes the callback).
	 */
	static bool Request(
		const FString& SystemInstructions,
		const FString& UserMessage,
		FACFAssistantCompletionResult OnComplete);
};
