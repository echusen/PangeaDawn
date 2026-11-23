// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "ACFKimodoSubsystem.h"

#include "AscentAIToolsSettings.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "HttpModule.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

DEFINE_LOG_CATEGORY_STATIC(LogACFKimodo, Log, All);

void UACFKimodoSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Refresh();
}

void UACFKimodoSubsystem::Deinitialize()
{
	if (ActiveRequest.IsValid())
	{
		ActiveRequest->OnProcessRequestComplete().Unbind();
		ActiveRequest->CancelRequest();
		ActiveRequest.Reset();
	}
	if (Process.IsValid())
	{
		FPlatformProcess::TerminateProc(Process, true);
		FPlatformProcess::CloseProc(Process);
	}
	ClosePipes();
	if (TickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
		TickerHandle.Reset();
	}
	Complete(FACFMotionImportResult(), TEXT("Kimodo stopped because the editor is shutting down."), FString());
	Super::Deinitialize();
}

bool UACFKimodoSubsystem::IsReady() const
{
	return Phase == EPhase::Ready && !ResolvedExecutable.IsEmpty() && FPaths::FileExists(ResolvedExecutable);
}

bool UACFKimodoSubsystem::IsBusy() const
{
	return Phase == EPhase::Installing || Phase == EPhase::Generating;
}

bool UACFKimodoSubsystem::IsFailed() const
{
	return Phase == EPhase::Failed;
}

FText UACFKimodoSubsystem::GetStatus() const
{
	return FText::FromString(StatusText);
}

FString UACFKimodoSubsystem::GetExecutable() const
{
	return ResolvedExecutable;
}

FString UACFKimodoSubsystem::GetLog() const
{
	return FullLog;
}

FString UACFKimodoSubsystem::GetInstallFolder() const
{
	return InstallRoot();
}

bool UACFKimodoSubsystem::IsKimodoFolder(const FString& Folder) const
{
	return !Folder.IsEmpty()
		&& (FPaths::FileExists(Folder / TEXT("venv/Scripts/kimodo_gen.exe")) || FPaths::FileExists(Folder / TEXT("venv/bin/kimodo_gen")));
}

FString UACFKimodoSubsystem::GetExistingFolderForPick(const FString& PickedFolder) const
{
	FString Folder = PickedFolder.TrimStartAndEnd().TrimQuotes();
	FPaths::NormalizeDirectoryName(Folder);
	if (Folder.IsEmpty() || IsKimodoFolder(Folder))
	{
		return Folder;
	}
	const FString Parent = FPaths::GetPath(Folder);
	if (IsKimodoFolder(Parent))
	{
		return Parent;
	}
	if (IsKimodoFolder(Folder / TEXT("Kimodo")))
	{
		return Folder / TEXT("Kimodo");
	}
	return Folder;
}

FString UACFKimodoSubsystem::GetFolderForPick(const FString& PickedFolder) const
{
	const FString Existing = GetExistingFolderForPick(PickedFolder);
	if (Existing.IsEmpty() || IsKimodoFolder(Existing) || FPaths::DirectoryExists(Existing / TEXT("venv")))
	{
		return Existing;
	}
	return FPaths::GetCleanFilename(Existing).Equals(TEXT("Kimodo"), ESearchCase::IgnoreCase) ? Existing : Existing / TEXT("Kimodo");
}

bool UACFKimodoSubsystem::NeedsDownload() const
{
	return LocateExecutable().IsEmpty() || !CanEncode();
}

FText UACFKimodoSubsystem::GetDownloadSummary() const
{
	const FString Root = InstallRoot();
	if (Root.IsEmpty())
	{
		return FText::FromString(TEXT("Choose where to install Kimodo first."));
	}
	const int64 Free = FreeGb(Root);
	return FText::FromString(FString::Printf(
		TEXT("Kimodo will download about 25 GB into this folder:\n\n%s\n\nFree space on that drive: %s\n\nSetup can take an hour or more. Keep Unreal open until it finishes. Start now?"),
		*NativePath(Root), Free >= 0 ? *FString::Printf(TEXT("%lld GB"), Free) : TEXT("unknown")));
}

void UACFKimodoSubsystem::Cancel()
{
	if (!IsBusy())
	{
		return;
	}
	const bool bWasGenerating = Phase == EPhase::Generating;
	if (ActiveRequest.IsValid())
	{
		ActiveRequest->OnProcessRequestComplete().Unbind();
		ActiveRequest->CancelRequest();
		ActiveRequest.Reset();
	}
	if (Process.IsValid())
	{
		FPlatformProcess::TerminateProc(Process, true);
		FPlatformProcess::CloseProc(Process);
	}
	ClosePipes();
	Steps.Reset();
	if (!bWasGenerating)
	{
		LogSetup(TEXT("setup cancelled"));
	}
	SetPhase(EPhase::Missing, TEXT("Cancelled"));
	if (bWasGenerating)
	{
		Refresh();
	}
	Complete(FACFMotionImportResult(), TEXT("Kimodo was cancelled by the user."), FString());
}

void UACFKimodoSubsystem::Refresh()
{
	if (IsBusy())
	{
		return;
	}
	ResolvedExecutable = LocateExecutable();
	if (ResolvedExecutable.IsEmpty())
	{
		SetPhase(EPhase::Missing, InstallRoot().IsEmpty() ? TEXT("Not set up") : TEXT("No Kimodo in this folder"));
		return;
	}
	if (!CanEncode())
	{
		SetPhase(EPhase::Missing, TEXT("Setup not finished, press Download to complete it"));
		return;
	}
	SetPhase(EPhase::Ready, TEXT("Ready"));
}

void UACFKimodoSubsystem::Download()
{
	if (IsBusy())
	{
		return;
	}
	const FString Root = InstallRoot();
	const bool bInstalled = !LocateExecutable().IsEmpty();
	if (Root.IsEmpty() && !(bInstalled && CanEncode()))
	{
		SetPhase(EPhase::Missing, TEXT("Choose where to install Kimodo first."));
		return;
	}
	if (!bInstalled)
	{
		const FString Problem = FolderProblem(Root);
		if (!Problem.IsEmpty())
		{
			Fail(Problem);
			return;
		}
	}
	const int32 Vram = VramMb();
	if (Vram <= 0)
	{
		Fail(TEXT("This computer needs an NVIDIA graphics card for Kimodo."));
		return;
	}
	const bool bSmallCard = Vram < 16000;
	if (bSmallCard)
	{
		if (UACFAIToolsSettings* Settings = GetMutableDefault<UACFAIToolsSettings>())
		{
			Settings->bKimodoTextEncoderOnCpu = true;
			Settings->SaveConfig();
		}
	}
	const bool bEncodes = CanEncode();
	if (bInstalled && bEncodes)
	{
		Refresh();
		return;
	}
	SetPhase(EPhase::Installing, TEXT("Checking this computer..."));
	AppendLog(FString::Printf(TEXT("Found an NVIDIA graphics card with %d GB.%s\n"), FMath::RoundToInt(Vram / 1024.0f),
		bSmallCard ? TEXT(" The language model will use the processor, which is slower.") : TEXT("")));
	LogSetup(FString::Printf(TEXT("setup start, vram %d MB, encoder %s, installed %s, encodes %s"), Vram,
		bSmallCard ? TEXT("cpu") : TEXT("gpu"), bInstalled ? TEXT("true") : TEXT("false"), bEncodes ? TEXT("true") : TEXT("false")));
	PlanSetup(bInstalled, bEncodes);
}

void UACFKimodoSubsystem::GenerateMotion(const FString& Prompt, float DurationSeconds, const FString& AssetName, const FString& TargetMesh,
	TFunction<void(const FACFMotionImportResult& Result, const FString& Error, const FString& BvhFile)> Completion)
{
	if (MotionDone)
	{
		Completion(FACFMotionImportResult(), TEXT("A Kimodo motion is already running."), FString());
		return;
	}
	if (IsBusy())
	{
		Completion(FACFMotionImportResult(), TEXT("Kimodo is still installing. Try again when Yarin settings > Motion (Kimodo) says Ready."), FString());
		return;
	}
	if (!IsReady())
	{
		Refresh();
	}
	if (!IsReady())
	{
		Completion(FACFMotionImportResult(), TEXT("Kimodo is not installed. Ask the user to open Yarin settings > Motion (Kimodo), choose a folder and press Download. Do not install anything yourself."), FString());
		return;
	}
	PendingPrompt = Prompt;
	PendingDuration = DurationSeconds;
	PendingAssetName = AssetName;
	PendingTargetMesh = TargetMesh;
	MotionDone = MoveTemp(Completion);
	BeginGenerate();
}

FString UACFKimodoSubsystem::InstallRoot() const
{
	const UACFAIToolsSettings* Settings = UACFAIToolsSettings::Get();
	FString Folder = Settings ? Settings->KimodoInstallFolder.TrimStartAndEnd().TrimQuotes() : FString();
	if (Folder.IsEmpty())
	{
		return Folder;
	}
	Folder = FPaths::ConvertRelativePathToFull(Folder);
	FPaths::NormalizeDirectoryName(Folder);
	return Folder;
}

FString UACFKimodoSubsystem::HfHome() const
{
	return InstallRoot() / TEXT("hf");
}

FString UACFKimodoSubsystem::EncoderDirectory() const
{
	return InstallRoot() / TEXT("text-encoders");
}

FString UACFKimodoSubsystem::VenvPython() const
{
	return InstallRoot() / (IsWindows() ? TEXT("venv/Scripts/python.exe") : TEXT("venv/bin/python"));
}

FString UACFKimodoSubsystem::BundledKimodo() const
{
	return InstallRoot() / (IsWindows() ? TEXT("venv/Scripts/kimodo_gen.exe") : TEXT("venv/bin/kimodo_gen"));
}

FString UACFKimodoSubsystem::MotionModelMarker() const
{
	return HfHome() / TEXT("motion-model-ready");
}

FString UACFKimodoSubsystem::MotionModelRepo() const
{
	const UACFAIToolsSettings* Settings = UACFAIToolsSettings::Get();
	const FString Model = Settings ? Settings->ResolvedKimodoModel() : FString(TEXT("Kimodo-SOMA-RP-v1.2"));
	return Model.Contains(TEXT("/")) ? Model : TEXT("nvidia/") + Model;
}

bool UACFKimodoSubsystem::IsWindows() const
{
	return FCStringAnsi::Strcmp(FPlatformProperties::IniPlatformName(), "Windows") == 0;
}

bool UACFKimodoSubsystem::IsEncoderReady() const
{
	return !InstallRoot().IsEmpty() && FPaths::FileExists(EncoderDirectory() / TEXT("ready"));
}

FString UACFKimodoSubsystem::SavedSignIn() const
{
	TArray<FString> Files;
	if (!InstallRoot().IsEmpty())
	{
		Files.Add(HfHome() / TEXT("token"));
	}
	Files.Add(FPaths::Combine(FPlatformProcess::UserHomeDir(), TEXT(".cache/huggingface/token")));
	for (const FString& File : Files)
	{
		FString Token;
		if (FFileHelper::LoadFileToString(Token, *File))
		{
			Token.TrimStartAndEndInline();
			if (!Token.IsEmpty())
			{
				return Token;
			}
		}
	}
	return FString();
}

bool UACFKimodoSubsystem::CanEncode() const
{
	return IsEncoderReady() || !SavedSignIn().IsEmpty();
}

bool UACFKimodoSubsystem::HasMotionCorrection() const
{
	// Yarin's setup skips the correction (a C++ extension); a folder set up by Yarin Studio may have it.
	return FPaths::DirectoryExists(InstallRoot() / TEXT("venv/Lib/site-packages/motion_correction"))
		|| FPaths::DirectoryExists(InstallRoot() / TEXT("venv/lib/python3.11/site-packages/motion_correction"));
}

FString UACFKimodoSubsystem::LocateExecutable() const
{
	const UACFAIToolsSettings* Settings = UACFAIToolsSettings::Get();
	const FString Override = Settings ? Settings->KimodoExecutable.TrimStartAndEnd().TrimQuotes() : FString();
	if (!Override.IsEmpty() && FPaths::FileExists(Override))
	{
		return FPaths::ConvertRelativePathToFull(Override);
	}
	if (!InstallRoot().IsEmpty() && FPaths::FileExists(BundledKimodo()))
	{
		return FPaths::ConvertRelativePathToFull(BundledKimodo());
	}
	return FindOnPath({TEXT("kimodo_gen.exe"), TEXT("kimodo_gen")});
}

FString UACFKimodoSubsystem::FindOnPath(const TArray<FString>& FileNames) const
{
	TArray<FString> Directories;
	FPlatformMisc::GetEnvironmentVariable(TEXT("PATH")).ParseIntoArray(Directories, FPlatformMisc::GetPathVarDelimiter(), true);
	for (const FString& Directory : Directories)
	{
		for (const FString& FileName : FileNames)
		{
			const FString Candidate = FPaths::Combine(Directory.TrimQuotes(), FileName);
			if (FPaths::FileExists(Candidate))
			{
				return FPaths::ConvertRelativePathToFull(Candidate);
			}
		}
	}
	return FString();
}

FString UACFKimodoSubsystem::FindPython() const
{
	// 3.11 only: the prebuilt motion correction wheel is built for it.
	TArray<TPair<FString, FString>> Candidates;
	if (IsWindows())
	{
		Candidates.Emplace(FindOnPath({TEXT("py.exe")}), TEXT("-3.11 "));
		Candidates.Emplace(FindOnPath({TEXT("python.exe")}), FString());
	}
	else
	{
		Candidates.Emplace(FindOnPath({TEXT("python3")}), FString());
		Candidates.Emplace(FindOnPath({TEXT("python")}), FString());
	}
	for (const TPair<FString, FString>& Candidate : Candidates)
	{
		if (Candidate.Key.IsEmpty())
		{
			continue;
		}
		int32 Code = -1;
		FString Out;
		FString Err;
		const FString Arguments = Candidate.Value + TEXT("-c \"import sys; assert sys.version_info[:2] == (3, 11); print(sys.executable)\"");
		if (!FPlatformProcess::ExecProcess(*Candidate.Key, *Arguments, &Code, &Out, &Err) || Code != 0)
		{
			continue;
		}
		TArray<FString> Lines;
		Out.ParseIntoArrayLines(Lines);
		for (int32 Line = Lines.Num() - 1; Line >= 0; --Line)
		{
			const FString Path = Lines[Line].TrimStartAndEnd();
			if (!Path.IsEmpty() && FPaths::FileExists(Path))
			{
				return Path;
			}
		}
	}
	return FString();
}

int32 UACFKimodoSubsystem::VramMb() const
{
	const FString Smi = FindOnPath({TEXT("nvidia-smi.exe"), TEXT("nvidia-smi")});
	if (Smi.IsEmpty())
	{
		return 0;
	}
	int32 Code = -1;
	FString Out;
	FString Err;
	if (!FPlatformProcess::ExecProcess(*Smi, TEXT("--query-gpu=memory.total --format=csv,noheader,nounits"), &Code, &Out, &Err) || Code != 0)
	{
		return 0;
	}
	TArray<FString> Lines;
	Out.ParseIntoArrayLines(Lines);
	int32 Best = 0;
	for (const FString& Line : Lines)
	{
		Best = FMath::Max(Best, FCString::Atoi(*Line.TrimStartAndEnd()));
	}
	return Best;
}

int64 UACFKimodoSubsystem::FreeGb(const FString& Folder) const
{
	FString Probe = FPaths::ConvertRelativePathToFull(Folder);
	while (!Probe.IsEmpty())
	{
		if (FPaths::DirectoryExists(Probe))
		{
			uint64 Total = 0;
			uint64 Free = 0;
			return FPlatformMisc::GetDiskTotalAndFreeSpace(Probe, Total, Free) ? int64(Free / (1024ull * 1024ull * 1024ull)) : -1;
		}
		const FString Parent = FPaths::GetPath(Probe);
		if (Parent == Probe)
		{
			break;
		}
		Probe = Parent;
	}
	return -1;
}

FString UACFKimodoSubsystem::FolderProblem(const FString& Folder) const
{
	const bool bResumed = FPaths::DirectoryExists(Folder / TEXT("venv"));
	const int64 Free = FreeGb(Folder);
	if (!bResumed && Free >= 0 && Free < 25)
	{
		return TEXT("There is not enough free space there. Kimodo needs about 25 GB. Choose a folder on another drive.");
	}
	const FString Probe = Folder / TEXT(".write-test");
	if (!IFileManager::Get().MakeDirectory(*Folder, true) || !FFileHelper::SaveStringToFile(FString(), *Probe))
	{
		return TEXT("Unreal cannot write to that folder. Choose another one.");
	}
	IFileManager::Get().Delete(*Probe);
	return FString();
}

FString UACFKimodoSubsystem::NativePath(const FString& Path) const
{
	FString Full = FPaths::ConvertRelativePathToFull(Path);
	if (IsWindows())
	{
		Full.ReplaceInline(TEXT("/"), TEXT("\\"));
	}
	return Full;
}

FString UACFKimodoSubsystem::Quote(const FString& Value) const
{
	FString Clean = Value;
	Clean.ReplaceInline(TEXT("\r"), TEXT(" "));
	Clean.ReplaceInline(TEXT("\n"), TEXT(" "));
	Clean.ReplaceInline(TEXT("\""), TEXT("'"));
	return TEXT("\"") + Clean + TEXT("\"");
}

FString UACFKimodoSubsystem::AnimationName(const FString& Prompt) const
{
	// Same rule as Yarin Studio: the first four words, each capitalized.
	FString Spaced;
	for (const TCHAR Character : Prompt)
	{
		Spaced.AppendChar(FChar::IsAlnum(Character) && Character < 128 ? Character : TEXT(' '));
	}
	TArray<FString> Words;
	Spaced.ParseIntoArrayWS(Words);
	FString Name;
	for (int32 Word = 0; Word < Words.Num() && Word < 4; ++Word)
	{
		Name += Words[Word].Left(1).ToUpper() + Words[Word].RightChop(1).ToLower();
	}
	return Name.Len() > 0 && FChar::IsAlpha(Name[0]) ? Name.Left(40) : FString(TEXT("Motion"));
}

void UACFKimodoSubsystem::ApplySetupEnvironment() const
{
	// Downloads stay in the chosen folder: no pip cache in the user profile.
	FPlatformMisc::SetEnvironmentVar(TEXT("PIP_NO_CACHE_DIR"), TEXT("1"));
	FPlatformMisc::SetEnvironmentVar(TEXT("HF_HOME"), *HfHome());
	FPlatformMisc::SetEnvironmentVar(TEXT("HF_HUB_DISABLE_PROGRESS_BARS"), TEXT("1"));
	// The correction is a C++ extension that needs CMake; setup installs the prebuilt wheel instead.
	FPlatformMisc::SetEnvironmentVar(TEXT("SKIP_MOTION_CORRECTION_IN_SETUP"), TEXT("1"));
}

void UACFKimodoSubsystem::ApplyGenerateEnvironment() const
{
	FPlatformMisc::SetEnvironmentVar(TEXT("TEXT_ENCODER_MODE"), TEXT("local"));
	if (!InstallRoot().IsEmpty())
	{
		FPlatformMisc::SetEnvironmentVar(TEXT("HF_HOME"), *HfHome());
	}
	if (IsEncoderReady())
	{
		FPlatformMisc::SetEnvironmentVar(TEXT("TEXT_ENCODERS_DIR"), *EncoderDirectory());
	}
	else
	{
		const FString Token = SavedSignIn();
		if (!Token.IsEmpty())
		{
			FPlatformMisc::SetEnvironmentVar(TEXT("HF_TOKEN"), *Token);
			FPlatformMisc::SetEnvironmentVar(TEXT("HUGGING_FACE_HUB_TOKEN"), *Token);
		}
	}
	const UACFAIToolsSettings* Settings = UACFAIToolsSettings::Get();
	FPlatformMisc::SetEnvironmentVar(TEXT("TEXT_ENCODER_DEVICE"), Settings && Settings->bKimodoTextEncoderOnCpu ? TEXT("cpu") : TEXT("cuda"));
}

void UACFKimodoSubsystem::LogSetup(const FString& Text)
{
	AppendLog(Text + TEXT("\n"));
	if (InstallRoot().IsEmpty())
	{
		return;
	}
	const FString Line = FString::Printf(TEXT("[%s] %s\n"), *FDateTime::UtcNow().ToIso8601(), *Text);
	FFileHelper::SaveStringToFile(Line, *(InstallRoot() / TEXT("setup.log")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append);
}

void UACFKimodoSubsystem::AppendLog(const FString& Text)
{
	FullLog += Text;
	if (FullLog.Len() > 200000)
	{
		FullLog.RightChopInline(FullLog.Len() - 200000);
	}
}

void UACFKimodoSubsystem::SetPhase(EPhase NewPhase, const FString& Sentence)
{
	Phase = NewPhase;
	StepStatus = Sentence;
	StatusText = Sentence;
	AppendLog(FString::Printf(TEXT("\n== %s ==\n"), *Sentence));
}

void UACFKimodoSubsystem::Fail(const FString& Message)
{
	UE_LOG(LogACFKimodo, Warning, TEXT("%s"), *Message);
	const bool bSetup = Phase == EPhase::Installing;
	Steps.Reset();
	if (bSetup)
	{
		LogSetup(TEXT("setup failed: ") + Message);
	}
	else
	{
		AppendLog(Message + TEXT("\n"));
	}
	SetPhase(EPhase::Failed, TEXT("Failed, see Log"));
	Complete(FACFMotionImportResult(), Message, FString());
}

void UACFKimodoSubsystem::Complete(const FACFMotionImportResult& Result, const FString& Error, const FString& BvhFile)
{
	TFunction<void(const FACFMotionImportResult&, const FString&, const FString&)> Done = MoveTemp(MotionDone);
	if (Done)
	{
		Done(Result, Error, BvhFile);
	}
}

void UACFKimodoSubsystem::AddStep(EACFKimodoStepKind Kind, const FString& Status, double TimeoutSeconds)
{
	FACFKimodoStep& Step = Steps.AddDefaulted_GetRef();
	Step.Kind = Kind;
	Step.Status = Status;
	Step.TimeoutSeconds = TimeoutSeconds;
}

void UACFKimodoSubsystem::PlanSetup(bool bInstalled, bool bEncodes)
{
	const FString Root = InstallRoot();
	Steps.Reset();
	StepIndex = 0;
	IFileManager::Get().MakeDirectory(*Root, true);

	if (!bInstalled)
	{
		BasePython = FindPython();
		if (!BasePython.IsEmpty())
		{
			AppendLog(TEXT("Python, which Kimodo runs on, is already on this PC: ") + BasePython + TEXT("\n"));
		}
		else if (IsWindows())
		{
			const FString Target = Root / TEXT("python");
			if (!FPaths::FileExists(Target / TEXT("python.exe")))
			{
				AddStep(EACFKimodoStepKind::Download, TEXT("Downloading Python, which Kimodo runs on."), 600.0);
				Steps.Last().Url = TEXT("https://www.python.org/ftp/python/3.11.9/python-3.11.9-amd64.exe");
				Steps.Last().Path = Root / TEXT("python-setup.exe");
				AddStep(EACFKimodoStepKind::Run, TEXT("Installing Python."), 600.0);
				Steps.Last().Executable = Root / TEXT("python-setup.exe");
				Steps.Last().Arguments = FString::Printf(TEXT("/quiet InstallAllUsers=0 PrependPath=0 Include_pip=1 Include_test=0 TargetDir=\"%s\""), *NativePath(Target));
			}
			BasePython = Target / TEXT("python.exe");
		}
		else
		{
			Fail(TEXT("Kimodo needs Python 3.11. Install it, then press Download again."));
			return;
		}
		if (!FPaths::FileExists(VenvPython()))
		{
			AddStep(EACFKimodoStepKind::RunBasePython, TEXT("Making a private space for Kimodo."), 180.0);
			Steps.Last().Arguments = FString::Printf(TEXT("-m venv \"%s\""), *NativePath(Root / TEXT("venv")));
		}
		AddStep(EACFKimodoStepKind::RunVenvPython, TEXT("Updating the installer."), 300.0);
		Steps.Last().Arguments = TEXT("-m pip install --upgrade pip");
		// cu128 covers every RTX card from the 20 series to the 50 series (sm_120), which cu124 builds lack.
		AddStep(EACFKimodoStepKind::RunVenvPython, TEXT("Downloading the graphics card libraries, about 3 GB. This can take a while."), 2700.0);
		Steps.Last().Arguments = TEXT("-m pip install torch --index-url https://download.pytorch.org/whl/cu128");
		AddStep(EACFKimodoStepKind::ResetFolder, TEXT("Downloading Kimodo."), 60.0);
		Steps.Last().Path = Root / TEXT("src");
		AddStep(EACFKimodoStepKind::Download, TEXT("Downloading Kimodo."), 600.0);
		Steps.Last().Url = TEXT("https://github.com/nv-tlabs/kimodo/archive/refs/heads/main.zip");
		Steps.Last().Path = Root / TEXT("src/kimodo.zip");
		const FString Tar = FindOnPath({TEXT("tar.exe"), TEXT("tar")});
		AddStep(EACFKimodoStepKind::Run, TEXT("Downloading Kimodo."), 120.0);
		Steps.Last().Executable = Tar.IsEmpty() ? FPaths::Combine(FPlatformMisc::GetEnvironmentVariable(TEXT("SystemRoot")), TEXT("System32/tar.exe")) : Tar;
		// -m: exFAT drives cannot restore the zip timestamps, which fails the whole extraction.
		Steps.Last().Arguments = FString::Printf(TEXT("-xmf \"%s\" -C \"%s\""), *NativePath(Root / TEXT("src/kimodo.zip")), *NativePath(Root / TEXT("src")));
		AddStep(EACFKimodoStepKind::InstallSource, TEXT("Installing Kimodo."), 1200.0);
		Steps.Last().Path = Root / TEXT("src");
		LogSetup(TEXT("no motion correction wheel; generating without foot-skating correction"));
		AddStep(EACFKimodoStepKind::CheckInstalled, TEXT("Installing Kimodo."), 60.0);
	}

	if (!bEncodes)
	{
		// NousResearch is an ungated copy of meta-llama/Meta-Llama-3-8B-Instruct, so no Hugging Face account is needed.
		const TArray<TPair<FString, FString>> Encoders = {
			{TEXT("NousResearch/Meta-Llama-3-8B-Instruct"), TEXT("Meta-Llama-3-8B-Instruct")},
			{TEXT("McGill-NLP/LLM2Vec-Meta-Llama-3-8B-Instruct-mntp"), TEXT("McGill-NLP/LLM2Vec-Meta-Llama-3-8B-Instruct-mntp")},
			{TEXT("McGill-NLP/LLM2Vec-Meta-Llama-3-8B-Instruct-mntp-supervised"), TEXT("McGill-NLP/LLM2Vec-Meta-Llama-3-8B-Instruct-mntp-supervised")}
		};
		for (const TPair<FString, FString>& Encoder : Encoders)
		{
			AddStep(EACFKimodoStepKind::RunVenvPython, TEXT("Downloading the language model, about 16 GB."), 14400.0);
			Steps.Last().bTrackEncoderSize = true;
			Steps.Last().Arguments = FString::Printf(
				TEXT("-c \"from huggingface_hub import snapshot_download; snapshot_download('%s', local_dir=r'%s', ignore_patterns=['original/*'])\""),
				*Encoder.Key, *NativePath(EncoderDirectory() / Encoder.Value));
		}
		AddStep(EACFKimodoStepKind::FinishEncoder, TEXT("Finishing the language model."), 60.0);
	}

	if (!FPaths::FileExists(MotionModelMarker()))
	{
		AddStep(EACFKimodoStepKind::RunVenvPython, TEXT("Downloading the motion model, about 1 GB."), 3600.0);
		Steps.Last().Arguments = FString::Printf(TEXT("-c \"from huggingface_hub import snapshot_download; snapshot_download('%s')\""), *MotionModelRepo());
		AddStep(EACFKimodoStepKind::WriteFile, TEXT("Downloading the motion model, about 1 GB."), 60.0);
		Steps.Last().Path = MotionModelMarker();
		Steps.Last().Content = MotionModelRepo();
	}

	RunStep();
}

void UACFKimodoSubsystem::NextStep()
{
	++StepIndex;
	RunStep();
}

void UACFKimodoSubsystem::RunStep()
{
	const FString Root = InstallRoot();
	while (Steps.IsValidIndex(StepIndex))
	{
		const FACFKimodoStep Step = Steps[StepIndex];
		if (Step.Status != StepStatus)
		{
			StepStatus = Step.Status;
			StatusText = Step.Status;
			AppendLog(FString::Printf(TEXT("\n== %s ==\n"), *Step.Status));
		}
		switch (Step.Kind)
		{
		case EACFKimodoStepKind::Run:
			StartProcess(Step.Executable, Step.Arguments, Root, Step.TimeoutSeconds);
			return;
		case EACFKimodoStepKind::RunBasePython:
			StartProcess(BasePython, Step.Arguments, Root, Step.TimeoutSeconds);
			return;
		case EACFKimodoStepKind::RunVenvPython:
		{
			// Kimodo found on PATH instead of in the folder: use the Python next to its kimodo_gen.
			FString Python = VenvPython();
			if (!FPaths::FileExists(Python) && !ResolvedExecutable.IsEmpty())
			{
				Python = FPaths::GetPath(ResolvedExecutable) / FPaths::GetCleanFilename(Python);
			}
			StartProcess(Python, Step.Arguments, Root, Step.TimeoutSeconds);
			return;
		}
		case EACFKimodoStepKind::Download:
			BeginDownload(Step.Url, Step.Path);
			return;
		case EACFKimodoStepKind::ResetFolder:
			IFileManager::Get().DeleteDirectory(*Step.Path, false, true);
			IFileManager::Get().MakeDirectory(*Step.Path, true);
			break;
		case EACFKimodoStepKind::InstallSource:
		{
			TArray<FString> Children;
			IFileManager::Get().FindFiles(Children, *(Step.Path / TEXT("*")), false, true);
			const FString* Folder = Children.FindByPredicate([&Step](const FString& Child)
			{
				return FPaths::FileExists(Step.Path / Child / TEXT("pyproject.toml"));
			});
			if (!Folder)
			{
				Fail(TEXT("The Kimodo download did not contain the project. Press Retry."));
				return;
			}
			StartProcess(VenvPython(), FString::Printf(TEXT("-m pip install \"%s\""), *NativePath(Step.Path / *Folder)), Root, Step.TimeoutSeconds);
			return;
		}
		case EACFKimodoStepKind::FinishEncoder:
		{
			FString Error;
			if (!FinishEncoder(Error))
			{
				Fail(Error);
				return;
			}
			break;
		}
		case EACFKimodoStepKind::WriteFile:
			if (!FFileHelper::SaveStringToFile(Step.Content, *Step.Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
			{
				Fail(FString::Printf(TEXT("Could not write %s."), *NativePath(Step.Path)));
				return;
			}
			break;
		case EACFKimodoStepKind::CheckInstalled:
			if (!FPaths::FileExists(BundledKimodo()))
			{
				Fail(TEXT("Kimodo was installed but kimodo_gen was not created. See the Log."));
				return;
			}
			break;
		}
		++StepIndex;
	}

	Steps.Reset();
	LogSetup(TEXT("setup done"));
	Phase = EPhase::Missing;
	Refresh();
	if (!IsReady())
	{
		Fail(TEXT("Setup finished but Kimodo is not ready. See the Log."));
	}
}

void UACFKimodoSubsystem::BeginDownload(const FString& Url, const FString& File)
{
	LogSetup(TEXT("> download ") + Url);
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(File), true);
	ActiveRequest = FHttpModule::Get().CreateRequest();
	ActiveRequest->SetVerb(TEXT("GET"));
	ActiveRequest->SetURL(Url);
	ActiveRequest->SetHeader(TEXT("User-Agent"), TEXT("yarin-studio"));
	ActiveRequest->SetTimeout(600.0f);
	TWeakObjectPtr<UACFKimodoSubsystem> Self(this);
	ActiveRequest->OnProcessRequestComplete().BindLambda([Self, File](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSucceeded)
	{
		if (UACFKimodoSubsystem* Live = Self.Get())
		{
			Live->OnDownloadComplete(Request, Response, bSucceeded, File);
		}
	});
	if (!ActiveRequest->ProcessRequest())
	{
		ActiveRequest.Reset();
		Fail(TEXT("The download could not start. Check the network and try again."));
	}
}

void UACFKimodoSubsystem::OnDownloadComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSucceeded, FString File)
{
	if (Request != ActiveRequest)
	{
		return;
	}
	ActiveRequest.Reset();
	const int32 Code = Response.IsValid() ? Response->GetResponseCode() : 0;
	if (!bSucceeded || !Response.IsValid() || Code < 200 || Code >= 300 || Response->GetContent().IsEmpty())
	{
		Fail(FString::Printf(TEXT("The download failed (%d). Check the network and press Retry."), Code));
		return;
	}
	if (!FFileHelper::SaveArrayToFile(Response->GetContent(), *File))
	{
		Fail(FString::Printf(TEXT("The download could not be saved to %s."), *NativePath(File)));
		return;
	}
	LogSetup(FString::Printf(TEXT("saved %s (%d bytes)"), *NativePath(File), Response->GetContent().Num()));
	NextStep();
}

bool UACFKimodoSubsystem::FinishEncoder(FString& Error) const
{
	const FString Base = EncoderDirectory() / TEXT("Meta-Llama-3-8B-Instruct");
	const FString Adapter = EncoderDirectory() / TEXT("McGill-NLP/LLM2Vec-Meta-Llama-3-8B-Instruct-mntp/adapter_config.json");
	if (!FPaths::FileExists(Base / TEXT("model.safetensors.index.json")))
	{
		Error = TEXT("The language model download is incomplete. Press Retry.");
		return false;
	}
	// The mntp adapter names the gated meta-llama repo as its base; point it at the local copy instead.
	FString Text;
	TSharedPtr<FJsonObject> Json;
	if (!FFileHelper::LoadFileToString(Text, *Adapter)
		|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Json) || !Json.IsValid())
	{
		Error = TEXT("The language model adapter could not be read. Press Retry.");
		return false;
	}
	Json->SetStringField(TEXT("base_model_name_or_path"), FPaths::ConvertRelativePathToFull(Base).Replace(TEXT("\\"), TEXT("/")));
	FString Patched;
	if (!FJsonSerializer::Serialize(Json.ToSharedRef(), TJsonWriterFactory<>::Create(&Patched))
		|| !FFileHelper::SaveStringToFile(Patched + TEXT("\n"), *Adapter, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)
		|| !FFileHelper::SaveStringToFile(FDateTime::UtcNow().ToIso8601(), *(EncoderDirectory() / TEXT("ready")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		Error = FString::Printf(TEXT("Could not write into %s."), *NativePath(EncoderDirectory()));
		return false;
	}
	return true;
}

void UACFKimodoSubsystem::StartProcess(const FString& Executable, const FString& Arguments, const FString& Directory, double TimeoutSeconds)
{
	ClosePipes();
	if (Phase == EPhase::Generating)
	{
		ApplyGenerateEnvironment();
	}
	else
	{
		ApplySetupEnvironment();
		LogSetup(FString::Printf(TEXT("> %s %s"), *Executable, *Arguments));
	}
	IFileManager::Get().MakeDirectory(*Directory, true);
	FPlatformProcess::CreatePipe(StdOutRead, StdOutWrite);
	FPlatformProcess::CreatePipe(StdErrRead, StdErrWrite);
	ProcessStartedAt = FPlatformTime::Seconds();
	ProcessTimeout = TimeoutSeconds;
	ProcessLog.Reset();
	UE_LOG(LogACFKimodo, Log, TEXT("Running %s %s"), *Executable, *Arguments);
	Process = FPlatformProcess::CreateProc(*Executable, *Arguments, false, true, true, nullptr, 0, *Directory, StdOutWrite, nullptr, StdErrWrite);
	if (!Process.IsValid())
	{
		ClosePipes();
		Fail(FString::Printf(TEXT("Could not start %s."), *NativePath(Executable)));
		return;
	}
	ArmTicker();
}

void UACFKimodoSubsystem::ClosePipes()
{
	if (StdOutRead || StdOutWrite)
	{
		FPlatformProcess::ClosePipe(StdOutRead, StdOutWrite);
		StdOutRead = nullptr;
		StdOutWrite = nullptr;
	}
	if (StdErrRead || StdErrWrite)
	{
		FPlatformProcess::ClosePipe(StdErrRead, StdErrWrite);
		StdErrRead = nullptr;
		StdErrWrite = nullptr;
	}
}

void UACFKimodoSubsystem::PullPipes()
{
	auto Append = [this](void* Pipe)
	{
		if (!Pipe)
		{
			return;
		}
		const FString Chunk = FPlatformProcess::ReadPipe(Pipe);
		if (!Chunk.IsEmpty())
		{
			AppendLog(Chunk);
			ProcessLog += Chunk;
			if (ProcessLog.Len() > 8000)
			{
				ProcessLog.RightChopInline(ProcessLog.Len() - 8000);
			}
		}
	};
	Append(StdOutRead);
	Append(StdErrRead);
}

void UACFKimodoSubsystem::ArmTicker()
{
	if (TickerHandle.IsValid())
	{
		return;
	}
	TWeakObjectPtr<UACFKimodoSubsystem> Self(this);
	TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Self](float)
	{
		UACFKimodoSubsystem* Live = Self.Get();
		if (!Live)
		{
			return false;
		}
		Live->TickWork();
		const bool bKeep = Live->Process.IsValid() || Live->ActiveRequest.IsValid();
		if (!bKeep)
		{
			Live->TickerHandle.Reset();
		}
		return bKeep;
	}), 0.25f);
}

void UACFKimodoSubsystem::TrackEncoderSize()
{
	const double Now = FPlatformTime::Seconds();
	if (Now - LastSizeCheck < 2.0)
	{
		return;
	}
	LastSizeCheck = Now;
	int64 Bytes = 0;
	IFileManager::Get().IterateDirectoryStatRecursively(*EncoderDirectory(), [&Bytes](const TCHAR*, const FFileStatData& Stat)
	{
		if (!Stat.bIsDirectory)
		{
			Bytes += Stat.FileSize;
		}
		return true;
	});
	const double Total = 15.3;
	const double Done = Bytes / (1024.0 * 1024.0 * 1024.0);
	StatusText = FString::Printf(TEXT("%s %d%% (%.1f of %.1f GB)"), *StepStatus, FMath::Clamp(FMath::FloorToInt(Done / Total * 100.0), 0, 99), Done, Total);
}

void UACFKimodoSubsystem::TickWork()
{
	if (!Process.IsValid())
	{
		return;
	}
	PullPipes();
	if (Steps.IsValidIndex(StepIndex) && Steps[StepIndex].bTrackEncoderSize)
	{
		TrackEncoderSize();
	}
	if (FPlatformProcess::IsProcRunning(Process))
	{
		if (FPlatformTime::Seconds() - ProcessStartedAt > ProcessTimeout)
		{
			FPlatformProcess::TerminateProc(Process, true);
			FPlatformProcess::CloseProc(Process);
			ClosePipes();
			Fail(TEXT("Kimodo took too long and was stopped. Press Retry."));
		}
		return;
	}
	int32 Code = 1;
	FPlatformProcess::GetProcReturnCode(Process, &Code);
	PullPipes();
	FPlatformProcess::CloseProc(Process);
	ClosePipes();
	if (Phase == EPhase::Installing)
	{
		FString Output = ProcessLog;
		Output.TrimStartAndEndInline();
		LogSetup(FString::Printf(TEXT("exit %d\n%s"), Code, *Output));
	}
	if (Code != 0)
	{
		FString Tail = ProcessLog;
		Tail.ReplaceInline(TEXT("\r"), TEXT(""));
		if (Tail.Len() > 500)
		{
			Tail.RightChopInline(Tail.Len() - 500);
		}
		Tail.TrimStartAndEndInline();
		Fail(Tail.IsEmpty() ? FString::Printf(TEXT("The step failed (%d)."), Code) : Tail);
		return;
	}
	if (Phase == EPhase::Generating)
	{
		FinishGenerate();
		return;
	}
	NextStep();
}

void UACFKimodoSubsystem::BeginGenerate()
{
	FString Prompt = PendingPrompt;
	Prompt.ReplaceInline(TEXT("\r"), TEXT(" "));
	Prompt.ReplaceInline(TEXT("\n"), TEXT(" "));
	TArray<FString> Words;
	Prompt.ParseIntoArrayWS(Words);
	Prompt = FString::Join(Words, TEXT(" "));
	if (Prompt.IsEmpty())
	{
		Fail(TEXT("Describe the motion to generate."));
		return;
	}
	ResolvedExecutable = LocateExecutable();
	if (ResolvedExecutable.IsEmpty())
	{
		Fail(TEXT("Kimodo is not installed."));
		return;
	}
	FString Launch = ResolvedExecutable;
	FString Prefix;
	if (Launch.EndsWith(TEXT(".cmd")) || Launch.EndsWith(TEXT(".bat")))
	{
		const FString Python = FPaths::Combine(FPaths::GetPath(Launch), TEXT("python.exe"));
		if (FPaths::FileExists(Python))
		{
			Launch = Python;
			Prefix = TEXT("-m kimodo.scripts.generate ");
		}
	}
	if (PendingAssetName.TrimStartAndEnd().IsEmpty())
	{
		PendingAssetName = AnimationName(Prompt);
	}
	PendingOutputDirectory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Yarin/Motions") / FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")));
	const float Seconds = FMath::Clamp(PendingDuration <= 0.0f ? 5.0f : PendingDuration, 1.0f, 20.0f);
	FString Arguments = Prefix + Quote(Prompt) + FString::Printf(TEXT(" --duration %g --num_samples 1 --bvh --bvh_standard_tpose"), Seconds);
	if (!HasMotionCorrection())
	{
		Arguments += TEXT(" --no-postprocess");
	}
	const FString Model = MotionModelRepo();
	if (!Model.Equals(TEXT("nvidia/Kimodo-SOMA-RP-v1.2"), ESearchCase::IgnoreCase))
	{
		Arguments += TEXT(" --model ") + Quote(FPaths::GetCleanFilename(Model));
	}
	Arguments += TEXT(" --output motion");
	SetPhase(EPhase::Generating, TEXT("Generating..."));
	AppendLog(Launch + TEXT(" ") + Arguments + TEXT("\n"));
	StartProcess(Launch, Arguments, PendingOutputDirectory, 1800.0);
}

void UACFKimodoSubsystem::FinishGenerate()
{
	TArray<FString> Found;
	IFileManager::Get().FindFilesRecursive(Found, *PendingOutputDirectory, TEXT("*.bvh"), true, false);
	if (Found.IsEmpty())
	{
		Fail(TEXT("Kimodo finished without a BVH file. See the Log."));
		return;
	}
	SetPhase(EPhase::Ready, TEXT("Ready"));
	const FACFMotionImportResult Result = UACFMotionImportLibrary::ImportMotionFile(Found[0], FString(), PendingAssetName, PendingTargetMesh);
	if (!Result.bSuccess)
	{
		AppendLog(Result.Error + TEXT("\n"));
	}
	Complete(Result, Result.bSuccess ? FString() : Result.Error, Found[0]);
}
