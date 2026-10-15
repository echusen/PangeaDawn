// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "ACFAssistantCompletion.h"
#include "ACFAssistantClientInternal.h"

bool FACFAssistantCompletion::IsAvailable(FString* OutReason)
{
	const TSharedPtr<FAscentACFAssistant> Assistant = GetSharedACFAssistant(false);
	if (!Assistant.IsValid())
	{
		if (OutReason)
		{
			*OutReason = TEXT("Open Yarin and sign in first.");
		}
		return false;
	}
	const bool bYarin = Assistant->IsProviderReady(EACFAssistantProvider::Yarin);
	const bool bCodex = Assistant->IsProviderReady(EACFAssistantProvider::Codex);
	const bool bClaude = Assistant->IsProviderReady(EACFAssistantProvider::ClaudeCode);
	if (!bYarin && !bCodex && !bClaude)
	{
		if (OutReason)
		{
			*OutReason = TEXT("Yarin is not connected. Open Yarin and sign in.");
		}
		return false;
	}
	if (OutReason)
	{
		TArray<FString> Ready;
		if (bYarin) { Ready.Add(TEXT("Yarin")); }
		if (bCodex) { Ready.Add(TEXT("ChatGPT")); }
		if (bClaude) { Ready.Add(TEXT("Claude")); }
		*OutReason = TEXT("Yarin ") + FString::Join(Ready, TEXT(" and ")) + TEXT(": ready");
	}
	return true;
}

bool FACFAssistantCompletion::Request(
	const FString& SystemInstructions,
	const FString& UserMessage,
	FACFAssistantCompletionResult OnComplete)
{
	FString Reason;
	if (!IsAvailable(&Reason))
	{
		OnComplete.ExecuteIfBound(false, Reason);
		return false;
	}
	const TSharedPtr<FAscentACFAssistant> Assistant = GetSharedACFAssistant(false);
	return Assistant->RequestOneShotCompletion(SystemInstructions, UserMessage, OnComplete);
}
