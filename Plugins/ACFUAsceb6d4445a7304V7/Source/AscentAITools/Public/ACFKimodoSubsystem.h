// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ACFMotionImportLibrary.h"
#include "Containers/Ticker.h"
#include "EditorSubsystem.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "ACFKimodoSubsystem.generated.h"

/** What one Kimodo setup step does. */
UENUM()
enum class EACFKimodoStepKind : uint8
{
	/** Runs Executable with Arguments. */
	Run,
	/** Runs the Python 3.11 found on this computer, or the one setup installed, with Arguments. */
	RunBasePython,
	/** Runs the Kimodo venv Python with Arguments. */
	RunVenvPython,
	/** Downloads Url into Path. */
	Download,
	/** Deletes and recreates the Path folder. */
	ResetFolder,
	/** Installs the Kimodo source unpacked under Path into the venv. */
	InstallSource,
	/** Points the language model adapter at the local copy and writes the ready marker. */
	FinishEncoder,
	/** Writes Content into Path. */
	WriteFile,
	/** Fails unless kimodo_gen is in the venv. */
	CheckInstalled
};

/** One step of the Kimodo setup. The same steps, in the same order, as Yarin Studio's setup. */
USTRUCT()
struct FACFKimodoStep
{
	GENERATED_BODY()

	UPROPERTY()
	EACFKimodoStepKind Kind = EACFKimodoStepKind::Run;

	/** The status line shown while the step runs. */
	UPROPERTY()
	FString Status;

	UPROPERTY()
	FString Executable;

	UPROPERTY()
	FString Arguments;

	UPROPERTY()
	FString Url;

	UPROPERTY()
	FString Path;

	UPROPERTY()
	FString Content;

	UPROPERTY()
	double TimeoutSeconds = 600.0;

	/** Shows the language model download progress from the size of the text-encoders folder. */
	UPROPERTY()
	bool bTrackEncoderSize = false;
};

/**
 * Finds Kimodo on this computer, installs it into the folder the user chose after they confirm, and generates a motion
 * from a text prompt. The install is the same as Yarin Studio's, so either app can use a folder the other set up.
 * The user never types a command. The chat tool and the Yarin settings card both call this.
 */
UCLASS()
class ASCENTAITOOLS_API UACFKimodoSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** True when kimodo_gen can be launched and the language model is available. */
	UFUNCTION(BlueprintCallable, Category = "ACF", meta = (DisplayName = "Kimodo Is Ready", ToolTip = "True when Kimodo and its language model are installed in the chosen folder or found on this computer."))
	bool IsReady() const;

	/** True while a setup or motion generation is running. */
	UFUNCTION(BlueprintCallable, Category = "ACF", meta = (DisplayName = "Kimodo Is Busy", ToolTip = "True while Yarin is installing or running Kimodo."))
	bool IsBusy() const;

	/** True when the last setup or generation stopped with an error. */
	UFUNCTION(BlueprintCallable, Category = "ACF", meta = (DisplayName = "Kimodo Failed", ToolTip = "True when the last Kimodo setup or generation stopped with an error."))
	bool IsFailed() const;

	/** What the settings card shows: ready, the current step, or the last error. */
	UFUNCTION(BlueprintCallable, Category = "ACF", meta = (DisplayName = "Kimodo Status", ToolTip = "The Kimodo status line shown in Yarin settings."))
	FText GetStatus() const;

	/** The kimodo_gen that will run, or empty when none was found. */
	UFUNCTION(BlueprintCallable, Category = "ACF", meta = (DisplayName = "Kimodo Executable Path", ToolTip = "Full path of the Kimodo program Yarin will run."))
	FString GetExecutable() const;

	/** Everything setup and generation printed, for the log window. Setup output is also kept in <Folder>/setup.log. */
	UFUNCTION(BlueprintCallable, Category = "ACF", meta = (DisplayName = "Kimodo Log", ToolTip = "The full Kimodo setup and generation log."))
	FString GetLog() const;

	/** The folder chosen for the install, or empty when none was chosen yet. */
	UFUNCTION(BlueprintPure, Category = "ACF", meta = (DisplayName = "Kimodo Install Folder", ToolTip = "Folder where Kimodo, PyTorch and the models are installed. Empty until the user chooses one."))
	FString GetInstallFolder() const;

	/** True when Folder holds a Kimodo install (venv with kimodo_gen), made by Yarin, Yarin Studio or by hand. */
	UFUNCTION(BlueprintPure, Category = "ACF", meta = (DisplayName = "Is Kimodo Folder", ToolTip = "True when the folder contains a Kimodo install: venv with kimodo_gen."))
	bool IsKimodoFolder(const FString& Folder) const;

	/** The install a picked folder points to: the folder itself, its parent when a subfolder such as venv was picked, or its Kimodo subfolder. */
	UFUNCTION(BlueprintPure, Category = "ACF", meta = (DisplayName = "Existing Kimodo Folder For Pick", ToolTip = "Finds the Kimodo install the user meant: the picked folder, its parent when they picked venv or hf, or a Kimodo subfolder."))
	FString GetExistingFolderForPick(const FString& PickedFolder) const;

	/** The install folder for a folder the user picked: a Kimodo subfolder, unless the picked folder already is one. */
	UFUNCTION(BlueprintPure, Category = "ACF", meta = (DisplayName = "Kimodo Folder For Pick", ToolTip = "Turns a picked folder into the Kimodo install folder: adds a Kimodo subfolder unless the folder is already named Kimodo."))
	FString GetFolderForPick(const FString& PickedFolder) const;

	/** True when Download has something to install, so the user must confirm the Download Summary first. */
	UFUNCTION(BlueprintPure, Category = "ACF", meta = (DisplayName = "Kimodo Needs Download", ToolTip = "True when Download Kimodo would download something. Show the Download Summary first."))
	bool NeedsDownload() const;

	/** What Download will fetch, where, and the free space there. Show it to the user before calling Download. */
	UFUNCTION(BlueprintPure, Category = "ACF", meta = (DisplayName = "Kimodo Download Summary", ToolTip = "Size, folder and free space for Download Kimodo. Ask the user to confirm it first."))
	FText GetDownloadSummary() const;

	/** Stops the running setup or generation. */
	UFUNCTION(BlueprintCallable, Category = "ACF", meta = (DisplayName = "Cancel Kimodo", ToolTip = "Stops the Kimodo setup or generation that is running."))
	void Cancel();

	/** Looks for Kimodo again in the folder, the executable setting and PATH. Does nothing while work is running. */
	UFUNCTION(BlueprintCallable, Category = "ACF", meta = (DisplayName = "Look for Kimodo", ToolTip = "Searches the install folder and this computer for Kimodo again."))
	void Refresh();

	/** Installs whatever is missing into the Install Folder. Call it only after the user confirmed the Download Summary. */
	UFUNCTION(BlueprintCallable, Category = "ACF", meta = (DisplayName = "Download Kimodo", ToolTip = "Installs Kimodo into the chosen Install Folder. Confirm the Download Summary with the user first."))
	void Download();

	/**
	 * Generates a SOMA motion and imports it retargeted onto TargetMesh (empty uses SKM_Manny). Never installs anything:
	 * fails when Kimodo is not installed. Completion runs on the editor thread with the import result, an error, and the BVH path.
	 */
	void GenerateMotion(const FString& Prompt, float DurationSeconds, const FString& AssetName, const FString& TargetMesh,
		TFunction<void(const FACFMotionImportResult& Result, const FString& Error, const FString& BvhFile)> Completion);

private:
	enum class EPhase : uint8
	{
		Missing,
		Ready,
		Installing,
		Generating,
		Failed
	};

	FString InstallRoot() const;
	FString HfHome() const;
	FString EncoderDirectory() const;
	FString VenvPython() const;
	FString BundledKimodo() const;
	FString MotionModelMarker() const;
	FString MotionModelRepo() const;
	bool IsWindows() const;
	bool IsEncoderReady() const;
	FString SavedSignIn() const;
	bool CanEncode() const;
	bool HasMotionCorrection() const;
	FString LocateExecutable() const;
	FString FindOnPath(const TArray<FString>& FileNames) const;
	FString FindPython() const;
	int32 VramMb() const;
	int64 FreeGb(const FString& Folder) const;
	FString FolderProblem(const FString& Folder) const;
	FString NativePath(const FString& Path) const;
	FString Quote(const FString& Value) const;
	FString AnimationName(const FString& Prompt) const;
	void ApplySetupEnvironment() const;
	void ApplyGenerateEnvironment() const;
	void LogSetup(const FString& Text);
	void AppendLog(const FString& Text);
	void SetPhase(EPhase NewPhase, const FString& Sentence);
	void Fail(const FString& Message);
	void Complete(const FACFMotionImportResult& Result, const FString& Error, const FString& BvhFile);
	void PlanSetup(bool bInstalled, bool bEncodes);
	void AddStep(EACFKimodoStepKind Kind, const FString& Status, double TimeoutSeconds);
	void RunStep();
	void NextStep();
	void BeginDownload(const FString& Url, const FString& File);
	void OnDownloadComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSucceeded, FString File);
	bool FinishEncoder(FString& Error) const;
	void StartProcess(const FString& Executable, const FString& Arguments, const FString& Directory, double TimeoutSeconds);
	void ClosePipes();
	void PullPipes();
	void ArmTicker();
	void TickWork();
	void TrackEncoderSize();
	void BeginGenerate();
	void FinishGenerate();

	EPhase Phase = EPhase::Missing;
	FString StatusText;
	FString StepStatus;
	FString ProcessLog;
	FString FullLog;
	FString ResolvedExecutable;
	FString BasePython;
	FString PendingPrompt;
	FString PendingAssetName;
	FString PendingTargetMesh;
	FString PendingOutputDirectory;
	float PendingDuration = 5.0f;

	UPROPERTY(Transient)
	TArray<FACFKimodoStep> Steps;

	int32 StepIndex = 0;
	TFunction<void(const FACFMotionImportResult& Result, const FString& Error, const FString& BvhFile)> MotionDone;
	TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> ActiveRequest;
	FProcHandle Process;
	void* StdOutRead = nullptr;
	void* StdOutWrite = nullptr;
	void* StdErrRead = nullptr;
	void* StdErrWrite = nullptr;
	double ProcessStartedAt = 0.0;
	double ProcessTimeout = 600.0;
	double LastSizeCheck = 0.0;
	FTSTicker::FDelegateHandle TickerHandle;
};
