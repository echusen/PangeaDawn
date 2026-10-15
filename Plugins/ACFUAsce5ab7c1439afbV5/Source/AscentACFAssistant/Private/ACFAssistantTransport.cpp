// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "ACFAssistantClientInternal.h"
#include "ACFAssistantOwnPlugin.h"
#include "ACFAssistantSettings.h"
#include "ACFAssistantToolGate.h"
#include "AscentACFAssistant.h"
#include "AscentEpicMCP.h"
#include "Interfaces/IPluginManager.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Async/Async.h"
#include "Async/TaskGraphInterfaces.h"
#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include "Windows/WindowsHWrapper.h"
#include "Windows/HideWindowsPlatformTypes.h"
#endif

namespace
{
	using FObject = TSharedPtr<FJsonObject>;
	FObject Object() { return MakeShared<FJsonObject>(); }
	FString String(const FObject& Json, const TCHAR* Key)
	{
		FString Value;
		if (Json.IsValid()) { Json->TryGetStringField(Key, Value); }
		return Value;
	}
	FObject Child(const FObject& Json, const TCHAR* Key)
	{
		FObject Value;
		if (Json.IsValid())
		{
			const TSharedPtr<FJsonValue> Field = Json->TryGetField(Key);
			if (Field.IsValid() && Field->Type == EJson::Object) { Value = Field->AsObject(); }
		}
		return Value.IsValid() ? Value : Object();
	}
	TArray<TSharedPtr<FJsonValue>> Array(const FObject& Json, const TCHAR* Key)
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (Json.IsValid() && Json->TryGetArrayField(Key, Values) && Values) { return *Values; }
		return {};
	}

#if PLATFORM_WINDOWS
	// The editor environment minus the nested-session markers. Shared by every claude child:
	// the sign-in check, the login console, turns and one-shots must all agree on which account they see.
	TArray<WCHAR> ClaudeEnvironmentBlock(bool bLongToolTimeout = false)
	{
		TArray<FString> Entries;
		LPWCH Environment = ::GetEnvironmentStringsW();
		for (const WCHAR* Entry = Environment; Entry && *Entry; Entry += FCString::Strlen(Entry) + 1)
		{
			const FString Line(Entry);
			if (!FAscentACFAssistant::IsClaudeNestingEnvironmentEntry(Line)) { Entries.Add(Line); }
		}
		if (Environment) { ::FreeEnvironmentStringsW(Environment); }
		if (bLongToolTimeout)
		{
			Entries.RemoveAll([](const FString& Line) { return Line.StartsWith(TEXT("MCP_TOOL_TIMEOUT="), ESearchCase::IgnoreCase); });
			Entries.Add(TEXT("MCP_TOOL_TIMEOUT=3600000"));
		}
		Entries.Sort();
		TArray<WCHAR> Block;
		for (const FString& Entry : Entries)
		{
			Block.Append(*Entry, Entry.Len() + 1);
		}
		Block.Add(0);
		return Block;
	}

	// npm installs claude as a .cmd shim, which only cmd.exe can start. An empty application lets CreateProcess find cmd.exe itself;
	// lpApplicationName does not search PATH.
	void ClaudeCommandLine(const FString& Program, const FString& Args, FString& OutApplication, FString& OutCommand)
	{
		if (Program.EndsWith(TEXT(".cmd")) || Program.EndsWith(TEXT(".bat")))
		{
			OutApplication.Reset();
			OutCommand = FString::Printf(TEXT("cmd.exe /c \"\"%s\" %s\""), *Program, *Args);
			return;
		}
		OutApplication = Program;
		OutCommand = FString::Printf(TEXT("\"%s\" %s"), *Program, *Args);
	}

	// Kill-on-close job for one CLI and everything it spawns. Terminating only the CLI leaves its shells and tool processes alive,
	// and they keep the inherited stdin/stdout pipes open: a WriteFile on a full stdin then never returns and the Editor hangs on exit.
	HANDLE CreateProcessTreeJob()
	{
		HANDLE Job = ::CreateJobObjectW(nullptr, nullptr);
		if (!Job) { return nullptr; }
		JOBOBJECT_EXTENDED_LIMIT_INFORMATION Limits = {};
		Limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
		if (!::SetInformationJobObject(Job, JobObjectExtendedLimitInformation, &Limits, sizeof(Limits)))
		{
			::CloseHandle(Job);
			return nullptr;
		}
		return Job;
	}

	// The process must have been created CREATE_SUSPENDED, so nothing it spawns can start outside the job.
	void ResumeInJob(HANDLE& Job, const PROCESS_INFORMATION& Info)
	{
		if (Job && !::AssignProcessToJobObject(Job, Info.hProcess))
		{
			::CloseHandle(Job);
			Job = nullptr;
		}
		::ResumeThread(Info.hThread);
	}

	// Ends the whole tree even when the CLI already exited on its own: its orphans are still in the job.
	void EndProcessTree(void*& Job, void* Process)
	{
		if (Job)
		{
			::TerminateJobObject(static_cast<HANDLE>(Job), 0);
			::CloseHandle(static_cast<HANDLE>(Job));
			Job = nullptr;
		}
		else if (Process && ::WaitForSingleObject(static_cast<HANDLE>(Process), 0) == WAIT_TIMEOUT)
		{
			::TerminateProcess(static_cast<HANDLE>(Process), 0);
		}
	}

	// Runs one short claude command to completion and returns its stdout. Blocks, so it only ever runs on a worker thread:
	// a warm `claude auth status` measured 1.6-2.1 s, and that used to freeze the editor on every panel open.
	bool CaptureClaude(const FString& Program, const FString& Args, const FString& Directory, FString& OutStdout, double TimeoutSeconds = 30.0, const FString& StdinText = FString())
	{
		SECURITY_ATTRIBUTES Security = {sizeof(SECURITY_ATTRIBUTES), nullptr, true};
		HANDLE ReadPipe = nullptr;
		HANDLE WritePipe = nullptr;
		if (!::CreatePipe(&ReadPipe, &WritePipe, &Security, 0)) { return false; }
		::SetHandleInformation(ReadPipe, HANDLE_FLAG_INHERIT, 0);
		HANDLE ChildInput = nullptr;
		HANDLE ParentInput = nullptr;
		if (!StdinText.IsEmpty() && ::CreatePipe(&ChildInput, &ParentInput, &Security, 0)) { ::SetHandleInformation(ParentInput, HANDLE_FLAG_INHERIT, 0); }
		// stderr goes nowhere: the stdout JSON is the answer, and a warning line on stderr must not be parsed as part of it.
		HANDLE NullError = ::CreateFileW(L"NUL", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &Security, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		STARTUPINFOW Startup = {};
		Startup.cb = sizeof(Startup);
		Startup.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
		Startup.wShowWindow = SW_HIDE;
		Startup.hStdInput = ChildInput;
		Startup.hStdOutput = WritePipe;
		Startup.hStdError = NullError;
		PROCESS_INFORMATION Info = {};
		FString Application;
		FString Command;
		ClaudeCommandLine(Program, Args, Application, Command);
		TArray<WCHAR> Environment = ClaudeEnvironmentBlock();
		// SW_HIDE, not CREATE_NO_WINDOW: without a console the bun-based CLI writes nothing.
		const bool bStarted = ::CreateProcessW(Application.IsEmpty() ? nullptr : *Application, Command.GetCharArray().GetData(), nullptr, nullptr, true,
			CREATE_UNICODE_ENVIRONMENT, Environment.GetData(), Directory.IsEmpty() ? nullptr : *Directory, &Startup, &Info) != 0;
		::CloseHandle(WritePipe);
		if (NullError && NullError != INVALID_HANDLE_VALUE) { ::CloseHandle(NullError); }
		if (ChildInput) { ::CloseHandle(ChildInput); }
		if (ParentInput)
		{
			// A request line is far below the pipe buffer, so this write never waits on the child. Closing the pipe ends the child's input.
			if (bStarted)
			{
				const FTCHARToUTF8 Utf8(*StdinText);
				DWORD Written = 0;
				::WriteFile(ParentInput, Utf8.Get(), Utf8.Length(), &Written, nullptr);
			}
			::CloseHandle(ParentInput);
		}
		if (!bStarted) { ::CloseHandle(ReadPipe); return false; }
		TArray<uint8> Bytes;
		const double Deadline = FPlatformTime::Seconds() + TimeoutSeconds;
		bool bExited = false;
		while (true)
		{
			DWORD Available = 0;
			while (::PeekNamedPipe(ReadPipe, nullptr, 0, nullptr, &Available, nullptr) && Available > 0)
			{
				uint8 Buffer[4096];
				DWORD Read = 0;
				if (!::ReadFile(ReadPipe, Buffer, FMath::Min<DWORD>(Available, sizeof(Buffer)), &Read, nullptr) || Read == 0) { break; }
				Bytes.Append(Buffer, Read);
			}
			if (bExited) { break; }
			bExited = ::WaitForSingleObject(Info.hProcess, 20) != WAIT_TIMEOUT;
			if (!bExited && FPlatformTime::Seconds() > Deadline)
			{
				::TerminateProcess(Info.hProcess, 1);
				break;
			}
		}
		::CloseHandle(ReadPipe);
		::CloseHandle(Info.hThread);
		::CloseHandle(Info.hProcess);
		const FUTF8ToTCHAR Converted(reinterpret_cast<const ANSICHAR*>(Bytes.GetData()), Bytes.Num());
		OutStdout = FString(Converted.Length(), Converted.Get());
		return bExited;
	}
#endif

	FString ClaudeWorkingDirectory()
	{
		// Every claude child runs where the turns run, so project-level Claude settings are seen the same way by the sign-in check.
		FString Directory = FPaths::ConvertRelativePathToFull(FPaths::GetPath(FPaths::GetProjectFilePath()));
		FPaths::MakePlatformFilename(Directory);
		return Directory;
	}
}

struct FClaudeLoginProcess
{
	void* Job = nullptr;
	void* Process = nullptr;
	FThreadSafeBool bStop = false;

	void Stop()
	{
		bStop = true;
#if PLATFORM_WINDOWS
		if (Job) { ::TerminateJobObject(Job, 1); }
		else if (Process) { ::TerminateProcess(Process, 1); }
#endif
	}

	~FClaudeLoginProcess()
	{
#if PLATFORM_WINDOWS
		// The job is created kill-on-close, so dropping the last reference also closes a console the user abandoned.
		if (Process) { ::CloseHandle(Process); }
		if (Job) { ::CloseHandle(Job); }
#endif
	}
};

namespace
{
	constexpr double ClaudeLoginTimeoutSeconds = 15.0 * 60.0;
}

bool FAscentACFAssistant::DecodeMessage(TArray<uint8>& Buffer, TSharedPtr<FJsonObject>& Message, bool& bInvalid, FString* OutLine)
{
	bInvalid = Buffer.Num() > 8 * 1024 * 1024;
	if (bInvalid) { return false; }
	int32 Newline = INDEX_NONE;
	if (!Buffer.Find(static_cast<uint8>('\n'), Newline)) { return false; }
	const FUTF8ToTCHAR Converted(reinterpret_cast<const ANSICHAR*>(Buffer.GetData()), Newline);
	const FString Line(Converted.Length(), Converted.Get());
	Buffer.RemoveAt(0, Newline + 1, EAllowShrinking::No);
	if (OutLine) { *OutLine = Line; }
	bInvalid = !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Line), Message) || !Message;
	return !bInvalid;
}

void FAscentACFAssistant::ConnectCodex()
{
#if PLATFORM_WINDOWS
	FString Program = UACFAssistantSettings::Get().CodexExecutable.TrimStartAndEnd().TrimQuotes();
	if (Program.IsEmpty()) { Program = FindCodexExecutable(); }
	if (!FPaths::FileExists(Program) || !Program.EndsWith(TEXT(".exe")))
	{
		Fail(EACFAssistantProvider::Codex, TEXT("Install Codex 0.153.4 or later so it is on PATH."));
		return;
	}
	Codex.Status = TEXT("Checking Codex...");
	// `codex --version` blocks for ~80 ms warm and longer cold; both providers connect when the panel opens, so it runs on a worker.
	const int32 CheckGeneration = Codex.Generation;
	TWeakPtr<FAscentACFAssistant> WeakSelf = AsShared();
	Async(EAsyncExecution::Thread, [WeakSelf, Program, CheckGeneration]()
	{
		int32 ReturnCode = -1;
		FString Version;
		FString ErrorText;
		FPlatformProcess::ExecProcess(*Program, TEXT("--version"), &ReturnCode, &Version, &ErrorText);
		AsyncTask(ENamedThreads::GameThread, [WeakSelf, Program, CheckGeneration, ReturnCode, Version, ErrorText]()
		{
			if (const TSharedPtr<FAscentACFAssistant> Self = WeakSelf.Pin())
			{
				Self->StartCodexProcess(CheckGeneration, Program, ReturnCode, Version, ErrorText);
			}
		});
	});
#else
	Fail(EACFAssistantProvider::Codex, TEXT("Yarin chat is currently supported on Windows Editor builds."));
#endif
}

void FAscentACFAssistant::StartCodexProcess(int32 CheckGeneration, const FString& Program, int32 ReturnCode, const FString& Version, const FString& ErrorText)
{
#if PLATFORM_WINDOWS
	// A sign-out, retry or editor shutdown since the check started owns the state now.
	if (CheckGeneration != Codex.Generation || Codex.State != EConnectionState::Connecting) { return; }
	if (ReturnCode != 0 || !IsSupportedCodexVersion(Version))
	{
		FString DetectedVersion = Version.TrimStartAndEnd();
		DetectedVersion.ReplaceInline(TEXT("\r"), TEXT(" "));
		DetectedVersion.ReplaceInline(TEXT("\n"), TEXT(" "));
		if (DetectedVersion.IsEmpty()) { DetectedVersion = ErrorText.TrimStartAndEnd(); }
		if (DetectedVersion.IsEmpty()) { DetectedVersion = TEXT("no version output"); }
		Fail(EACFAssistantProvider::Codex, FString::Printf(TEXT("Requires codex-cli 0.153.4 or later. Detected: %s"), *DetectedVersion));
		return;
	}
	UE_LOG(LogAscentACFAssistant, Log, TEXT("Codex runtime: %s (%s)"), *Program, *Version.TrimStartAndEnd());
	const FString CodexDirectory = FPaths::Combine(ProfileDirectory, TEXT("Codex"));
	IFileManager::Get().MakeDirectory(*CodexDirectory, true);
	SyncSkills(FPaths::Combine(CodexDirectory, TEXT("skills")));
	// Yarin publishes all Unreal and Ascent tools through its own MCP server instance on a dedicated loopback port; the shared module server (default port 8000) is left to its owner.
	// The CLI never talks to that server directly: it goes through the work-mode gate, which judges every tools/call first.
	if (!AscentEpicMCP::Ensure() || !ACFAssistantToolGate::Ensure())
	{
		Fail(EACFAssistantProvider::Codex, TEXT("Yarin could not start Unreal's Editor tool service. Restart the Editor and try again."));
		return;
	}
	UE_LOG(LogAscentACFAssistant, Log, TEXT("Connecting to Codex. Protocol log: %s"), *FPaths::Combine(ProfileDirectory, TEXT("protocol.log")));
	// Whoever is signed in to Yarin gives the runtime its gateway provider. The key is passed below, in the environment, never in this file.
	const bool bGateway = IsProviderAvailable(EACFAssistantProvider::Yarin) && YarinCredential.IsComplete();
	CodexGatewayKeyHash = bGateway ? GatewayKeyHash(YarinCredential.GatewayKey) : FString();
	if (!FFileHelper::SaveStringToFile(CodexRuntimeConfiguration(bGateway ? YarinCredential.GatewayBaseUrl : FString()), *FPaths::Combine(CodexDirectory, TEXT("config.toml")),
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		Fail(EACFAssistantProvider::Codex, TEXT("Yarin could not prepare its working folder. Check disk permissions."));
		return;
	}

	TArray<FString> Entries;
	LPWCH Environment = ::GetEnvironmentStringsW();
	for (const WCHAR* Entry = Environment; Entry && *Entry; Entry += FCString::Strlen(Entry) + 1)
	{
		const FString Line(Entry);
		if (!Line.StartsWith(TEXT("CODEX_"), ESearchCase::IgnoreCase)
			&& !Line.StartsWith(TEXT("OPENAI_"), ESearchCase::IgnoreCase)
			&& !Line.StartsWith(TEXT("ANTHROPIC_"), ESearchCase::IgnoreCase)
			&& !Line.StartsWith(TEXT("CLAUDE_"), ESearchCase::IgnoreCase)
			// An ambient key must never become the credential of a provider this profile did not declare.
			&& !Line.StartsWith(TEXT("YARIN_GATEWAY_API_KEY="), ESearchCase::IgnoreCase))
		{
			Entries.Add(Line);
		}
	}
	if (Environment) { ::FreeEnvironmentStringsW(Environment); }
	Entries.Add(TEXT("CODEX_HOME=") + CodexDirectory);
	if (bGateway) { Entries.Add(TEXT("YARIN_GATEWAY_API_KEY=") + YarinCredential.GatewayKey); }
	Entries.Sort();
	TArray<WCHAR> EnvironmentBlock;
	for (const FString& Entry : Entries)
	{
		EnvironmentBlock.Append(*Entry, Entry.Len() + 1);
	}
	EnvironmentBlock.Add(0);

	SECURITY_ATTRIBUTES Security = {sizeof(SECURITY_ATTRIBUTES), nullptr, true};
	HANDLE ChildOutput = nullptr;
	HANDLE ChildInput = nullptr;
	HANDLE ParentOutput = nullptr;
	HANDLE ParentInput = nullptr;
	HANDLE NullError = ::CreateFileW(L"NUL", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &Security, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	bool bPipes = ::CreatePipe(&ParentOutput, &ChildOutput, &Security, 0) && ::CreatePipe(&ChildInput, &ParentInput, &Security, 0);
	bPipes = bPipes && ::SetHandleInformation(ParentOutput, HANDLE_FLAG_INHERIT, 0) && ::SetHandleInformation(ParentInput, HANDLE_FLAG_INHERIT, 0);
	STARTUPINFOW Startup = {};
	Startup.cb = sizeof(Startup);
	Startup.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
	Startup.wShowWindow = SW_HIDE;
	Startup.hStdInput = ChildInput;
	Startup.hStdOutput = ChildOutput;
	Startup.hStdError = NullError;
	PROCESS_INFORMATION Info = {};
	FString Command = TEXT("\"") + Program + TEXT("\" app-server --strict-config --listen stdio://");
	HANDLE Job = CreateProcessTreeJob();
	const bool bStarted = bPipes && ::CreateProcessW(*Program, Command.GetCharArray().GetData(), nullptr, nullptr, true,
		CREATE_NO_WINDOW | CREATE_SUSPENDED | CREATE_UNICODE_ENVIRONMENT, EnvironmentBlock.GetData(), *WorkingDirectory, &Startup, &Info);
	for (HANDLE Handle : {ChildOutput, ChildInput, NullError})
	{
		if (Handle && Handle != INVALID_HANDLE_VALUE) { ::CloseHandle(Handle); }
	}
	if (!bStarted)
	{
		if (Job) { ::CloseHandle(Job); }
		if (ParentOutput) { ::CloseHandle(ParentOutput); }
		if (ParentInput) { ::CloseHandle(ParentInput); }
		Fail(EACFAssistantProvider::Codex, TEXT("Yarin could not start its AI runtime. Install Codex and try again."));
		return;
	}
	ResumeInJob(Job, Info);
	Process = Info.hProcess;
	ProcessThread = Info.hThread;
	ProcessJob = Job;
	OutputRead = ParentOutput;
	InputWrite = ParentInput;
	bStopWriter = false;
	bWriteFailed = false;
	Writer = Async(EAsyncExecution::Thread, [this]
	{
		HANDLE Self = nullptr;
		::DuplicateHandle(::GetCurrentProcess(), ::GetCurrentThread(), ::GetCurrentProcess(), &Self, 0, false, DUPLICATE_SAME_ACCESS);
		WriterThread = Self;
		while (!bStopWriter)
		{
			TArray<uint8> Bytes;
			if (!Outgoing.Dequeue(Bytes)) { FPlatformProcess::Sleep(0.01f); continue; }
			int32 Offset = 0;
			while (Offset < Bytes.Num() && !bStopWriter)
			{
				DWORD Written = 0;
				if (!::WriteFile(InputWrite, Bytes.GetData() + Offset, Bytes.Num() - Offset, &Written, nullptr) || Written == 0)
				{
					bWriteFailed = true;
					return;
				}
				Offset += Written;
			}
		}
	});
	Codex.Ticker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FAscentACFAssistant::TickCodex), 0.05f);
	Codex.LastActivity = FPlatformTime::Seconds();
	Codex.Status = TEXT("Connecting to Codex...");
	auto Params = MakeShared<FJsonObject>();
	auto Client = MakeShared<FJsonObject>();
	Client->SetStringField(TEXT("name"), TEXT("acf_assistant_client"));
	Client->SetStringField(TEXT("title"), TEXT("Yarin"));
	Client->SetStringField(TEXT("version"), TEXT("1.0.0"));
	Params->SetObjectField(TEXT("clientInfo"), Client);
	auto Capabilities = MakeShared<FJsonObject>();
	Capabilities->SetBoolField(TEXT("experimentalApi"), true);
	Params->SetObjectField(TEXT("capabilities"), Capabilities);
	Request(TEXT("initialize"), Params, [this, CodexDirectory](TSharedPtr<FJsonObject> Result)
	{
		FString ReturnedHome;
		Result->TryGetStringField(TEXT("codexHome"), ReturnedHome);
		FPaths::NormalizeDirectoryName(ReturnedHome);
		FString ExpectedHome = CodexDirectory;
		FPaths::NormalizeDirectoryName(ExpectedHome);
		if (!ReturnedHome.Equals(ExpectedHome, ESearchCase::IgnoreCase))
		{
			Disconnect(EACFAssistantProvider::Codex);
			Fail(EACFAssistantProvider::Codex, TEXT("The connection could not be secured and was stopped. Reconnect and try again."));
			return;
		}
		auto Notification = MakeShared<FJsonObject>();
		Notification->SetStringField(TEXT("method"), TEXT("initialized"));
		Notification->SetObjectField(TEXT("params"), MakeShared<FJsonObject>());
		Write(Notification);
		Codex.bInitialized = true;
		// Yarin's models were waiting for this process; it may also need replacing if it started without the right key.
		FinishYarinConnect();
		// FinishYarinConnect replaces this process when it carries the wrong gateway key; nothing may be asked of the connection it just closed.
		if (Codex.bInitialized) { ReadAccount(); }
	});
#else
	Fail(EACFAssistantProvider::Codex, TEXT("Yarin chat is currently supported on Windows Editor builds."));
#endif
}

FString FAscentACFAssistant::ClaudeProgram() const
{
	FString Program = UACFAssistantSettings::Get().ClaudeExecutable.TrimStartAndEnd().TrimQuotes();
	if (Program.IsEmpty()) { Program = FindClaudeExecutable(); }
	return !Program.IsEmpty() && FPaths::FileExists(Program) ? Program : FString();
}

FString FAscentACFAssistant::ClaudePluginDirectory() const
{
	return FPaths::ConvertRelativePathToFull(FPaths::Combine(ProfileDirectory, TEXT("Claude/Plugins/acf")));
}

void FAscentACFAssistant::ConnectClaude()
{
	Claude.Status = TEXT("Checking Claude sign-in...");
	StartClaudeAuthCheck(false);
}

void FAscentACFAssistant::StartClaudeAuthCheck(bool bSignOutFirst)
{
#if PLATFORM_WINDOWS
	const FString Program = ClaudeProgram();
	if (Program.IsEmpty())
	{
		Fail(EACFAssistantProvider::ClaudeCode, TEXT("Claude Code is not installed. Install it from https://code.claude.com/docs/en/setup, then press Try again."));
		return;
	}
	const int32 CheckGeneration = Claude.Generation;
	TWeakPtr<FAscentACFAssistant> WeakSelf = AsShared();
	Async(EAsyncExecution::Thread, [WeakSelf, Program, Directory = ClaudeWorkingDirectory(), CheckGeneration, bSignOutFirst]()
	{
		FString Ignored;
		if (bSignOutFirst) { CaptureClaude(Program, TEXT("auth logout"), Directory, Ignored); }
		FString AuthStatus;
		CaptureClaude(Program, TEXT("auth status"), Directory, AuthStatus);
		AsyncTask(ENamedThreads::GameThread, [WeakSelf, CheckGeneration, AuthStatus, bSignOutFirst]()
		{
			if (const TSharedPtr<FAscentACFAssistant> Self = WeakSelf.Pin())
			{
				Self->FinishClaudeAuthCheck(CheckGeneration, AuthStatus, bSignOutFirst);
			}
		});
	});
#else
	Fail(EACFAssistantProvider::ClaudeCode, TEXT("Yarin chat is currently supported on Windows Editor builds."));
#endif
}

void FAscentACFAssistant::FinishClaudeAuthCheck(int32 CheckGeneration, const FString& AuthStatus, bool bSignedOutByUser)
{
	// A sign-out, retry or editor shutdown since the check started owns the state now.
	if (CheckGeneration != Claude.Generation || Claude.State != EConnectionState::Connecting) { return; }
	FString Problem;
	switch (ClassifyClaudeAuth(AuthStatus, &Problem))
	{
	case EClaudeAuth::Subscription:
	case EClaudeAuth::ApiBilling:
		CompleteClaudeConnect(AuthStatus);
		return;
	case EClaudeAuth::SignedOut:
		if (Claude.bSignInIfSignedOut) { LoginClaude(); return; }
		EnterSignedOut(EACFAssistantProvider::ClaudeCode, !Problem.IsEmpty() ? Problem
			: bSignedOutByUser ? FString(TEXT("Signed out of Claude Code."))
			: FString(TEXT("Sign in to Claude Code to use it in Yarin.")));
		return;
	default:
		Fail(EACFAssistantProvider::ClaudeCode, Problem);
		return;
	}
}

void FAscentACFAssistant::CompleteClaudeConnect(const FString& AuthStatus)
{
#if PLATFORM_WINDOWS
	Claude.AccountLabel = ClaudeAccountLabel(AuthStatus);
	if (!AscentEpicMCP::Ensure() || !ACFAssistantToolGate::Ensure())
	{
		Fail(EACFAssistantProvider::ClaudeCode, TEXT("Yarin could not start Unreal's Editor tool service. Restart the Editor and try again."));
		return;
	}
	const FString ClaudeDirectory = FPaths::Combine(ProfileDirectory, TEXT("Claude"));
	IFileManager::Get().MakeDirectory(*ClaudeDirectory, true);
	// The bundled skills ship as a Claude Code plugin loaded per turn with --plugin-dir, so they exist only inside Yarin's conversations
	// and never shadow or overwrite the user's own ~/.claude/skills.
	const FString PluginDirectory = ClaudePluginDirectory();
	const TSharedPtr<IPlugin> AcfPlugin = ACFAssistantPlugin::Owner();
	const FString Manifest = FString::Printf(
		TEXT("{\r\n  \"name\": \"acf\",\r\n  \"description\": \"Yarin skills bundled with the plugin.\",\r\n  \"version\": \"%s\"\r\n}\r\n"),
		AcfPlugin ? *AcfPlugin->GetDescriptor().VersionName : TEXT("0"));
	FFileHelper::SaveStringToFile(Manifest, *FPaths::Combine(PluginDirectory, TEXT(".claude-plugin/plugin.json")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	SyncSkills(FPaths::Combine(PluginDirectory, TEXT("skills")));
	RemoveLegacyClaudeSkills();
	TArray<FString> StaleConversationConfigs;
	IFileManager::Get().FindFiles(StaleConversationConfigs, *FPaths::Combine(ClaudeDirectory, TEXT("mcp-*.json")), true, false);
	for (const FString& Stale : StaleConversationConfigs) { IFileManager::Get().Delete(*FPaths::Combine(ClaudeDirectory, Stale)); }
	if (!FFileHelper::SaveStringToFile(ClaudeMcpConfiguration(ACFAssistantToolGate::Url()), *FPaths::Combine(ClaudeDirectory, TEXT("mcp.json")),
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)
		|| !FFileHelper::SaveStringToFile(Instructions(), *FPaths::Combine(ClaudeDirectory, TEXT("system-prompt.txt")),
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		Fail(EACFAssistantProvider::ClaudeCode, TEXT("Yarin could not prepare its working folder. Check disk permissions."));
		return;
	}
	UE_LOG(LogAscentACFAssistant, Log, TEXT("Connecting to Claude Code. Protocol log: %s"), *FPaths::Combine(ProfileDirectory, TEXT("protocol.log")));
	LoadClaudeModels();
	if (DefaultModelFor(EACFAssistantProvider::ClaudeCode).IsEmpty()) { Fail(EACFAssistantProvider::ClaudeCode, TEXT("No Claude models are available.")); return; }
	Claude.bInitialized = true;
	Claude.bAuthenticated = true;
	Claude.State = EConnectionState::Ready;
	Claude.Failure.Reset();
	Claude.Status = TEXT("Connected. Yarin is ready.");
	Claude.LastActivity = FPlatformTime::Seconds();
	// An expired sign-in stops the ticker by returning false, which leaves a dead handle behind; never run two.
	FTSTicker::GetCoreTicker().RemoveTicker(Claude.Ticker);
	Claude.Ticker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FAscentACFAssistant::TickClaude), 0.05f);
	Changed.Broadcast();
	// The aliases above are enough to chat now; the CLI's own catalog replaces them as soon as it answers.
	RefreshClaudeModels();
#else
	Fail(EACFAssistantProvider::ClaudeCode, TEXT("Yarin chat is currently supported on Windows Editor builds."));
#endif
}

void FAscentACFAssistant::RefreshClaudeModels()
{
#if PLATFORM_WINDOWS
	const FString Program = ClaudeProgram();
	if (Program.IsEmpty()) { return; }
	const int32 CheckGeneration = Claude.Generation;
	TWeakPtr<FAscentACFAssistant> WeakSelf = AsShared();
	// An `initialize` control request is answered from the CLI's own catalog without running a turn (about a second).
	Async(EAsyncExecution::Thread, [WeakSelf, Program, Directory = ClaudeWorkingDirectory(), CheckGeneration]()
	{
		FString Catalog;
		CaptureClaude(Program, TEXT("-p --input-format stream-json --output-format stream-json --verbose --no-session-persistence"), Directory, Catalog, 30.0,
			TEXT("{\"type\":\"control_request\",\"request_id\":\"yarin-models\",\"request\":{\"subtype\":\"initialize\"}}\n"));
		AsyncTask(ENamedThreads::GameThread, [WeakSelf, CheckGeneration, Catalog]()
		{
			const TSharedPtr<FAscentACFAssistant> Self = WeakSelf.Pin();
			// A sign-out, reconnect or editor shutdown since the request started owns the list now.
			if (!Self || CheckGeneration != Self->Claude.Generation || Self->Claude.State != EConnectionState::Ready) { return; }
			TArray<TSharedPtr<FACFAssistantModel>> Parsed = FAscentACFAssistant::ParseClaudeModels(Catalog);
			if (Parsed.IsEmpty())
			{
				UE_LOG(LogAscentACFAssistant, Warning, TEXT("Claude Code returned no model catalog; keeping the built-in aliases."));
				return;
			}
			UE_LOG(LogAscentACFAssistant, Log, TEXT("Claude Code offers %d models for this account."), Parsed.Num());
			Self->SetProviderModels(EACFAssistantProvider::ClaudeCode, MoveTemp(Parsed));
			Self->Changed.Broadcast();
		});
	});
#endif
}

void FAscentACFAssistant::RemoveLegacyClaudeSkills() const
{
	// Earlier builds copied the bundled skills into the personal ~/.claude/skills, where they leaked into every Claude Code session on
	// the machine. The version marker those builds wrote is the proof that the copies are Yarin's; without it nothing is touched.
	const FString Personal = FPaths::Combine(FPlatformProcess::UserHomeDir(), TEXT(".claude/skills"));
	const FString Marker = FPaths::Combine(Personal, TEXT(".acf_skills_version"));
	if (!FPaths::FileExists(Marker)) { return; }
	const TSharedPtr<IPlugin> AcfPlugin = ACFAssistantPlugin::Owner();
	if (!AcfPlugin) { return; }
	TArray<FString> Bundled;
	IFileManager::Get().FindFiles(Bundled, *(FPaths::ConvertRelativePathToFull(AcfPlugin->GetBaseDir() / TEXT("Resources/Skills")) / TEXT("*")), false, true);
	for (const FString& Skill : Bundled)
	{
		IFileManager::Get().DeleteDirectory(*FPaths::Combine(Personal, Skill), false, true);
	}
	IFileManager::Get().Delete(*Marker, false, true, true);
	UE_LOG(LogAscentACFAssistant, Log, TEXT("Removed %d skills an earlier Yarin build copied into %s"), Bundled.Num(), *Personal);
}

void FAscentACFAssistant::LoginClaude()
{
	Claude.bSignInIfSignedOut = false;
	if (ClaudeLogin.IsValid()) { return; }
#if PLATFORM_WINDOWS
	const FString Program = ClaudeProgram();
	if (Program.IsEmpty())
	{
		Fail(EACFAssistantProvider::ClaudeCode, TEXT("Claude Code is not installed. Install it from https://code.claude.com/docs/en/setup, then press Try again."));
		return;
	}
	// Kill-on-close job: cancel, provider switch and editor exit all end the whole console tree, including the child of a .cmd shim.
	HANDLE Job = ::CreateJobObjectW(nullptr, nullptr);
	if (Job)
	{
		JOBOBJECT_EXTENDED_LIMIT_INFORMATION Limits = {};
		Limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
		::SetInformationJobObject(Job, JobObjectExtendedLimitInformation, &Limits, sizeof(Limits));
	}
	STARTUPINFOW Startup = {};
	Startup.cb = sizeof(Startup);
	Startup.dwFlags = STARTF_USESHOWWINDOW;
	Startup.wShowWindow = SW_SHOW;
	PROCESS_INFORMATION Info = {};
	FString Application;
	FString Command;
	ClaudeCommandLine(Program, TEXT("auth login"), Application, Command);
	TArray<WCHAR> Environment = ClaudeEnvironmentBlock();
	const FString Directory = ClaudeWorkingDirectory();
	// Claude Code's sign-in may ask for a pasted code in its own console, so it gets a visible one; Yarin waits for it to exit.
	const bool bStarted = ::CreateProcessW(Application.IsEmpty() ? nullptr : *Application, Command.GetCharArray().GetData(), nullptr, nullptr, false,
		CREATE_NEW_CONSOLE | CREATE_SUSPENDED | CREATE_UNICODE_ENVIRONMENT, Environment.GetData(), *Directory, &Startup, &Info) != 0;
	if (!bStarted)
	{
		if (Job) { ::CloseHandle(Job); }
		EnterSignedOut(EACFAssistantProvider::ClaudeCode, TEXT("Yarin could not open the Claude sign-in window. Run `claude auth login` in a terminal, then press Sign in with Claude."));
		return;
	}
	if (Job && !::AssignProcessToJobObject(Job, Info.hProcess))
	{
		::CloseHandle(Job);
		Job = nullptr;
	}
	::ResumeThread(Info.hThread);
	::CloseHandle(Info.hThread);
	const TSharedPtr<FClaudeLoginProcess> Login = MakeShared<FClaudeLoginProcess>();
	Login->Job = Job;
	Login->Process = Info.hProcess;
	ClaudeLogin = Login;
	Claude.State = EConnectionState::SigningIn;
	Claude.Failure.Reset();
	Claude.Status = TEXT("Finish signing in to Claude in your browser. If the Claude window asks for a code, paste it there. Yarin continues on its own.");
	Changed.Broadcast();
	TWeakPtr<FAscentACFAssistant> WeakSelf = AsShared();
	Async(EAsyncExecution::Thread, [WeakSelf, Login, Program, Directory]()
	{
		const double Deadline = FPlatformTime::Seconds() + ClaudeLoginTimeoutSeconds;
		bool bTimedOut = false;
		while (::WaitForSingleObject(Login->Process, 250) == WAIT_TIMEOUT)
		{
			if (Login->bStop) { break; }
			if (FPlatformTime::Seconds() > Deadline) { bTimedOut = true; Login->Stop(); break; }
		}
		FString AuthStatus;
		if (!Login->bStop) { CaptureClaude(Program, TEXT("auth status"), Directory, AuthStatus); }
		AsyncTask(ENamedThreads::GameThread, [WeakSelf, Login, AuthStatus, bTimedOut]()
		{
			if (const TSharedPtr<FAscentACFAssistant> Self = WeakSelf.Pin())
			{
				Self->FinishClaudeLogin(Login, AuthStatus, bTimedOut);
			}
		});
	});
#else
	EnterSignedOut(EACFAssistantProvider::ClaudeCode, TEXT("Run `claude auth login` in a terminal, then press Sign in with Claude."));
#endif
}

void FAscentACFAssistant::FinishClaudeLogin(const TSharedPtr<FClaudeLoginProcess>& Login, const FString& AuthStatus, bool bTimedOut)
{
	// Canceled or replaced: whoever stopped it already set the state.
	if (Login != ClaudeLogin) { return; }
	ClaudeLogin.Reset();
	if (Claude.State != EConnectionState::SigningIn) { return; }
	if (bTimedOut)
	{
		EnterSignedOut(EACFAssistantProvider::ClaudeCode, TEXT("Claude sign-in timed out after 15 minutes. Press Sign in with Claude to try again."));
		return;
	}
	FString Problem;
	switch (ClassifyClaudeAuth(AuthStatus, &Problem))
	{
	case EClaudeAuth::Subscription:
	case EClaudeAuth::ApiBilling:
		CompleteClaudeConnect(AuthStatus);
		return;
	case EClaudeAuth::SignedOut:
		EnterSignedOut(EACFAssistantProvider::ClaudeCode, !Problem.IsEmpty() ? Problem : FString(TEXT("Claude sign-in was not completed. Press Sign in with Claude to try again.")));
		return;
	default:
		Fail(EACFAssistantProvider::ClaudeCode, Problem);
		return;
	}
}

void FAscentACFAssistant::StopClaudeLogin()
{
	if (!ClaudeLogin.IsValid()) { return; }
	ClaudeLogin->Stop();
	ClaudeLogin.Reset();
}

void FAscentACFAssistant::Disconnect()
{
	Disconnect(EACFAssistantProvider::Yarin);
	Disconnect(EACFAssistantProvider::Codex);
	Disconnect(EACFAssistantProvider::ClaudeCode);
}

void FAscentACFAssistant::Disconnect(EACFAssistantProvider InProvider)
{
	FProviderLink& Stopping = Link(InProvider);
	Stopping.State = EConnectionState::Disconnected;
	Stopping.bInitialized = false;
	Stopping.bAuthenticated = false;
	if (InProvider == EACFAssistantProvider::Yarin)
	{
		// The Yarin account has no process: stopping it forgets the connection, not the sign-in. The Codex runtime and the conversations on it are left alone.
		StopYarinLogin();
		SetProviderModels(EACFAssistantProvider::Yarin, {});
		return;
	}
	++Stopping.Generation;
	FTSTicker::GetCoreTicker().RemoveTicker(Stopping.Ticker);
	Stopping.Ticker.Reset();
	FlushProtocolLog();
	FailOneShots(InProvider, TEXT("Yarin disconnected before the reply finished."));
	SetProviderModels(InProvider, {});
	if (InProvider == EACFAssistantProvider::Codex)
	{
		// Its process is going away, and Yarin's models with it. The catalog is kept to republish when the runtime is back.
		Yarin.bInitialized = false;
		CodexGatewayKeyHash.Reset();
		SetProviderModels(EACFAssistantProvider::Yarin, {});
		FailOneShots(EACFAssistantProvider::Yarin, TEXT("Yarin disconnected before the reply finished."));
	}
	// Conversations survive a disconnect: their transcripts and thread ids stay, so reconnecting can resume them. Only this provider's are touched.
	for (const TSharedPtr<FACFAssistantSession>& Session : Sessions)
	{
		if (HarnessOf(Session->Provider) != HarnessOf(InProvider)) { continue; }
		SaveSession(Session);
		StopClaudeProcess(Session);
		DenyApprovals(Session, nullptr);
		Session->PendingText.Reset();
		Session->SteerText.Reset();
		Session->bBusy = false;
		Session->bCancelRequested = false;
		Session->TurnId.Reset();
		Session->bNeedsThreadResume = HarnessOf(Session->Provider) == EACFAssistantProvider::Codex && !Session->ThreadId.IsEmpty();
		++Session->TitleGeneration;
		Session->TitleThreadId.Reset();
		Session->TitleTurnId.Reset();
		Session->TitleText.Reset();
		Session->PendingTitleSource.Reset();
	}
	SaveHistory();
	if (InProvider == EACFAssistantProvider::ClaudeCode) { return; }
	// Everything below is the shared Codex app-server process.
	Pending.Reset();
	PendingCodexModels.Reset();
	LoginId.Reset();
	Incoming.Reset();
	bStopWriter = true;
#if PLATFORM_WINDOWS
	if (Writer.IsValid() && !Writer.WaitFor(FTimespan::FromMilliseconds(100)))
	{
		// The writer is inside a WriteFile on a full stdin. It returns only once every holder of the read end is gone,
		// or once the call is cancelled; a plain Wait() here froze Editor shutdown.
		EndProcessTree(ProcessJob, Process);
		// A thread killed by process exit never fulfils the future, so its signalled handle ends the wait too.
		const double Started = FPlatformTime::Seconds();
		while (!Writer.WaitFor(FTimespan::FromMilliseconds(50)))
		{
			void* Thread = WriterThread.load();
			if (!Thread && FPlatformTime::Seconds() - Started > 1.0) { break; }
			if (Thread && ::WaitForSingleObject(static_cast<HANDLE>(Thread), 0) != WAIT_TIMEOUT) { break; }
			if (Thread) { ::CancelSynchronousIo(static_cast<HANDLE>(Thread)); }
		}
	}
	if (void* Thread = WriterThread.exchange(nullptr)) { ::CloseHandle(static_cast<HANDLE>(Thread)); }
	TArray<uint8> Discarded;
	while (Outgoing.Dequeue(Discarded)) {}
	if (InputWrite) { ::CloseHandle(InputWrite); InputWrite = nullptr; }
	if (Process)
	{
		::WaitForSingleObject(Process, 100);
		EndProcessTree(ProcessJob, Process);
		::CloseHandle(Process);
		Process = nullptr;
	}
	if (ProcessJob) { EndProcessTree(ProcessJob, nullptr); }
	if (ProcessThread) { ::CloseHandle(ProcessThread); ProcessThread = nullptr; }
	if (OutputRead) { ::CloseHandle(OutputRead); OutputRead = nullptr; }
#endif
}

void FAscentACFAssistant::Write(const TSharedPtr<FJsonObject>& Message)
{
	if (InputWrite && !bStopWriter)
	{
		FString Line;
		FJsonSerializer::Serialize(Message.ToSharedRef(), TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Line));
		LogProtocol(TEXT("->"), Line);
		Line += TEXT("\n");
		FTCHARToUTF8 Converted(*Line);
		TArray<uint8> Bytes;
		Bytes.Append(reinterpret_cast<const uint8*>(Converted.Get()), Converted.Length());
		Outgoing.Enqueue(MoveTemp(Bytes));
	}
}

bool FAscentACFAssistant::TickCodex(float DeltaSeconds)
{
#if PLATFORM_WINDOWS
	if (!Process || bWriteFailed || ::WaitForSingleObject(Process, 0) != WAIT_TIMEOUT)
	{
		Disconnect(EACFAssistantProvider::Codex);
		Fail(EACFAssistantProvider::Codex, TEXT("The connection stopped. Reconnect and try again."));
		return false;
	}
	DWORD Available = 0;
	int32 ReadThisTick = 0;
	while (::PeekNamedPipe(OutputRead, nullptr, 0, nullptr, &Available, nullptr) && Available > 0 && ReadThisTick < 1024 * 1024)
	{
		uint8 Buffer[8192];
		DWORD Read = 0;
		if (!::ReadFile(OutputRead, Buffer, FMath::Min<DWORD>(Available, sizeof(Buffer)), &Read, nullptr) || Read == 0) { break; }
		Incoming.Append(Buffer, Read);
		ReadThisTick += Read;
		Codex.LastActivity = FPlatformTime::Seconds();
	}
	if (Incoming.Num() > 8 * 1024 * 1024)
	{
		Disconnect(EACFAssistantProvider::Codex);
		Fail(EACFAssistantProvider::Codex, TEXT("The connection returned invalid data. Reconnect and try again."));
		return false;
	}
	int32 Processed = 0;
	const double Deadline = FPlatformTime::Seconds() + 0.01;
	while (Processed++ < 100 && FPlatformTime::Seconds() < Deadline)
	{
		TSharedPtr<FJsonObject> Message;
		bool bInvalid = false;
		FString RawLine;
		const bool bDecoded = DecodeMessage(Incoming, Message, bInvalid, &RawLine);
		if (bInvalid)
		{
			LogProtocol(TEXT("<- invalid"), RawLine);
			Disconnect(EACFAssistantProvider::Codex);
			Fail(EACFAssistantProvider::Codex, TEXT("The connection returned invalid data. Reconnect and try again."));
			return false;
		}
		if (!bDecoded) { break; }
		LogProtocol(TEXT("<-"), RawLine);
		Handle(Message);
		if (!Process) { return false; }
	}
	const double Now = FPlatformTime::Seconds();
	// A change waiting on the Approve banner is the user's time, not the provider's: it must not count as silence.
	if (Sessions.ContainsByPredicate([](const TSharedPtr<FACFAssistantSession>& Session) { return Session.IsValid() && HarnessOf(Session->Provider) == EACFAssistantProvider::Codex && !Session->Approvals.IsEmpty(); }))
	{
		Codex.LastActivity = Now;
	}
	bool bTimedOut = false;
	for (const auto& Entry : Pending) { bTimedOut |= Entry.Value.bCritical && Now - Entry.Value.Started > 60; }
	if (bTimedOut)
	{
		Disconnect(EACFAssistantProvider::Codex);
		Fail(EACFAssistantProvider::Codex, TEXT("No response for 60 seconds. Reconnect and try again."));
		return false;
	}
	// LastActivity is stamped when a turn is sent, not only when bytes arrive. An idle Connected session must not
	// look timed-out the instant the first message marks it busy.
	if (IsAnyBusy(EACFAssistantProvider::Codex) && Codex.LastActivity > 0.0 && Now - Codex.LastActivity > 300)
	{
		Disconnect(EACFAssistantProvider::Codex);
		Fail(EACFAssistantProvider::Codex, TEXT("No response arrived for 5 minutes. Reconnect and resume."));
		return false;
	}
	for (int32 Index = OneShotCompletions.Num() - 1; Index >= 0; --Index)
	{
		const TSharedPtr<FOneShotCompletion>& Shot = OneShotCompletions[Index];
		if (Shot.IsValid() && Shot->Provider != EACFAssistantProvider::ClaudeCode && Now - Shot->Started > 180.0)
		{
			CompleteOneShot(Shot, false, FString::Printf(TEXT("%s did not answer within 3 minutes."), ProviderName(Shot->Provider)));
		}
	}
	// Everything this tick decoded goes to disk in one append. Every early return above runs through Disconnect or Fail, which flush too.
	FlushProtocolLog();
#endif
	return true;
}

void FAscentACFAssistant::StopOneShotProcess(const TSharedPtr<FOneShotCompletion>& Shot)
{
#if PLATFORM_WINDOWS
	if (!Shot.IsValid()) { return; }
	EndProcessTree(Shot->ClaudeJob, Shot->ClaudeProcess);
	if (Shot->ClaudeProcess)
	{
		::CloseHandle(static_cast<HANDLE>(Shot->ClaudeProcess));
		Shot->ClaudeProcess = nullptr;
	}
	if (Shot->ClaudeProcessThread)
	{
		::CloseHandle(static_cast<HANDLE>(Shot->ClaudeProcessThread));
		Shot->ClaudeProcessThread = nullptr;
	}
	if (Shot->ClaudeOutputRead)
	{
		::CloseHandle(static_cast<HANDLE>(Shot->ClaudeOutputRead));
		Shot->ClaudeOutputRead = nullptr;
	}
	Shot->ClaudeIncoming.Reset();
#else
	(void)Shot;
#endif
}

bool FAscentACFAssistant::StartClaudeOneShot(
	const TSharedPtr<FOneShotCompletion>& Shot,
	const FString& SystemInstructions,
	const FString& UserMessage)
{
#if PLATFORM_WINDOWS
	if (!Shot.IsValid()) { return false; }
	FString Program = UACFAssistantSettings::Get().ClaudeExecutable.TrimStartAndEnd().TrimQuotes();
	if (Program.IsEmpty()) { Program = FindClaudeExecutable(); }
	if (Program.IsEmpty() || !FPaths::FileExists(Program))
	{
		CompleteOneShot(Shot, false, TEXT("Install Claude Code first, then Sign in through Yarin."));
		return false;
	}

	const FString ClaudeDirectory = FPaths::Combine(ProfileDirectory, TEXT("Claude"));
	IFileManager::Get().MakeDirectory(*ClaudeDirectory, true);
	const FString PromptPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(ClaudeDirectory, FString::Printf(TEXT("oneshot-%d.txt"), Shot->Id)));
	const FString McpPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(ClaudeDirectory, TEXT("oneshot-mcp.json")));
	const FString Instructions = SystemInstructions
		+ TEXT("\r\n\r\nYou have no tools. Do not call tools, browse, or edit files. Reply with the requested text only.");
	if (!FFileHelper::SaveStringToFile(TEXT("{\r\n  \"mcpServers\": {}\r\n}\r\n"), *McpPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)
		|| !FFileHelper::SaveStringToFile(Instructions, *PromptPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		CompleteOneShot(Shot, false, TEXT("Could not write the Claude prompt for this translation batch."));
		return false;
	}

	TArray<WCHAR> EnvironmentBlock = ClaudeEnvironmentBlock();

	SECURITY_ATTRIBUTES Security = {sizeof(SECURITY_ATTRIBUTES), nullptr, true};
	HANDLE ChildOutput = nullptr;
	HANDLE ChildInput = nullptr;
	HANDLE ParentOutput = nullptr;
	HANDLE ParentInput = nullptr;
	bool bPipes = ::CreatePipe(&ParentOutput, &ChildOutput, &Security, 0) && ::CreatePipe(&ChildInput, &ParentInput, &Security, 0);
	bPipes = bPipes && ::SetHandleInformation(ParentOutput, HANDLE_FLAG_INHERIT, 0) && ::SetHandleInformation(ParentInput, HANDLE_FLAG_INHERIT, 0);
	STARTUPINFOW Startup = {};
	Startup.cb = sizeof(Startup);
	Startup.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
	Startup.wShowWindow = SW_HIDE;
	Startup.hStdInput = ChildInput;
	Startup.hStdOutput = ChildOutput;
	Startup.hStdError = ChildOutput;
	PROCESS_INFORMATION Info = {};
	const FString SessionId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
	const FString ClaudeModel = DefaultModelFor(EACFAssistantProvider::ClaudeCode);
	const FString Model = ClaudeModel.IsEmpty() ? TEXT("sonnet") : ClaudeModel;
	FString Command = FString::Printf(
		TEXT("\"%s\" -p --verbose --no-chrome --output-format stream-json --include-partial-messages --input-format text --strict-mcp-config --mcp-config \"%s\" --append-system-prompt-file \"%s\" --permission-mode bypassPermissions --permission-prompts none --model %s --disallowedTools Bash PowerShell Edit Write NotebookEdit WebSearch WebFetch Read Glob Grep Agent Task --session-id %s"),
		*Program, *McpPath, *PromptPath, *Model, *SessionId);
	FString ProjectDirectory = FPaths::ConvertRelativePathToFull(FPaths::GetPath(FPaths::GetProjectFilePath()));
	FPaths::MakePlatformFilename(ProjectDirectory);
	TArray<TCHAR> CommandLine = Command.GetCharArray();
	HANDLE Job = CreateProcessTreeJob();
	const bool bStarted = bPipes && CommandLine.Num() > 0 && ::CreateProcessW(*Program, CommandLine.GetData(), nullptr, nullptr, true,
		CREATE_SUSPENDED | CREATE_UNICODE_ENVIRONMENT, EnvironmentBlock.GetData(), *ProjectDirectory, &Startup, &Info);
	for (HANDLE Handle : {ChildOutput, ChildInput})
	{
		if (Handle && Handle != INVALID_HANDLE_VALUE) { ::CloseHandle(Handle); }
	}
	if (!bStarted)
	{
		if (Job) { ::CloseHandle(Job); }
		if (ParentOutput) { ::CloseHandle(ParentOutput); }
		if (ParentInput) { ::CloseHandle(ParentInput); }
		CompleteOneShot(Shot, false, TEXT("Yarin could not start Claude for this translation batch."));
		return false;
	}
	ResumeInJob(Job, Info);
	Shot->ClaudeJob = Job;
	Shot->ClaudeProcess = Info.hProcess;
	Shot->ClaudeProcessThread = Info.hThread;
	Shot->ClaudeOutputRead = ParentOutput;
	Shot->ThreadId = SessionId;
	FTCHARToUTF8 Converted(*(UserMessage + TEXT("\n")));
	DWORD Written = 0;
	const bool bWrote = ::WriteFile(ParentInput, Converted.Get(), Converted.Length(), &Written, nullptr) && Written > 0;
	::CloseHandle(ParentInput);
	if (!bWrote)
	{
		CompleteOneShot(Shot, false, TEXT("Yarin could not send this batch to Claude."));
		return false;
	}
	LogProtocol(TEXT("-> claude oneshot"), Command.Left(500));
	return true;
#else
	CompleteOneShot(Shot, false, TEXT("Claude translation is currently supported on Windows Editor builds."));
	return false;
#endif
}

void FAscentACFAssistant::TickClaudeOneShots(double Now)
{
#if PLATFORM_WINDOWS
	for (int32 Index = OneShotCompletions.Num() - 1; Index >= 0; --Index)
	{
		TSharedPtr<FOneShotCompletion> Shot = OneShotCompletions[Index];
		if (!Shot.IsValid() || Shot->Provider != EACFAssistantProvider::ClaudeCode) { continue; }
		if (Now - Shot->Started > 180.0)
		{
			CompleteOneShot(Shot, false, TEXT("Claude did not answer within 3 minutes."));
			continue;
		}
		if (!Shot->ClaudeProcess || !Shot->ClaudeOutputRead) { continue; }
		const bool bAlive = ::WaitForSingleObject(static_cast<HANDLE>(Shot->ClaudeProcess), 0) == WAIT_TIMEOUT;
		DWORD Available = 0;
		int32 ReadThisTick = 0;
		while (::PeekNamedPipe(static_cast<HANDLE>(Shot->ClaudeOutputRead), nullptr, 0, nullptr, &Available, nullptr)
			&& Available > 0 && ReadThisTick < 1024 * 1024)
		{
			uint8 Buffer[8192];
			DWORD Read = 0;
			if (!::ReadFile(static_cast<HANDLE>(Shot->ClaudeOutputRead), Buffer, FMath::Min<DWORD>(Available, sizeof(Buffer)), &Read, nullptr) || Read == 0) { break; }
			Shot->ClaudeIncoming.Append(Buffer, Read);
			ReadThisTick += Read;
		}
		int32 Processed = 0;
		bool bFinished = false;
		while (Processed++ < 100 && !bFinished)
		{
			TSharedPtr<FJsonObject> Message;
			bool bInvalid = false;
			FString RawLine;
			const bool bDecoded = DecodeMessage(Shot->ClaudeIncoming, Message, bInvalid, &RawLine);
			if (!bDecoded && !bInvalid) { break; }
			if (bInvalid || !Message.IsValid()) { continue; }
			const FString Type = String(Message, TEXT("type"));
			if (Type == TEXT("stream_event"))
			{
				const FObject Event = Child(Message, TEXT("event"));
				if (String(Event, TEXT("type")) == TEXT("content_block_delta"))
				{
					const FObject Delta = Child(Event, TEXT("delta"));
					if (!String(Delta, TEXT("type")).Contains(TEXT("thinking")))
					{
						const FString Text = String(Delta, TEXT("text"));
						if (!Text.IsEmpty())
						{
							Shot->AccumulatedText += Text;
							Shot->bClaudeSawText = true;
						}
					}
				}
			}
			else if (Type == TEXT("assistant") && !Shot->bClaudeSawText)
			{
				const FObject Assistant = Child(Message, TEXT("message"));
				for (const auto& Value : Array(Assistant, TEXT("content")))
				{
					if (Value->Type != EJson::Object) { continue; }
					const FObject Block = Value->AsObject();
					if (String(Block, TEXT("type")) == TEXT("text"))
					{
						Shot->AccumulatedText += String(Block, TEXT("text"));
					}
				}
			}
			else if (Type == TEXT("result"))
			{
				bool bError = false;
				Message->TryGetBoolField(TEXT("is_error"), bError);
				const FString ResultText = String(Message, TEXT("result"));
				if (bError || String(Message, TEXT("subtype")).Contains(TEXT("error")))
				{
					CompleteOneShot(Shot, false, ResultText.IsEmpty() ? TEXT("Claude could not complete this batch.") : ResultText.Left(1500));
				}
				else
				{
					const FString Answer = Shot->AccumulatedText.TrimStartAndEnd().IsEmpty() ? ResultText : Shot->AccumulatedText;
					CompleteOneShot(Shot, !Answer.TrimStartAndEnd().IsEmpty(),
						Answer.TrimStartAndEnd().IsEmpty() ? TEXT("Claude returned an empty reply.") : Answer);
				}
				bFinished = true;
			}
			else if (Type == TEXT("error"))
			{
				FString ErrorText = String(Message, TEXT("error"));
				if (ErrorText.IsEmpty()) { ErrorText = String(Child(Message, TEXT("error")), TEXT("message")); }
				CompleteOneShot(Shot, false, ErrorText.IsEmpty() ? TEXT("Claude reported a problem.") : ErrorText.Left(1500));
				bFinished = true;
			}
		}
		if (!bFinished && !bAlive && OneShotCompletions.Contains(Shot))
		{
			const FString Answer = Shot->AccumulatedText.TrimStartAndEnd();
			CompleteOneShot(Shot, !Answer.IsEmpty(), Answer.IsEmpty() ? TEXT("Claude stopped before answering.") : Answer);
		}
	}
#else
	(void)Now;
#endif
}

void FAscentACFAssistant::StopClaudeProcess(const TSharedPtr<FACFAssistantSession>& Session)
{
#if PLATFORM_WINDOWS
	if (!Session.IsValid()) { return; }
	EndProcessTree(Session->ClaudeJob, Session->ClaudeProcess);
	if (Session->ClaudeProcess)
	{
		::CloseHandle(Session->ClaudeProcess);
		Session->ClaudeProcess = nullptr;
	}
	if (Session->ClaudeProcessThread) { ::CloseHandle(Session->ClaudeProcessThread); Session->ClaudeProcessThread = nullptr; }
	if (Session->ClaudeOutputRead) { ::CloseHandle(Session->ClaudeOutputRead); Session->ClaudeOutputRead = nullptr; }
	Session->ClaudeIncoming.Reset();
#endif
}

void FAscentACFAssistant::StartClaudeTurn(const TSharedPtr<FACFAssistantSession>& Session)
{
#if PLATFORM_WINDOWS
	if (!Session.IsValid()) { return; }
	StopClaudeProcess(Session);
	FString Program = UACFAssistantSettings::Get().ClaudeExecutable.TrimStartAndEnd().TrimQuotes();
	if (Program.IsEmpty()) { Program = FindClaudeExecutable(); }
	if (Program.IsEmpty() || !FPaths::FileExists(Program))
	{
		FailSession(Session, TEXT("Install Claude Code first, then Sign in."));
		return;
	}
	const FString ClaudeDirectory = FPaths::Combine(ProfileDirectory, TEXT("Claude"));
	const FString McpPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(ClaudeDirectory, FString::Printf(TEXT("mcp-%d.json"), Session->LocalId)));
	if (!FFileHelper::SaveStringToFile(ClaudeMcpConfiguration(GateUrlFor(Session)), *McpPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		FailSession(Session, TEXT("Yarin could not prepare its working folder. Check disk permissions."));
		return;
	}
	const FString PromptPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(ClaudeDirectory, TEXT("system-prompt.txt")));
	FFileHelper::SaveStringToFile(Instructions(), *PromptPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	const FString Prompt = ClaudeTurnPrompt(Session);
	Session->PendingText.Reset();
	Session->PendingImages.Reset();
	Session->bClaudeGotResult = false;
	Session->TurnId = Session->ThreadId;
	Claude.LastActivity = FPlatformTime::Seconds();

	// An Editor tool held on the Approve banner must not hit Claude's MCP tool timeout while the user is still reading.
	TArray<WCHAR> EnvironmentBlock = ClaudeEnvironmentBlock(true);

	SECURITY_ATTRIBUTES Security = {sizeof(SECURITY_ATTRIBUTES), nullptr, true};
	HANDLE ChildOutput = nullptr;
	HANDLE ChildInput = nullptr;
	HANDLE ParentOutput = nullptr;
	HANDLE ParentInput = nullptr;
	bool bPipes = ::CreatePipe(&ParentOutput, &ChildOutput, &Security, 0) && ::CreatePipe(&ChildInput, &ParentInput, &Security, 0);
	bPipes = bPipes && ::SetHandleInformation(ParentOutput, HANDLE_FLAG_INHERIT, 0) && ::SetHandleInformation(ParentInput, HANDLE_FLAG_INHERIT, 0);
	STARTUPINFOW Startup = {};
	Startup.cb = sizeof(Startup);
	Startup.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
	Startup.wShowWindow = SW_HIDE;
	Startup.hStdInput = ChildInput;
	Startup.hStdOutput = ChildOutput;
	// stderr shares stdout so a startup failure is visible in the chat instead of being discarded.
	Startup.hStdError = ChildOutput;
	PROCESS_INFORMATION Info = {};
	// Approve all lifts the conversation to bypassPermissions; until then Ask for approval runs in default mode and Claude's own prompts
	// (Bash, Edit, Write, ...) are routed to the gate's permission tool, where the Approve / Deny buttons answer them.
	const bool bAsksForApproval = Session->Permissions.AsksForApproval();
	const TCHAR* Permission = bAsksForApproval ? TEXT("default") : ClaudePermissionModeName(Session->Permissions.AutoApproves() || Session->Permissions.bConversationApproved
		? EACFAssistantWorkMode::ApproveForMe : Session->Permissions.WorkMode);
	// --disallowedTools is variadic: the names are separate space-separated arguments, not one comma-joined value.
	// Shell, web, and source edits are on unless the matching setting is unchecked.
	const UACFAssistantSettings& AssistantSettings = UACFAssistantSettings::Get();
	FString DisallowedTools = TEXT("NotebookEdit");
	if (!AssistantSettings.bAllowShell)
	{
		DisallowedTools += TEXT(" Bash PowerShell");
	}
	if (!AssistantSettings.bAllowWeb)
	{
		DisallowedTools += TEXT(" WebSearch WebFetch");
	}
	if (!AssistantSettings.bAllowSourceFileEdits)
	{
		DisallowedTools += TEXT(" Edit Write");
	}
	// Editor tools are pre-allowed for Claude because the gate itself holds the mutating ones; without this every Editor call would prompt twice.
	const FString PermissionPrompts = bAsksForApproval
		? FString::Printf(TEXT("--permission-prompts host --permission-prompt-tool mcp__ue-editor__%s --allowedTools mcp__ue-editor"), ACFAssistantToolGate::ClaudePermissionToolName)
		: TEXT("--permission-prompts none --allowedTools mcp__ue-editor");
	const FString TurnModel = SessionModelId(Session);
	FString Command = FString::Printf(
		TEXT("\"%s\" -p --verbose --no-chrome --output-format stream-json --include-partial-messages --input-format text --strict-mcp-config --mcp-config \"%s\" --append-system-prompt-file \"%s\" --permission-mode %s %s --model %s --disallowedTools %s"),
		*Program, *McpPath, *PromptPath, Permission, *PermissionPrompts, *TurnModel, *DisallowedTools);
	const FString TurnEffort = EffortFor(TurnModel);
	if (!TurnEffort.IsEmpty()) { Command += FString::Printf(TEXT(" --effort %s"), *TurnEffort); }
	// Plan mode denies reads outside the working directory, and engine headers are the reference for most C++ work.
	FString EngineDirectory = FPaths::ConvertRelativePathToFull(FPaths::EngineDir());
	FPaths::NormalizeDirectoryName(EngineDirectory);
	FPaths::MakePlatformFilename(EngineDirectory);
	if (!EngineDirectory.IsEmpty()) { Command += FString::Printf(TEXT(" --add-dir \"%s\""), *EngineDirectory); }
	FString PluginDirectory = ClaudePluginDirectory();
	FPaths::MakePlatformFilename(PluginDirectory);
	if (IFileManager::Get().DirectoryExists(*PluginDirectory)) { Command += FString::Printf(TEXT(" --plugin-dir \"%s\""), *PluginDirectory); }
	if (Session->bClaudeHasStarted)
	{
		Command += FString::Printf(TEXT(" --resume %s"), *Session->ThreadId);
	}
	else
	{
		Command += FString::Printf(TEXT(" --session-id %s"), *Session->ThreadId);
	}
	FString ProjectDirectory = FPaths::ConvertRelativePathToFull(FPaths::GetPath(FPaths::GetProjectFilePath()));
	FPaths::MakePlatformFilename(ProjectDirectory);
	// SW_HIDE keeps the console off screen. CREATE_NO_WINDOW removes it, and the bun-based CLI then writes nothing.
	TArray<TCHAR> CommandLine = Command.GetCharArray();
	HANDLE Job = CreateProcessTreeJob();
	const bool bStarted = bPipes && CommandLine.Num() > 0 && ::CreateProcessW(*Program, CommandLine.GetData(), nullptr, nullptr, true,
		CREATE_SUSPENDED | CREATE_UNICODE_ENVIRONMENT, EnvironmentBlock.GetData(), *ProjectDirectory, &Startup, &Info);
	for (HANDLE Handle : {ChildOutput, ChildInput})
	{
		if (Handle && Handle != INVALID_HANDLE_VALUE) { ::CloseHandle(Handle); }
	}
	if (!bStarted)
	{
		if (Job) { ::CloseHandle(Job); }
		if (ParentOutput) { ::CloseHandle(ParentOutput); }
		if (ParentInput) { ::CloseHandle(ParentInput); }
		FailSession(Session, TEXT("Yarin could not start Claude Code. Install it and try again."));
		return;
	}
	ResumeInJob(Job, Info);
	Session->ClaudeJob = Job;
	Session->ClaudeProcess = Info.hProcess;
	Session->ClaudeProcessThread = Info.hThread;
	Session->ClaudeOutputRead = ParentOutput;
	// bClaudeHasStarted stays false until Claude accepts the session. Setting it here made the next send
	// --resume a session that was never created, so every retry died the same way.
	FTCHARToUTF8 Converted(*(Prompt + TEXT("\n")));
	DWORD Written = 0;
	const bool bWrote = ::WriteFile(ParentInput, Converted.Get(), Converted.Length(), &Written, nullptr) && Written > 0;
	::CloseHandle(ParentInput);
	if (!bWrote)
	{
		FailSession(Session, TEXT("Yarin could not send the prompt to Claude Code. Try again."));
		return;
	}
	LogProtocol(TEXT("-> claude"), Command.Left(500));
#else
	FailSession(Session, TEXT("Yarin chat is currently supported on Windows Editor builds."));
#endif
}

bool FAscentACFAssistant::TickClaude(float DeltaSeconds)
{
#if PLATFORM_WINDOWS
	if (Claude.State != EConnectionState::Ready && Claude.State != EConnectionState::Connecting) { return false; }
	const double Now = FPlatformTime::Seconds();
	for (const TSharedPtr<FACFAssistantSession>& Session : Sessions)
	{
		if (!Session.IsValid() || !Session->ClaudeProcess) { continue; }
		const bool bAlive = ::WaitForSingleObject(Session->ClaudeProcess, 0) == WAIT_TIMEOUT;
		DWORD Available = 0;
		int32 ReadThisTick = 0;
		while (Session->ClaudeOutputRead && ::PeekNamedPipe(Session->ClaudeOutputRead, nullptr, 0, nullptr, &Available, nullptr)
			&& Available > 0 && ReadThisTick < 1024 * 1024)
		{
			uint8 Buffer[8192];
			DWORD Read = 0;
			if (!::ReadFile(Session->ClaudeOutputRead, Buffer, FMath::Min<DWORD>(Available, sizeof(Buffer)), &Read, nullptr) || Read == 0) { break; }
			Session->ClaudeIncoming.Append(Buffer, Read);
			ReadThisTick += Read;
			Claude.LastActivity = Now;
		}
		if (Session->ClaudeIncoming.Num() > 8 * 1024 * 1024)
		{
			CompleteClaudeTurn(Session, TEXT("The connection returned invalid data. Try again."));
			continue;
		}
		int32 Processed = 0;
		const double Deadline = FPlatformTime::Seconds() + 0.01;
		FString ClaudeFailure;
		// Once the process has exited, finish every line already in the pipe. The 10ms budget is only for while it is still streaming.
		while (Processed++ < 100 && (!bAlive || FPlatformTime::Seconds() < Deadline))
		{
			TSharedPtr<FJsonObject> Message;
			bool bInvalid = false;
			FString RawLine;
			const bool bDecoded = DecodeMessage(Session->ClaudeIncoming, Message, bInvalid, &RawLine);
			if (!bDecoded && !bInvalid) { break; }
			LogProtocol(TEXT("<-"), RawLine);
			if (bInvalid)
			{
				const FString Trimmed = RawLine.TrimStartAndEnd();
				if (!Trimmed.IsEmpty()) { ClaudeFailure = Trimmed.Left(1500); }
				continue;
			}
			// The first line of every turn names the exact model the alias resolved to (e.g. opus -> claude-opus-5-5); the dropdown shows its version from then on.
			if (String(Message, TEXT("type")) == TEXT("system") && String(Message, TEXT("subtype")) == TEXT("init"))
			{
				NoteClaudeResolvedModel(Session->ModelId, String(Message, TEXT("model")));
			}
			FString StreamError;
			const bool bTurnFinished = HandleClaudeStreamLine(*Session, Message, StreamError);
			// The window a result reports is kept on the model, so the next conversation on it shows the real size from its first call instead of nothing.
			if (String(Message, TEXT("type")) == TEXT("result") && Session->ReportedContextWindowTokens > 0)
			{
				if (const TSharedPtr<FACFAssistantModel> Model = FindModel(Session->ModelId)) { Model->ContextWindowTokens = Session->ReportedContextWindowTokens; }
			}
			if (bTurnFinished)
			{
				CompleteClaudeTurn(Session, StreamError);
				break;
			}
			if (!StreamError.IsEmpty())
			{
				CompleteClaudeTurn(Session, StreamError);
				break;
			}
			Changed.Broadcast();
		}
		if (Session->ClaudeProcess && !bAlive && !Session->bClaudeGotResult && Session->ClaudeIncoming.Num() > 0)
		{
			const FUTF8ToTCHAR Leftover(reinterpret_cast<const ANSICHAR*>(Session->ClaudeIncoming.GetData()), Session->ClaudeIncoming.Num());
			const FString Tail(Leftover.Length(), Leftover.Get());
			const FString Trimmed = Tail.TrimStartAndEnd();
			if (!Trimmed.IsEmpty()) { ClaudeFailure = Trimmed.Left(1500); }
		}
		if (Session->ClaudeProcess && !bAlive && !Session->bClaudeGotResult)
		{
			CompleteClaudeTurn(Session, ClaudeFailure.IsEmpty()
				? TEXT("Yarin could not complete this response. Try again; if it repeats, reconnect from Settings.")
				: ClaudeFailure);
		}
	}
	if (Sessions.ContainsByPredicate([](const TSharedPtr<FACFAssistantSession>& Session) { return Session.IsValid() && Session->Provider == EACFAssistantProvider::ClaudeCode && !Session->Approvals.IsEmpty(); }))
	{
		Claude.LastActivity = Now;
	}
	if (IsAnyBusy(EACFAssistantProvider::ClaudeCode) && Claude.LastActivity > 0.0 && Now - Claude.LastActivity > 300)
	{
		for (const TSharedPtr<FACFAssistantSession>& Session : Sessions)
		{
			if (Session->bBusy && Session->Provider == EACFAssistantProvider::ClaudeCode)
			{
				CompleteClaudeTurn(Session, TEXT("No response arrived for 5 minutes. You can continue this conversation."));
			}
		}
	}
	TickClaudeOneShots(Now);
	FlushProtocolLog();
	return true;
#else
	return false;
#endif
}
