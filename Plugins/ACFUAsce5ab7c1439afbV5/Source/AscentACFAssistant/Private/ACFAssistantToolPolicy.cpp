// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "ACFAssistantToolPolicy.h"
#include "ACFAssistantSettings.h"
#include "Misc/Char.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/Class.h"
#include "UObject/UObjectIterator.h"

namespace ACFAssistantToolPolicy
{
	namespace
	{
		// Verbs that only observe the Editor. A name is read-only when it carries one of these and none of the mutating verbs.
		const TCHAR* ReadVerbs[] = {
			TEXT("inspect"), TEXT("search"), TEXT("list"), TEXT("get"), TEXT("find"), TEXT("describe"), TEXT("read"), TEXT("query"),
			TEXT("diff"), TEXT("summary"), TEXT("summarize"), TEXT("validate"), TEXT("schema"), TEXT("estimate"), TEXT("check"),
			TEXT("preview"), TEXT("analyze"), TEXT("analyse"), TEXT("explain"), TEXT("suggest"), TEXT("count"), TEXT("has"), TEXT("is"),
			TEXT("exists"), TEXT("lookup"), TEXT("show"), TEXT("dump"), TEXT("info"), TEXT("status"), TEXT("fetch"), TEXT("view"),
			TEXT("scan"), TEXT("detect"), TEXT("compare"), TEXT("resolve"), TEXT("browse"), TEXT("glob"), TEXT("grep"), TEXT("screenshot"),
			TEXT("capture"), TEXT("select"), TEXT("focus"), TEXT("flow"), TEXT("selected"), TEXT("overridable"), TEXT("complexity")
		};

		// Verbs that change project content, assets, levels, or files. Any one of these makes the tool mutating.
		const TCHAR* WriteVerbs[] = {
			TEXT("create"), TEXT("add"), TEXT("set"), TEXT("remove"), TEXT("delete"), TEXT("rename"), TEXT("replace"), TEXT("link"),
			TEXT("unlink"), TEXT("connect"), TEXT("disconnect"), TEXT("build"), TEXT("clear"), TEXT("configure"), TEXT("author"),
			TEXT("implement"), TEXT("attach"), TEXT("detach"), TEXT("import"), TEXT("generate"), TEXT("apply"), TEXT("spawn"),
			TEXT("destroy"), TEXT("save"), TEXT("move"), TEXT("duplicate"), TEXT("compile"), TEXT("reorder"), TEXT("write"), TEXT("edit"),
			TEXT("update"), TEXT("modify"), TEXT("run"), TEXT("execute"), TEXT("exec"), TEXT("assign"), TEXT("register"),
			TEXT("unregister"), TEXT("enable"), TEXT("disable"), TEXT("reset"), TEXT("change"), TEXT("install"), TEXT("upgrade"),
			TEXT("revert"), TEXT("copy"), TEXT("paste"), TEXT("cut"), TEXT("transfer"), TEXT("insert"), TEXT("append"), TEXT("patch"),
			TEXT("toggle"), TEXT("open"), TEXT("close"), TEXT("load"), TEXT("unload"), TEXT("play"), TEXT("stop"), TEXT("python"),
			TEXT("make"), TEXT("bake"), TEXT("cook"), TEXT("export"), TEXT("convert"), TEXT("migrate"), TEXT("fix"), TEXT("repair"),
			TEXT("undo"), TEXT("redo"), TEXT("place"), TEXT("drop"), TEXT("kill"), TEXT("launch"), TEXT("start"), TEXT("restart")
		};

		void SplitWords(const FString& Name, TArray<FString>& OutWords)
		{
			FString Current;
			auto Flush = [&]()
			{
				if (!Current.IsEmpty()) { OutWords.Add(Current.ToLower()); Current.Reset(); }
			};
			for (int32 Index = 0; Index < Name.Len(); ++Index)
			{
				const TCHAR Character = Name[Index];
				if (Character == TEXT('_') || Character == TEXT('.') || Character == TEXT('-') || Character == TEXT(' ') || Character == TEXT('/'))
				{
					Flush();
					continue;
				}
				// PascalCase boundary: lower/digit followed by upper, or an acronym run ending before "Xy" (e.g. "MCPTool" -> "MCP", "Tool").
				if (FChar::IsUpper(Character) && !Current.IsEmpty())
				{
					const TCHAR Previous = Name[Index - 1];
					const bool bNextIsLower = Index + 1 < Name.Len() && FChar::IsLower(Name[Index + 1]);
					if (FChar::IsLower(Previous) || FChar::IsDigit(Previous) || (FChar::IsUpper(Previous) && bNextIsLower))
					{
						Flush();
					}
				}
				Current.AppendChar(Character);
			}
			Flush();
		}

		bool ContainsWord(const TArray<FString>& Words, const TCHAR* const* Verbs, int32 Count)
		{
			for (const FString& Word : Words)
			{
				for (int32 Index = 0; Index < Count; ++Index)
				{
					if (Word.Equals(Verbs[Index], ESearchCase::IgnoreCase)) { return true; }
				}
			}
			return false;
		}

		// Toolset qualifiers look like "AscentAIAgent_AscentDataToolset"; the UCLASS is the last segment. Cached: the gate asks on every tool call.
		const UClass* FindToolsetClass(const FString& Toolset)
		{
			if (Toolset.IsEmpty()) { return nullptr; }
			static TMap<FString, TWeakObjectPtr<UClass>> Cache;
			if (const TWeakObjectPtr<UClass>* Known = Cache.Find(Toolset))
			{
				if (Known->IsValid()) { return Known->Get(); }
				Cache.Remove(Toolset);
			}
			FString ClassName = Toolset;
			int32 Separator = INDEX_NONE;
			if (ClassName.FindLastChar(TEXT('_'), Separator) || ClassName.FindLastChar(TEXT('.'), Separator))
			{
				ClassName = ClassName.Mid(Separator + 1);
			}
			for (TObjectIterator<UClass> It; It; ++It)
			{
				if (It->GetName().Equals(ClassName, ESearchCase::IgnoreCase) || It->GetName().Equals(Toolset, ESearchCase::IgnoreCase))
				{
					Cache.Add(Toolset, *It);
					return *It;
				}
			}
			return nullptr;
		}

		// Explicit ReadOnlyTool / MutatingTool metadata on the toolset UFUNCTION beats the name heuristic.
		TOptional<bool> ReadOnlyFromMetadata(const FToolCall& Call)
		{
#if WITH_EDITORONLY_DATA
			const UClass* ToolsetClass = FindToolsetClass(Call.Toolset);
			if (!ToolsetClass) { return {}; }
			const UFunction* Function = ToolsetClass->FindFunctionByName(*Call.Tool);
			if (!Function) { return {}; }
			if (Function->HasMetaData(TEXT("ReadOnlyTool"))) { return true; }
			if (Function->HasMetaData(TEXT("MutatingTool"))) { return false; }
#endif
			return {};
		}

		bool IsArbitraryExecution(const FToolCall& Call)
		{
			return Call.Tool.Contains(TEXT("Python"), ESearchCase::IgnoreCase)
				|| Call.Tool.Contains(TEXT("py_run"), ESearchCase::IgnoreCase)
				|| Call.Tool.Equals(TEXT("execute_tool_script"), ESearchCase::IgnoreCase);
		}
	}

	FString FToolCall::Label() const
	{
		return Toolset.IsEmpty() ? Tool : Toolset + TEXT(".") + Tool;
	}

	FToolCall Unwrap(const FString& McpToolName, const TSharedPtr<FJsonObject>& McpArguments)
	{
		FToolCall Call;
		Call.Arguments = McpArguments;
		if (McpToolName.Equals(TEXT("call_tool"), ESearchCase::IgnoreCase) && McpArguments.IsValid())
		{
			McpArguments->TryGetStringField(TEXT("toolset_name"), Call.Toolset);
			McpArguments->TryGetStringField(TEXT("tool_name"), Call.Tool);
			const TSharedPtr<FJsonObject>* Inner = nullptr;
			Call.Arguments = McpArguments->TryGetObjectField(TEXT("arguments"), Inner) && Inner ? *Inner : nullptr;
			if (Call.Tool.IsEmpty()) { Call.Tool = McpToolName; }
			return Call;
		}
		// Eager registration names tools "Toolset.Tool"; top-level MCP tools such as list_toolsets have no dot.
		int32 Dot = INDEX_NONE;
		if (McpToolName.FindLastChar(TEXT('.'), Dot))
		{
			Call.Toolset = McpToolName.Left(Dot);
			Call.Tool = McpToolName.Mid(Dot + 1);
		}
		else
		{
			Call.Tool = McpToolName;
		}
		return Call;
	}

	bool MatchesPattern(const FString& Pattern, const FString& Toolset, const FString& Tool)
	{
		const FString Trimmed = Pattern.TrimStartAndEnd();
		if (Trimmed.IsEmpty()) { return false; }
		int32 Dot = INDEX_NONE;
		if (!Trimmed.FindLastChar(TEXT('.'), Dot))
		{
			return Trimmed.Equals(Tool, ESearchCase::IgnoreCase) || Trimmed == TEXT("*");
		}
		const FString PatternToolset = Trimmed.Left(Dot);
		const FString PatternTool = Trimmed.Mid(Dot + 1);
		const bool bToolsetMatches = PatternToolset == TEXT("*") || PatternToolset.Equals(Toolset, ESearchCase::IgnoreCase)
			// Allow the short class name too ("AscentDataToolset.*" for "AscentAIAgent_AscentDataToolset").
			|| Toolset.EndsWith(TEXT("_") + PatternToolset, ESearchCase::IgnoreCase) || Toolset.EndsWith(TEXT(".") + PatternToolset, ESearchCase::IgnoreCase);
		const bool bToolMatches = PatternTool == TEXT("*") || PatternTool.Equals(Tool, ESearchCase::IgnoreCase);
		return bToolsetMatches && bToolMatches;
	}

	bool IsReadOnlyByName(const FString& ToolName)
	{
		TArray<FString> Words;
		SplitWords(ToolName, Words);
		if (ContainsWord(Words, WriteVerbs, UE_ARRAY_COUNT(WriteVerbs))) { return false; }
		return ContainsWord(Words, ReadVerbs, UE_ARRAY_COUNT(ReadVerbs));
	}

	bool IsReadOnly(const FToolCall& Call)
	{
		// A caller-supplied mode or read-only override cannot sandbox arbitrary code.
		if (IsArbitraryExecution(Call)) { return false; }
		const UACFAssistantSettings& Settings = UACFAssistantSettings::Get();
		for (const FString& Pattern : Settings.ReadOnlyTools)
		{
			if (MatchesPattern(Pattern, Call.Toolset, Call.Tool)) { return true; }
		}
		for (const FString& Pattern : Settings.MutatingTools)
		{
			if (MatchesPattern(Pattern, Call.Toolset, Call.Tool)) { return false; }
		}
		if (const TOptional<bool> FromMetadata = ReadOnlyFromMetadata(Call); FromMetadata.IsSet())
		{
			return FromMetadata.GetValue();
		}
		return IsReadOnlyByName(Call.Tool);
	}

	bool RequiresIndividualApproval(const FToolCall& Call)
	{
		// Computed Python commands can invoke Live Coding without spelling its name in the request.
		if (IsArbitraryExecution(Call)) { return true; }
		// Spacing and separators vary between console commands and tool names: LiveCoding.Compile, live_coding, "Live Coding".
		auto Squash = [](FString Text)
		{
			Text.ReplaceInline(TEXT("_"), TEXT(""));
			Text.ReplaceInline(TEXT(" "), TEXT(""));
			Text.ReplaceInline(TEXT("-"), TEXT(""));
			return Text;
		};
		// Reads that only mention Live Coding (describe_toolset, log searches, GetLiveCodingStatus) pass like any other read.
		if (IsReadOnly(Call)) { return false; }
		if (Squash(Call.Tool).Contains(TEXT("livecoding"), ESearchCase::IgnoreCase)) { return true; }
		if (!Call.Arguments.IsValid()) { return false; }
		FString Arguments;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Arguments);
		FJsonSerializer::Serialize(Call.Arguments.ToSharedRef(), Writer);
		Arguments = Squash(Arguments);
		// A console command, or the Ctrl+Alt+F11 shortcut sent as a key press.
		return Arguments.Contains(TEXT("livecoding"), ESearchCase::IgnoreCase) || Arguments.Contains(TEXT("F11"), ESearchCase::IgnoreCase);
	}
}
