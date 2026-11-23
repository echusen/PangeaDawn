// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "Containers/Ticker.h"
#include "LocalizationCommandletExecution.h"
#include "ACFLocalizationSubsystem.generated.h"

class ULocalizationTarget;

DECLARE_MULTICAST_DELEGATE_OneParam(FACFLocalizationStatusChanged, const FString& /*Status*/);
DECLARE_MULTICAST_DELEGATE(FACFLocalizationJobFinished);

UCLASS()
class UACFLocalizationSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	TArray<ULocalizationTarget*> GetGameTargets() const;
	bool IsBusy() const { return bBusy; }
	bool HasError() const { return !LastError.IsEmpty(); }
	FString GetStatus() const { return Status; }
	FString GetLastError() const { return LastError; }
	FString GetLastSummary() const { return LastSummary; }
	int32 GetProgressDone() const { return ProgressDone; }
	int32 GetProgressTotal() const { return ProgressTotal; }
	/** Newest line last. Working steps and errors, kept while the job runs. */
	const TArray<FString>& GetLogLines() const { return LogLines; }

	FACFLocalizationStatusChanged& OnStatusChanged() { return StatusChanged; }
	FACFLocalizationJobFinished& OnJobFinished() { return JobFinished; }

	/** Gather, export, translate missing strings into CultureCode, import, and compile. */
	void StartTranslation(ULocalizationTarget* Target, const FString& CultureCode, bool bOverwriteExisting);

	void Cancel();

	/** Asset folders the selected target scans for FText. Patterns are relative to the project, for example "Content/MyGame/" followed by a wildcard. */
	TArray<FString> GetGatherIncludePaths(ULocalizationTarget* Target) const;
	bool AddGatherIncludePath(ULocalizationTarget* Target, const FString& Folder);
	bool RemoveGatherIncludePath(ULocalizationTarget* Target, const FString& Pattern);

private:
	struct FTranslationEntry
	{
		FString MsgCtxt;
		FString MsgId;
		FString Translation;
		bool bTranslated = false;
		bool bSkipped = false;
		FString SkipReason;
	};

	bool bBusy = false;
	bool bCancelRequested = false;
	FString Status;
	FString LastError;
	FString LastSummary;
	int32 ProgressDone = 0;
	int32 ProgressTotal = 0;
	TArray<FString> LogLines;
	FACFLocalizationStatusChanged StatusChanged;
	FACFLocalizationJobFinished JobFinished;

	TWeakObjectPtr<ULocalizationTarget> ActiveTarget;
	FString ActiveCulture;
	bool bOverwrite = false;
	TArray<FTranslationEntry> Entries;
	int32 NextBatchIndex = 0;
	int32 TranslatedCount = 0;
	int32 SkippedCount = 0;
	static constexpr int32 BatchSize = 40;

	enum class ETextSearchStep : uint8
	{
		None,
		Gather,
		Export
	};

	ETextSearchStep SearchStep = ETextSearchStep::None;
	FString NativeCulture;
	TSharedPtr<FLocalizationCommandletProcess> SearchProcess;
	FTSTicker::FDelegateHandle SearchTicker;
	FString SearchOutputRemainder;
	FString LastSearchDetail;
	double SearchStartedAt = 0.0;
	int32 PackagesScanned = 0;
	int32 LastLoggedPackageCount = 0;
	bool bCommandletLoggedExitZero = false;

	void SetStatus(const FString& NewStatus);
	void ReportError(const FString& Error);
	void Finish(bool bSuccess, const FString& Summary);

	bool EnsureCultureOnTarget(ULocalizationTarget* Target, const FString& CultureCode);
	void BeginTextSearch();
	bool LaunchTextCommandlet(const FString& ConfigPath, bool bUseProjectFile);
	bool TickTextSearch(float DeltaSeconds);
	void RefreshSearchStatus();
	void IngestCommandletOutput(const FString& Chunk);
	void OnTextCommandletExited(int32 ReturnCode);
	bool StartExportAfterSearch();
	void ContinueAfterTextFileReady(const FString& NativePoPath);
	void StopTextSearch();
	bool LoadEntriesFromPo(const FString& PoPath, const FString& CulturePoPath);
	void TranslateNextBatch();
	bool ApplyTranslationsAndCompile();
	void RegisterGameLocalizationPath(const ULocalizationTarget* Target);

	static FString BuildSystemPrompt(const FString& LanguageDisplayName);
	static FString BuildBatchUserMessage(const TArray<FTranslationEntry>& Batch);
	static bool ParseBatchResponse(const FString& Response, TMap<int32, FString>& OutIdToText, FString& OutError);
	static bool TranslationPreservesPlaceholders(const FString& Source, const FString& Translation, FString& OutReason);
	static void CollectPlaceholders(const FString& Text, TArray<FString>& OutPlaceholders, TArray<FString>& OutTags);
	/** FPortableObjectFormatDOM::ToString does not escape quotes or newlines. */
	static FString EscapePoString(const FString& Text);
};
