// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "ACFLocalizationSubsystem.h"
#include "ACFAssistantCompletion.h"
#include "AscentLocalizationEditor.h"
#include "LocalizationSettings.h"
#include "LocalizationTargetTypes.h"
#include "LocalizationConfigurationScript.h"
#include "LocalizationCommandletTasks.h"
#include "LocalizationCommandletExecution.h"
#include "Containers/Ticker.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "PortableObjectFormatDOM.h"
#include "Framework/Application/SlateApplication.h"
#include "Internationalization/Culture.h"
#include "Internationalization/Internationalization.h"
#include "UObject/UnrealType.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/ConfigContext.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "Widgets/SWindow.h"
#include "Framework/Docking/TabManager.h"

void UACFLocalizationSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	for (ULocalizationTarget* Target : GetGameTargets())
	{
		RegisterGameLocalizationPath(Target);
	}
}

void UACFLocalizationSubsystem::Deinitialize()
{
	StopTextSearch();
	bBusy = false;
	Super::Deinitialize();
}

TArray<ULocalizationTarget*> UACFLocalizationSubsystem::GetGameTargets() const
{
	TArray<ULocalizationTarget*> Result;
	if (ULocalizationTargetSet* Set = ULocalizationSettings::GetGameTargetSet())
	{
		for (ULocalizationTarget* Target : Set->TargetObjects)
		{
			if (IsValid(Target))
			{
				Result.Add(Target);
			}
		}
	}
	return Result;
}

void UACFLocalizationSubsystem::SetStatus(const FString& NewStatus)
{
	Status = NewStatus;
	LogLines.Add(NewStatus);
	if (LogLines.Num() > 40)
	{
		LogLines.RemoveAt(0, LogLines.Num() - 40);
	}
	UE_LOG(LogAscentLocalizationEditor, Log, TEXT("%s"), *Status);
	StatusChanged.Broadcast(Status);
}

void UACFLocalizationSubsystem::ReportError(const FString& Error)
{
	LastError = Error;
	UE_LOG(LogAscentLocalizationEditor, Error, TEXT("%s"), *Error);
	SetStatus(FString::Printf(TEXT("ERROR — %s"), *Error));
}

void UACFLocalizationSubsystem::Finish(bool bSuccess, const FString& Summary)
{
	bBusy = false;
	bCancelRequested = false;
	StopTextSearch();
	ActiveTarget.Reset();
	Entries.Reset();
	LastSummary = Summary;
	if (!bSuccess)
	{
		LastError = Summary;
		UE_LOG(LogAscentLocalizationEditor, Error, TEXT("%s"), *Summary);
		SetStatus(FString::Printf(TEXT("FAILED — %s"), *Summary));
	}
	else
	{
		LastError.Reset();
		SetStatus(FString::Printf(TEXT("DONE — %s"), *Summary));
	}
	JobFinished.Broadcast();
}

TArray<FString> UACFLocalizationSubsystem::GetGatherIncludePaths(ULocalizationTarget* Target) const
{
	TArray<FString> Paths;
	if (!IsValid(Target))
	{
		return Paths;
	}
	for (const FGatherTextIncludePath& Include : Target->Settings.GatherFromPackages.IncludePathWildcards)
	{
		if (!Include.Pattern.IsEmpty())
		{
			Paths.Add(Include.Pattern);
		}
	}
	return Paths;
}

bool UACFLocalizationSubsystem::AddGatherIncludePath(ULocalizationTarget* Target, const FString& Folder)
{
	if (!IsValid(Target) || bBusy)
	{
		return false;
	}
	FString Pattern = Folder.TrimStartAndEnd().Replace(TEXT("\\"), TEXT("/"));
	if (Pattern.StartsWith(TEXT("/Game/")))
	{
		Pattern = TEXT("Content/") + Pattern.Mid(6);
	}
	Pattern.RemoveFromStart(TEXT("/"));
	if (Pattern.IsEmpty())
	{
		return false;
	}
	if (!Pattern.Contains(TEXT("*")))
	{
		if (!Pattern.EndsWith(TEXT("/")))
		{
			Pattern += TEXT("/");
		}
		Pattern += TEXT("*");
	}
	for (const FGatherTextIncludePath& Include : Target->Settings.GatherFromPackages.IncludePathWildcards)
	{
		if (Include.Pattern.Equals(Pattern, ESearchCase::IgnoreCase))
		{
			return true;
		}
	}
	FGatherTextIncludePath NewPath;
	NewPath.PathRoot = ELocalizationGatherPathRoot::Project;
	NewPath.Pattern = Pattern;
	Target->Settings.GatherFromPackages.IsEnabled = true;
	Target->Settings.GatherFromPackages.IncludePathWildcards.Add(NewPath);
	Target->MarkPackageDirty();
	if (ULocalizationSettings* Settings = GetMutableDefault<ULocalizationSettings>())
	{
		FPropertyChangedEvent EmptyEvent(nullptr);
		Settings->PostEditChangeProperty(EmptyEvent);
	}
	LocalizationConfigurationScript::GenerateAllConfigFiles(Target);
	return true;
}

bool UACFLocalizationSubsystem::RemoveGatherIncludePath(ULocalizationTarget* Target, const FString& Pattern)
{
	if (!IsValid(Target) || bBusy || Pattern.IsEmpty())
	{
		return false;
	}
	const int32 Removed = Target->Settings.GatherFromPackages.IncludePathWildcards.RemoveAll(
		[&Pattern](const FGatherTextIncludePath& Include)
		{
			return Include.Pattern.Equals(Pattern, ESearchCase::IgnoreCase);
		});
	if (Removed == 0)
	{
		return false;
	}
	Target->MarkPackageDirty();
	if (ULocalizationSettings* Settings = GetMutableDefault<ULocalizationSettings>())
	{
		FPropertyChangedEvent EmptyEvent(nullptr);
		Settings->PostEditChangeProperty(EmptyEvent);
	}
	LocalizationConfigurationScript::GenerateAllConfigFiles(Target);
	return true;
}

void UACFLocalizationSubsystem::Cancel()
{
	if (!bBusy)
	{
		return;
	}
	bCancelRequested = true;
	if (SearchStep != ETextSearchStep::None)
	{
		StopTextSearch();
		Finish(false, TEXT("Cancelled during text search."));
		return;
	}
	SetStatus(TEXT("WORKING — Cancelling after the current Yarin reply..."));
}

void UACFLocalizationSubsystem::StartTranslation(ULocalizationTarget* Target, const FString& CultureCode, bool bOverwriteExisting)
{
	if (bBusy)
	{
		return;
	}
	if (!IsValid(Target))
	{
		Finish(false, TEXT("Select a localization target."));
		return;
	}
	const FString Culture = CultureCode.TrimStartAndEnd();
	if (Culture.IsEmpty())
	{
		Finish(false, TEXT("Enter a culture code (for example fr, de, ja)."));
		return;
	}
	if (!FInternationalization::Get().GetCulture(Culture))
	{
		Finish(false, FString::Printf(TEXT("Unknown culture code: %s"), *Culture));
		return;
	}

	FString AvailabilityReason;
	if (!FACFAssistantCompletion::IsAvailable(&AvailabilityReason))
	{
		Finish(false, AvailabilityReason);
		return;
	}

	bBusy = true;
	bCancelRequested = false;
	LastError.Reset();
	LogLines.Reset();
	ProgressDone = 0;
	ProgressTotal = 0;
	bOverwrite = bOverwriteExisting;
	ActiveTarget = Target;
	ActiveCulture = Culture;
	Entries.Reset();
	NextBatchIndex = 0;
	TranslatedCount = 0;
	SkippedCount = 0;
	LastSummary.Reset();

	if (!EnsureCultureOnTarget(Target, Culture))
	{
		Finish(false, FString::Printf(TEXT("Could not add culture %s to the target."), *Culture));
		return;
	}

	RegisterGameLocalizationPath(Target);
	BeginTextSearch();
}

void UACFLocalizationSubsystem::ContinueAfterTextFileReady(const FString& NativePoPath)
{
	ULocalizationTarget* Target = ActiveTarget.Get();
	if (!IsValid(Target) || !bBusy)
	{
		return;
	}
	if (bCancelRequested)
	{
		Finish(false, TEXT("Cancelled."));
		return;
	}

	const FString CulturePoPath = LocalizationConfigurationScript::GetDefaultPOPath(Target, ActiveCulture);
	SetStatus(TEXT("WORKING — Reading the exported text file..."));
	if (!LoadEntriesFromPo(NativePoPath, CulturePoPath))
	{
		Finish(false, TEXT("Could not read the exported PO file."));
		return;
	}

	int32 Pending = 0;
	for (const FTranslationEntry& Entry : Entries)
	{
		if (Entry.bSkipped && Entry.SkipReason == TEXT("Already translated"))
		{
			++SkippedCount;
		}
		else if (!Entry.bSkipped && !Entry.bTranslated)
		{
			++Pending;
		}
	}
	if (Pending == 0)
	{
		Finish(true, TEXT("Nothing to translate. All strings already have translations (or use Overwrite)."));
		return;
	}

	ProgressTotal = Entries.Num();
	ProgressDone = SkippedCount;
	SetStatus(FString::Printf(TEXT("WORKING — %d strings to send to Yarin. %d already translated and skipped."), Pending, SkippedCount));
	TranslateNextBatch();
}

bool UACFLocalizationSubsystem::EnsureCultureOnTarget(ULocalizationTarget* Target, const FString& CultureCode)
{
	if (!IsValid(Target))
	{
		return false;
	}
	for (const FCultureStatistics& Stats : Target->Settings.SupportedCulturesStatistics)
	{
		if (Stats.CultureName.Equals(CultureCode, ESearchCase::IgnoreCase))
		{
			return true;
		}
	}
	Target->Settings.SupportedCulturesStatistics.Add(FCultureStatistics(CultureCode));
	Target->MarkPackageDirty();
	LocalizationConfigurationScript::GenerateAllConfigFiles(Target);
	return true;
}

void UACFLocalizationSubsystem::StopTextSearch()
{
	if (SearchTicker.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(SearchTicker);
		SearchTicker.Reset();
	}
	SearchProcess.Reset();
	SearchStep = ETextSearchStep::None;
}

bool UACFLocalizationSubsystem::LaunchTextCommandlet(const FString& ConfigPath, bool bUseProjectFile)
{
	SearchProcess = FLocalizationCommandletProcess::Execute(ConfigPath, bUseProjectFile);
	return SearchProcess.IsValid();
}

void UACFLocalizationSubsystem::BeginTextSearch()
{
	ULocalizationTarget* Target = ActiveTarget.Get();
	if (!IsValid(Target))
	{
		Finish(false, TEXT("Select a localization target."));
		return;
	}

	NativeCulture = Target->Settings.SupportedCulturesStatistics.IsValidIndex(Target->Settings.NativeCultureIndex)
		? Target->Settings.SupportedCulturesStatistics[Target->Settings.NativeCultureIndex].CultureName
		: TEXT("en");
	PackagesScanned = 0;
	LastLoggedPackageCount = 0;
	bCommandletLoggedExitZero = false;
	LastSearchDetail.Reset();
	SearchOutputRemainder.Reset();
	SearchStartedAt = FPlatformTime::Seconds();
	SearchStep = ETextSearchStep::Gather;

	const FString GatherScriptPath = LocalizationConfigurationScript::GetGatherTextConfigPath(Target);
	LocalizationConfigurationScript::GenerateGatherTextConfigFile(Target).WriteWithSCC(GatherScriptPath);
	SetStatus(TEXT("WORKING — Step 1 of 3: searching the project for texts. Translation has not started. This scans assets and can take several minutes. Stay on this panel."));

	if (!LaunchTextCommandlet(GatherScriptPath, !Target->IsMemberOfEngineTargetSet()))
	{
		Finish(false, TEXT("Could not start the text search. The editor commandlet did not launch."));
		return;
	}

	TWeakObjectPtr<UACFLocalizationSubsystem> WeakThis(this);
	SearchTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakThis](float DeltaSeconds)
	{
		UACFLocalizationSubsystem* Self = WeakThis.Get();
		return Self && Self->TickTextSearch(DeltaSeconds);
	}));
}

bool UACFLocalizationSubsystem::TickTextSearch(float DeltaSeconds)
{
	(void)DeltaSeconds;
	if (!bBusy || SearchStep == ETextSearchStep::None)
	{
		return false;
	}
	if (bCancelRequested)
	{
		Finish(false, TEXT("Cancelled during text search."));
		return false;
	}
	if (!SearchProcess.IsValid())
	{
		Finish(false, TEXT("Text search stopped unexpectedly."));
		return false;
	}

	IngestCommandletOutput(FPlatformProcess::ReadPipe(SearchProcess->GetReadPipe()));

	int32 ReturnCode = 0;
	FProcHandle Handle = SearchProcess->GetHandle();
	if (Handle.IsValid() && FPlatformProcess::GetProcReturnCode(Handle, &ReturnCode))
	{
		IngestCommandletOutput(FPlatformProcess::ReadPipe(SearchProcess->GetReadPipe()));
		SearchProcess.Reset();
		OnTextCommandletExited(ReturnCode);
		return bBusy && SearchProcess.IsValid();
	}

	RefreshSearchStatus();
	return true;
}

void UACFLocalizationSubsystem::RefreshSearchStatus()
{
	const int32 Elapsed = FMath::Max(0, static_cast<int32>(FPlatformTime::Seconds() - SearchStartedAt));
	const TCHAR* StepText = SearchStep == ETextSearchStep::Export
		? TEXT("Step 2 of 3: writing the source text file. Translation has not started.")
		: TEXT("Step 1 of 3: searching the project for texts. Translation has not started.");
	const FString Now = LastSearchDetail.IsEmpty() ? TEXT("starting up") : LastSearchDetail;
	const FString NewStatus = FString::Printf(
		TEXT("WORKING — %s  %d:%02d elapsed. Assets opened: %d. Now: %s"),
		StepText, Elapsed / 60, Elapsed % 60, PackagesScanned, *Now);
	if (NewStatus == Status)
	{
		return;
	}
	Status = NewStatus;
	StatusChanged.Broadcast(Status);
	if (PackagesScanned >= LastLoggedPackageCount + 40)
	{
		LastLoggedPackageCount = PackagesScanned;
		LogLines.Add(FString::Printf(TEXT("Still searching. Assets opened: %d. Now: %s"), PackagesScanned, *Now));
		if (LogLines.Num() > 40)
		{
			LogLines.RemoveAt(0, LogLines.Num() - 40);
		}
	}
}

void UACFLocalizationSubsystem::IngestCommandletOutput(const FString& Chunk)
{
	if (Chunk.IsEmpty())
	{
		return;
	}
	SearchOutputRemainder += Chunk;
	FString Line;
	while (SearchOutputRemainder.Split(TEXT("\n"), &Line, &SearchOutputRemainder))
	{
		Line.TrimStartAndEndInline();
		if (Line.IsEmpty()
			|| Line.Contains(TEXT("LogHttpListener"))
			|| Line.Contains(TEXT("LogExit"))
			|| Line.Contains(TEXT("LogShader"))
			|| Line.Contains(TEXT("LogInit:")))
		{
			continue;
		}
		if (Line.Contains(TEXT("exit code 0")))
		{
			bCommandletLoggedExitZero = true;
		}
		if (Line.Contains(TEXT("Loading package")) || Line.Contains(TEXT("LoadPackage")))
		{
			++PackagesScanned;
		}
		int32 PathIndex = Line.Find(TEXT("/Game/"));
		if (PathIndex == INDEX_NONE)
		{
			PathIndex = Line.Find(TEXT("/Engine/"));
		}
		FString Detail = PathIndex != INDEX_NONE ? Line.Mid(PathIndex) : Line;
		if (Detail.Len() > 140)
		{
			Detail.LeftInline(140);
		}
		if (PathIndex != INDEX_NONE || Line.Contains(TEXT("Gather")) || Line.Contains(TEXT("Export")) || Line.Contains(TEXT("Error:")))
		{
			LastSearchDetail = Detail;
		}
	}
}

bool UACFLocalizationSubsystem::StartExportAfterSearch()
{
	ULocalizationTarget* Target = ActiveTarget.Get();
	if (!IsValid(Target))
	{
		return false;
	}
	SearchStep = ETextSearchStep::Export;
	PackagesScanned = 0;
	LastLoggedPackageCount = 0;
	bCommandletLoggedExitZero = false;
	LastSearchDetail.Reset();
	SearchOutputRemainder.Reset();
	SearchStartedAt = FPlatformTime::Seconds();

	const TOptional<FString> CultureOpt(NativeCulture);
	const FString ExportScriptPath = LocalizationConfigurationScript::GetExportTextConfigPath(Target, CultureOpt);
	LocalizationConfigurationScript::GenerateExportTextConfigFile(Target, CultureOpt).WriteWithSCC(ExportScriptPath);
	SetStatus(FString::Printf(TEXT("WORKING — Step 2 of 3: writing the source text file (%s). Translation has not started."), *NativeCulture));
	return LaunchTextCommandlet(ExportScriptPath, !Target->IsMemberOfEngineTargetSet());
}

void UACFLocalizationSubsystem::OnTextCommandletExited(int32 ReturnCode)
{
	ULocalizationTarget* Target = ActiveTarget.Get();
	if (!IsValid(Target))
	{
		Finish(false, TEXT("The localization target disappeared during text search."));
		return;
	}

	if (SearchStep == ETextSearchStep::Gather)
	{
		const FString ManifestPath = LocalizationConfigurationScript::GetManifestPath(Target);
		const bool bManifest = FPaths::FileExists(ManifestPath);
		if (!bManifest)
		{
			Finish(false, FString::Printf(
				TEXT("Text search failed. No text list was written. Last line: %s"),
				LastSearchDetail.IsEmpty() ? TEXT("(no output)") : *LastSearchDetail));
			return;
		}
		if (ReturnCode != 0 && !bCommandletLoggedExitZero)
		{
			LogLines.Add(TEXT("Text search reported an extra engine error, but the text list was written. Continuing to the source file."));
		}
		if (!StartExportAfterSearch())
		{
			Finish(false, TEXT("Could not start writing the source text file."));
		}
		return;
	}

	if (SearchStep == ETextSearchStep::Export)
	{
		const FString PoPath = LocalizationConfigurationScript::GetDefaultPOPath(Target, NativeCulture);
		if (!FPaths::FileExists(PoPath))
		{
			Finish(false, FString::Printf(
				TEXT("Writing the source text file failed. File missing: %s. Last line: %s"),
				*PoPath,
				LastSearchDetail.IsEmpty() ? TEXT("(no output)") : *LastSearchDetail));
			return;
		}
		SearchStep = ETextSearchStep::None;
		if (SearchTicker.IsValid())
		{
			FTSTicker::GetCoreTicker().RemoveTicker(SearchTicker);
			SearchTicker.Reset();
		}
		SetStatus(TEXT("WORKING — Source text file is ready. Reading strings. Translation starts after this."));
		ContinueAfterTextFileReady(PoPath);
	}
}

bool UACFLocalizationSubsystem::LoadEntriesFromPo(const FString& PoPath, const FString& CulturePoPath)
{
	FString PoText;
	if (!FFileHelper::LoadFileToString(PoText, *PoPath))
	{
		return false;
	}
	FPortableObjectFormatDOM NativePo;
	FText ParseError;
	if (!NativePo.FromString(PoText, &ParseError))
	{
		UE_LOG(LogAscentLocalizationEditor, Warning, TEXT("PO parse failed: %s"), *ParseError.ToString());
		return false;
	}

	TMap<FString, FString> ExistingTranslations;
	if (FPaths::FileExists(CulturePoPath))
	{
		FString CultureText;
		if (FFileHelper::LoadFileToString(CultureText, *CulturePoPath))
		{
			FPortableObjectFormatDOM CulturePo;
			if (CulturePo.FromString(CultureText))
			{
				for (FPortableObjectEntries::TConstIterator It = CulturePo.GetEntriesIterator(); It; ++It)
				{
					const TSharedPtr<FPortableObjectEntry>& Entry = It.Value();
					if (!Entry.IsValid() || Entry->MsgId.IsEmpty())
					{
						continue;
					}
					const FString Key = Entry->MsgCtxt + TEXT("\x1f") + Entry->MsgId;
					if (Entry->MsgStr.Num() > 0 && !Entry->MsgStr[0].IsEmpty())
					{
						ExistingTranslations.Add(Key, Entry->MsgStr[0]);
					}
				}
			}
		}
	}

	Entries.Reset();
	for (FPortableObjectEntries::TConstIterator It = NativePo.GetEntriesIterator(); It; ++It)
	{
		const TSharedPtr<FPortableObjectEntry>& Entry = It.Value();
		if (!Entry.IsValid() || Entry->MsgId.IsEmpty())
		{
			continue;
		}
		FTranslationEntry Row;
		Row.MsgCtxt = Entry->MsgCtxt;
		Row.MsgId = Entry->MsgId;
		const FString Key = Row.MsgCtxt + TEXT("\x1f") + Row.MsgId;
		if (const FString* Existing = ExistingTranslations.Find(Key))
		{
			if (!bOverwrite)
			{
				Row.Translation = *Existing;
				Row.bTranslated = true;
				Row.bSkipped = true;
				Row.SkipReason = TEXT("Already translated");
			}
		}
		Entries.Add(MoveTemp(Row));
	}
	return true;
}

FString UACFLocalizationSubsystem::BuildSystemPrompt(const FString& LanguageDisplayName)
{
	return FString::Printf(
		TEXT("You are a professional game localizer. Translate each string into %s.\n")
		TEXT("Return ONLY a JSON array of objects: [{\"id\":0,\"text\":\"...\"}, ...].\n")
		TEXT("Preserve every Unreal placeholder exactly ({0}, {1}, {Name}, %%s, %%d).\n")
		TEXT("Preserve rich-text tags and their nesting (<...>...</...>).\n")
		TEXT("Do not invent ids. Do not wrap the JSON in markdown."),
		*LanguageDisplayName);
}

FString UACFLocalizationSubsystem::BuildBatchUserMessage(const TArray<FTranslationEntry>& Batch)
{
	TArray<TSharedPtr<FJsonValue>> Array;
	for (int32 Index = 0; Index < Batch.Num(); ++Index)
	{
		TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetNumberField(TEXT("id"), Index);
		Obj->SetStringField(TEXT("source"), Batch[Index].MsgId);
		if (!Batch[Index].MsgCtxt.IsEmpty())
		{
			Obj->SetStringField(TEXT("context"), Batch[Index].MsgCtxt);
		}
		Array.Add(MakeShared<FJsonValueObject>(Obj));
	}
	FString Json;
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Json);
	FJsonSerializer::Serialize(Array, Writer);
	return TEXT("Translate these game strings:\n") + Json;
}

bool UACFLocalizationSubsystem::ParseBatchResponse(const FString& Response, TMap<int32, FString>& OutIdToText, FString& OutError)
{
	OutIdToText.Reset();
	FString Json = Response.TrimStartAndEnd();
	if (Json.StartsWith(TEXT("```")))
	{
		int32 FirstNewline = INDEX_NONE;
		Json.FindChar(TEXT('\n'), FirstNewline);
		if (FirstNewline != INDEX_NONE)
		{
			Json = Json.Mid(FirstNewline + 1);
		}
		const int32 Fence = Json.Find(TEXT("```"));
		if (Fence != INDEX_NONE)
		{
			Json = Json.Left(Fence);
		}
		Json = Json.TrimStartAndEnd();
	}
	const int32 ArrayStart = Json.Find(TEXT("["));
	const int32 ArrayEnd = Json.Find(TEXT("]"), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
	if (ArrayStart == INDEX_NONE || ArrayEnd == INDEX_NONE || ArrayEnd <= ArrayStart)
	{
		OutError = TEXT("Response was not a JSON array.");
		return false;
	}
	Json = Json.Mid(ArrayStart, ArrayEnd - ArrayStart + 1);

	TArray<TSharedPtr<FJsonValue>> Array;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	if (!FJsonSerializer::Deserialize(Reader, Array))
	{
		OutError = TEXT("Could not parse JSON array from Yarin.");
		return false;
	}
	for (const TSharedPtr<FJsonValue>& Value : Array)
	{
		if (!Value.IsValid() || Value->Type != EJson::Object)
		{
			continue;
		}
		const TSharedPtr<FJsonObject> Obj = Value->AsObject();
		double IdNumber = 0;
		FString Text;
		if (!Obj->TryGetNumberField(TEXT("id"), IdNumber) || !Obj->TryGetStringField(TEXT("text"), Text))
		{
			continue;
		}
		OutIdToText.Add(static_cast<int32>(IdNumber), Text);
	}
	if (OutIdToText.Num() == 0)
	{
		OutError = TEXT("JSON array contained no usable translations.");
		return false;
	}
	return true;
}

void UACFLocalizationSubsystem::CollectPlaceholders(const FString& Text, TArray<FString>& OutPlaceholders, TArray<FString>& OutTags)
{
	OutPlaceholders.Reset();
	OutTags.Reset();
	for (int32 Index = 0; Index < Text.Len(); ++Index)
	{
		if (Text[Index] == TEXT('{'))
		{
			const int32 Close = Text.Find(TEXT("}"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Index);
			if (Close != INDEX_NONE)
			{
				OutPlaceholders.Add(Text.Mid(Index, Close - Index + 1));
				Index = Close;
			}
		}
		else if (Text[Index] == TEXT('<'))
		{
			const int32 Close = Text.Find(TEXT(">"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Index);
			if (Close != INDEX_NONE)
			{
				OutTags.Add(Text.Mid(Index, Close - Index + 1));
				Index = Close;
			}
		}
		else if (Text[Index] == TEXT('%') && Index + 1 < Text.Len())
		{
			OutPlaceholders.Add(Text.Mid(Index, 2));
			++Index;
		}
	}
}

bool UACFLocalizationSubsystem::TranslationPreservesPlaceholders(const FString& Source, const FString& Translation, FString& OutReason)
{
	TArray<FString> SourcePh, SourceTags, TransPh, TransTags;
	CollectPlaceholders(Source, SourcePh, SourceTags);
	CollectPlaceholders(Translation, TransPh, TransTags);
	for (const FString& Ph : SourcePh)
	{
		if (!TransPh.Contains(Ph))
		{
			OutReason = FString::Printf(TEXT("Missing placeholder %s"), *Ph);
			return false;
		}
	}
	for (const FString& Tag : SourceTags)
	{
		if (!TransTags.Contains(Tag))
		{
			OutReason = FString::Printf(TEXT("Missing rich-text tag %s"), *Tag);
			return false;
		}
	}
	return true;
}

void UACFLocalizationSubsystem::TranslateNextBatch()
{
	if (bCancelRequested)
	{
		Finish(false, TEXT("Cancelled."));
		return;
	}

	TArray<int32> BatchIndices;
	for (int32 Index = NextBatchIndex; Index < Entries.Num() && BatchIndices.Num() < BatchSize; ++Index)
	{
		if (!Entries[Index].bTranslated && !Entries[Index].bSkipped)
		{
			BatchIndices.Add(Index);
		}
	}
	if (BatchIndices.Num() == 0)
	{
		SetStatus(TEXT("WORKING — Writing translations, importing, and compiling the culture..."));
		if (!ApplyTranslationsAndCompile())
		{
			Finish(false, TEXT("Import or compile failed after translation."));
			return;
		}
		Finish(true, FString::Printf(
			TEXT("Done. Translated %d strings, skipped %d. Culture: %s"),
			TranslatedCount, SkippedCount, *ActiveCulture));
		return;
	}

	NextBatchIndex = BatchIndices.Last() + 1;
	TArray<FTranslationEntry> Batch;
	for (const int32 Index : BatchIndices)
	{
		Batch.Add(Entries[Index]);
	}

	const FCulturePtr Culture = FInternationalization::Get().GetCulture(ActiveCulture);
	const FString LanguageName = Culture.IsValid() ? Culture->GetEnglishName() : ActiveCulture;
	const FString SystemPrompt = BuildSystemPrompt(LanguageName);
	const FString UserMessage = BuildBatchUserMessage(Batch);

	const int32 Done = TranslatedCount + SkippedCount;
	const int32 Total = Entries.Num();
	ProgressDone = Done;
	ProgressTotal = Total;
	const int32 BatchNumber = (Done / BatchSize) + 1;
	const int32 BatchCount = FMath::Max(1, FMath::DivideAndRoundUp(Total, BatchSize));
	SetStatus(FString::Printf(
		TEXT("WORKING — Waiting for Yarin. Batch %d of %d. Strings %d / %d. Translated %d, skipped %d."),
		BatchNumber, BatchCount, FMath::Min(Done + Batch.Num(), Total), Total, TranslatedCount, SkippedCount));

	TWeakObjectPtr<UACFLocalizationSubsystem> WeakThis(this);
	TArray<int32> IndicesCopy = BatchIndices;
	FACFAssistantCompletion::Request(SystemPrompt, UserMessage,
		FACFAssistantCompletionResult::CreateLambda([WeakThis, IndicesCopy](bool bSuccess, const FString& ResponseOrError)
		{
			UACFLocalizationSubsystem* Self = WeakThis.Get();
			if (!Self || !Self->bBusy)
			{
				return;
			}
			if (Self->bCancelRequested)
			{
				Self->Finish(false, TEXT("Cancelled."));
				return;
			}
			if (!bSuccess)
			{
				int32 FailedInBatch = 0;
				for (const int32 EntryIndex : IndicesCopy)
				{
					if (Self->Entries.IsValidIndex(EntryIndex) && !Self->Entries[EntryIndex].bTranslated)
					{
						Self->Entries[EntryIndex].bSkipped = true;
						Self->Entries[EntryIndex].SkipReason = ResponseOrError;
						++Self->SkippedCount;
						++FailedInBatch;
					}
				}
				Self->ReportError(FString::Printf(
					TEXT("Yarin failed this batch (%d strings skipped). The job continues. Reason: %s"),
					FailedInBatch, *ResponseOrError));
				Self->TranslateNextBatch();
				return;
			}

			TMap<int32, FString> IdToText;
			FString ParseError;
			const bool bParsed = ParseBatchResponse(ResponseOrError, IdToText, ParseError);
			int32 RejectedInBatch = 0;
			if (bParsed)
			{
				for (const TPair<int32, FString>& Pair : IdToText)
				{
					if (!IndicesCopy.IsValidIndex(Pair.Key))
					{
						continue;
					}
					const int32 EntryIndex = IndicesCopy[Pair.Key];
					if (!Self->Entries.IsValidIndex(EntryIndex))
					{
						continue;
					}
					FTranslationEntry& Entry = Self->Entries[EntryIndex];
					FString Reason;
					if (!TranslationPreservesPlaceholders(Entry.MsgId, Pair.Value, Reason))
					{
						Entry.bSkipped = true;
						Entry.SkipReason = Reason;
						++Self->SkippedCount;
						++RejectedInBatch;
						continue;
					}
					Entry.Translation = Pair.Value;
					Entry.bTranslated = true;
					++Self->TranslatedCount;
				}
			}
			int32 MissingInBatch = 0;
			for (const int32 EntryIndex : IndicesCopy)
			{
				if (Self->Entries.IsValidIndex(EntryIndex)
					&& !Self->Entries[EntryIndex].bTranslated
					&& !Self->Entries[EntryIndex].bSkipped)
				{
					Self->Entries[EntryIndex].bSkipped = true;
					Self->Entries[EntryIndex].SkipReason = ParseError.IsEmpty()
						? TEXT("Missing from Yarin response")
						: ParseError;
					++Self->SkippedCount;
					++MissingInBatch;
				}
			}
			if (RejectedInBatch > 0)
			{
				Self->ReportError(FString::Printf(
					TEXT("%d strings were rejected because a placeholder or rich-text tag changed. The job continues."),
					RejectedInBatch));
			}
			if (MissingInBatch > 0)
			{
				Self->ReportError(FString::Printf(
					TEXT("%d strings in this batch were skipped. The job continues. Reason: %s"),
					MissingInBatch,
					ParseError.IsEmpty() ? TEXT("Missing from Yarin response") : *ParseError));
			}
			Self->TranslateNextBatch();
		}));
}

FString UACFLocalizationSubsystem::EscapePoString(const FString& Text)
{
	FString Escaped = Text;
	Escaped.ReplaceInline(TEXT("\\"), TEXT("\\\\"), ESearchCase::CaseSensitive);
	Escaped.ReplaceInline(TEXT("\""), TEXT("\\\""), ESearchCase::CaseSensitive);
	Escaped.ReplaceInline(TEXT("\r\n"), TEXT("\\n"), ESearchCase::CaseSensitive);
	Escaped.ReplaceInline(TEXT("\n"), TEXT("\\n"), ESearchCase::CaseSensitive);
	Escaped.ReplaceInline(TEXT("\r"), TEXT("\\n"), ESearchCase::CaseSensitive);
	Escaped.ReplaceInline(TEXT("\t"), TEXT("\\t"), ESearchCase::CaseSensitive);
	return Escaped;
}

bool UACFLocalizationSubsystem::ApplyTranslationsAndCompile()
{
	ULocalizationTarget* Target = ActiveTarget.Get();
	if (!IsValid(Target))
	{
		return false;
	}

	FPortableObjectFormatDOM CulturePo;
	CulturePo.CreateNewHeader();
	CulturePo.SetProjectName(Target->Settings.Name);
	if (!CulturePo.SetLanguage(ActiveCulture))
	{
		UE_LOG(LogAscentLocalizationEditor, Warning, TEXT("SetLanguage failed for %s"), *ActiveCulture);
		return false;
	}

	for (const FTranslationEntry& Entry : Entries)
	{
		TSharedRef<FPortableObjectEntry> PoEntry = MakeShared<FPortableObjectEntry>();
		PoEntry->MsgCtxt = EscapePoString(Entry.MsgCtxt);
		PoEntry->MsgId = EscapePoString(Entry.MsgId);
		if (Entry.bTranslated && !Entry.Translation.IsEmpty())
		{
			PoEntry->MsgStr.Add(EscapePoString(Entry.Translation));
		}
		else
		{
			PoEntry->MsgStr.Add(FString());
		}
		CulturePo.AddEntry(PoEntry);
	}

	const FString PoPath = LocalizationConfigurationScript::GetDefaultPOPath(Target, ActiveCulture);
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(PoPath), true);
	const FString PoContents = CulturePo.ToString();
	if (!FFileHelper::SaveStringToFile(PoContents, *PoPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		return false;
	}

	TSharedPtr<SWindow> ParentWindow = FSlateApplication::Get().GetActiveTopLevelWindow();
	if (!ParentWindow.IsValid())
	{
		ParentWindow = FGlobalTabmanager::Get()->GetRootWindow();
	}
	if (!ParentWindow.IsValid())
	{
		return false;
	}

	if (!LocalizationCommandletTasks::ImportTextForCulture(ParentWindow.ToSharedRef(), Target, ActiveCulture, PoPath))
	{
		return false;
	}
	if (!LocalizationCommandletTasks::CompileTextForCulture(ParentWindow.ToSharedRef(), Target, ActiveCulture))
	{
		return false;
	}
	RegisterGameLocalizationPath(Target);
	return true;
}

void UACFLocalizationSubsystem::RegisterGameLocalizationPath(const ULocalizationTarget* Target)
{
	if (!IsValid(Target) || Target->Settings.Name.IsEmpty())
	{
		return;
	}

	// Same path the Localization Dashboard writes when a target's loading policy is Game.
	// Preview Game Language only lists cultures under these paths, for whatever the target is named.
	const FString DataDirectory = LocalizationConfigurationScript::GetDataDirectory(Target);
	const FString CollapsedDataDirectory = FConfigValue::CollapseValue(DataDirectory);
	if (CollapsedDataDirectory.IsEmpty())
	{
		return;
	}

	TArray<FString> Paths;
	GConfig->GetArray(TEXT("Internationalization"), TEXT("LocalizationPaths"), Paths, GGameIni);
	if (Paths.Contains(DataDirectory) || Paths.Contains(CollapsedDataDirectory))
	{
		return;
	}

	const FString DefaultGameIni = FPaths::SourceConfigDir() / TEXT("DefaultGame.ini");
	FConfigFile IniFile;
	FConfigCacheIni::LoadLocalIniFile(IniFile, TEXT("DefaultGame"), false);
	IniFile.AddToSection(TEXT("Internationalization"), TEXT("+LocalizationPaths"), CollapsedDataDirectory);
	IniFile.Dirty = true;
	IniFile.UpdateSections(*DefaultGameIni);
	FConfigContext::ForceReloadIntoGConfig().Load(TEXT("Game"));
}
