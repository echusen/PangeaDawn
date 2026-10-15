// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "Toolsets/AscentSystemToolset.h"

#include "Toolsets/AscentToolsetUtils.h"

#if ACF_WITH_LIVE_CODING
#include "ILiveCodingModule.h"
#include "Modules/ModuleManager.h"
#endif

namespace
{
#if ACF_WITH_LIVE_CODING
    ILiveCodingModule* LiveCodingModule()
    {
        return FModuleManager::GetModulePtr<ILiveCodingModule>(LIVE_CODING_MODULE_NAME);
    }

    const TCHAR* CompileResultName(ELiveCodingCompileResult Result)
    {
        switch (Result)
        {
        case ELiveCodingCompileResult::Success: return TEXT("Success");
        case ELiveCodingCompileResult::NoChanges: return TEXT("NoChanges");
        case ELiveCodingCompileResult::InProgress: return TEXT("InProgress");
        case ELiveCodingCompileResult::CompileStillActive: return TEXT("CompileStillActive");
        case ELiveCodingCompileResult::NotStarted: return TEXT("NotStarted");
        case ELiveCodingCompileResult::Failure: return TEXT("Failure");
        case ELiveCodingCompileResult::Cancelled: return TEXT("Cancelled");
        default: return TEXT("Unknown");
        }
    }
#endif

    // Empty when Live Coding can compile now; otherwise the reason to report to the model.
    FString LiveCodingUnavailableReason()
    {
#if ACF_WITH_LIVE_CODING
        const ILiveCodingModule* Module = LiveCodingModule();
        if (!Module) { return TEXT("The Live Coding module is not loaded in this Editor."); }
        if (!Module->IsEnabledForSession())
        {
            const FString Error = Module->GetEnableErrorText().ToString();
            return Error.IsEmpty() ? TEXT("Live Coding is disabled for this Editor session (Editor Preferences > Live Coding).") : Error;
        }
        return FString();
#else
        return TEXT("Live Coding is only available in the Windows Editor.");
#endif
    }
}

FString UAscentSystemToolset::PythonRun(
    const FString& Code,
    const FString& Mode,
    const FString& ExpectedPathsJson)
{
    const TSharedPtr<FJsonObject> Params = MakeShared<FJsonObject>();
    Params->SetStringField(TEXT("code"), Code);
    Params->SetStringField(TEXT("mode"), Mode);

    if (!ExpectedPathsJson.IsEmpty())
    {
        TArray<TSharedPtr<FJsonValue>> ExpectedPaths;
        if (!FAscentToolsetUtils::ParseArray(ExpectedPathsJson, ExpectedPaths))
        {
            return FAscentToolsetUtils::MakeErrorResponse(
                TEXT("[AscentSystem] ExpectedPathsJson is not a valid JSON array"));
        }
        Params->SetArrayField(TEXT("expected_paths"), ExpectedPaths);
    }

    return FAscentToolsetUtils::Dispatch(TEXT("ue_py_run"), Params);
}

FString UAscentSystemToolset::TriggerLiveCoding()
{
    const TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    const FString Unavailable = LiveCodingUnavailableReason();
    if (!Unavailable.IsEmpty())
    {
        Data->SetStringField(TEXT("result"), TEXT("Unavailable"));
        Data->SetStringField(TEXT("message"), Unavailable);
        return FAscentToolsetUtils::MakeSuccessResponse(Data);
    }
#if ACF_WITH_LIVE_CODING
    ILiveCodingModule* Module = LiveCodingModule();
    if (Module->IsCompiling())
    {
        Data->SetStringField(TEXT("result"), TEXT("CompileStillActive"));
        return FAscentToolsetUtils::MakeSuccessResponse(Data);
    }
    // Waiting here would freeze the Editor for the whole compile, so the call returns as soon as the compile starts.
    ELiveCodingCompileResult Result = ELiveCodingCompileResult::NotStarted;
    Module->Compile(ELiveCodingCompileFlags::None, &Result);
    Data->SetStringField(TEXT("result"), CompileResultName(Result));
    if (Result == ELiveCodingCompileResult::InProgress)
    {
        Data->SetStringField(TEXT("message"), TEXT("Compile started. Poll GetLiveCodingStatus until compiling is false, then read the LogLiveCoding log entries for the outcome."));
    }
#endif
    return FAscentToolsetUtils::MakeSuccessResponse(Data);
}

FString UAscentSystemToolset::GetLiveCodingStatus()
{
    const TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    const FString Unavailable = LiveCodingUnavailableReason();
#if ACF_WITH_LIVE_CODING
    const ILiveCodingModule* Module = LiveCodingModule();
    Data->SetBoolField(TEXT("available"), Module != nullptr);
    Data->SetBoolField(TEXT("enabled"), Module && Module->IsEnabledForSession());
    Data->SetBoolField(TEXT("started"), Module && Module->HasStarted());
    Data->SetBoolField(TEXT("compiling"), Module && Module->IsCompiling());
#else
    Data->SetBoolField(TEXT("available"), false);
#endif
    if (!Unavailable.IsEmpty()) { Data->SetStringField(TEXT("message"), Unavailable); }
    return FAscentToolsetUtils::MakeSuccessResponse(Data);
}
