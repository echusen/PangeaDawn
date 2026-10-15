// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "Commands/UEPythonCommands.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "HAL/FileManager.h"
#include "IPythonScriptPlugin.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
    bool ContainsAnyToken(const FString& Haystack, const TArray<FString>& Tokens, FString& OutToken)
    {
        for (const FString& Token : Tokens)
        {
            if (Haystack.Contains(Token, ESearchCase::IgnoreCase))
            {
                OutToken = Token;
                return true;
            }
        }
        return false;
    }

    TArray<FString> GetStringArrayField(const TSharedPtr<FJsonObject>& Params, const TCHAR* FieldName)
    {
        TArray<FString> Result;
        if (!Params.IsValid())
        {
            return Result;
        }

        const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
        if (!Params->TryGetArrayField(FieldName, Values) || !Values)
        {
            return Result;
        }

        for (const TSharedPtr<FJsonValue>& Value : *Values)
        {
            FString Text;
            if (Value.IsValid() && Value->TryGetString(Text))
            {
                Result.Add(Text);
            }
        }
        return Result;
    }
}

bool FUEPythonCommands::IsCommand(const FString& CommandType) const
{
    return CommandType == TEXT("ue_py_run");
}

FString FUEPythonCommands::HandleCommand(const FString& /*CommandType*/, const TSharedPtr<FJsonObject>& Params)
{
    // Single verb domain — CommandType is always ue_py_run; dispatch directly.
    return HandleRun(Params);
}

FString FUEPythonCommands::ValidateRequest(
    const FString& Code,
    const FString& Mode,
    const TArray<FString>& ExpectedPaths) const
{
    if (Code.TrimStartAndEnd().IsEmpty())
    {
        return TEXT("[ue_py] code is empty");
    }

    static const TSet<FString> ValidModes = {
        TEXT("read_only"),
        TEXT("mutate_asset"),
        TEXT("mutate_level"),
    };
    if (!ValidModes.Contains(Mode))
    {
        return TEXT("[ue_py] mode must be one of: read_only, mutate_asset, mutate_level");
    }

    FString Hit;
    static const TArray<FString> AlwaysBlockedTokens = {
        TEXT("subprocess"),
        TEXT("socket"),
        TEXT("requests"),
        TEXT("urllib"),
        TEXT("http.client"),
        TEXT("os.system"),
        TEXT("popen("),
        TEXT("rmtree"),
        TEXT("shutil."),
        TEXT(".remove("),
        TEXT(".unlink("),
        TEXT("eval("),
        TEXT("exec("),
        TEXT("compile("),
        TEXT("__import__"),
        TEXT("importlib"),
        TEXT("open(")
    };
    if (ContainsAnyToken(Code, AlwaysBlockedTokens, Hit))
    {
        return FString::Printf(TEXT("[ue_py] blocked unsafe Python token: %s"), *Hit);
    }

    static const TArray<FString> MutatingTokens = {
        TEXT("save_asset"),
        TEXT("save_loaded_asset"),
        TEXT("delete_asset"),
        TEXT("rename_asset"),
        TEXT("duplicate_asset"),
        TEXT("make_directory"),
        TEXT("set_editor_property"),
        TEXT("spawn_actor"),
        TEXT("destroy_actor"),
        TEXT("set_actor_"),
        TEXT("add_component"),
        TEXT("remove_component"),
        TEXT("create_asset"),
        TEXT("factory_create_new")
    };
    if (Mode == TEXT("read_only") && ContainsAnyToken(Code, MutatingTokens, Hit))
    {
        return FString::Printf(TEXT("[ue_py] read_only mode cannot use mutating token: %s"), *Hit);
    }

    if (Mode == TEXT("mutate_asset"))
    {
        if (ExpectedPaths.IsEmpty())
        {
            return TEXT("[ue_py] mutate_asset requires expected_paths so the intended asset scope is explicit");
        }
        for (const FString& Path : ExpectedPaths)
        {
            if (!Path.StartsWith(TEXT("/Game/")))
            {
                return FString::Printf(TEXT("[ue_py] mutate_asset path must be under /Game: %s"), *Path);
            }
            if (!Code.Contains(Path))
            {
                return FString::Printf(TEXT("[ue_py] expected path is not referenced by the script: %s"), *Path);
            }
        }
    }

    return FString();
}

FString FUEPythonCommands::HandleRun(const TSharedPtr<FJsonObject>& Params)
{
    if (!Params.IsValid())
    {
        return MakeErrorResponse(TEXT("[ue_py] missing params"));
    }

    FString Code;
    if (!Params->TryGetStringField(TEXT("code"), Code))
    {
        return MakeErrorResponse(TEXT("[ue_py] missing 'code' parameter"));
    }

    FString Mode = TEXT("read_only");
    Params->TryGetStringField(TEXT("mode"), Mode);

    const TArray<FString> ExpectedPaths = GetStringArrayField(Params, TEXT("expected_paths"));
    const FString ValidationError = ValidateRequest(Code, Mode, ExpectedPaths);
    if (!ValidationError.IsEmpty())
    {
        return MakeErrorResponse(ValidationError);
    }

    IPythonScriptPlugin* PythonPlugin = IPythonScriptPlugin::Get();
    if (!PythonPlugin)
    {
        return MakeErrorResponse(TEXT("[ue_py] Python plugin not available. Enable PythonScriptPlugin / Python Editor Script Plugin."));
    }

    FString OutputFile = FPaths::ProjectSavedDir() / TEXT("ACFAgent") / TEXT("python_output.txt");
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(OutputFile), true);
    const FString OutputPath = OutputFile.Replace(TEXT("\\"), TEXT("/"));

    FString EscapedCode = Code;
    EscapedCode.ReplaceInline(TEXT("\\"), TEXT("\\\\"));
    EscapedCode.ReplaceInline(TEXT("\"\"\""), TEXT("\\\"\\\"\\\""));

    const FString WrapperCode = FString::Printf(TEXT(
        "import sys, io\n"
        "_acf_agent_buf = io.StringIO()\n"
        "_acf_agent_old = sys.stdout, sys.stderr\n"
        "sys.stdout = sys.stderr = _acf_agent_buf\n"
        "try:\n"
        "    exec(\"\"\"%s\"\"\")\n"
        "except Exception:\n"
        "    import traceback\n"
        "    traceback.print_exc()\n"
        "finally:\n"
        "    sys.stdout, sys.stderr = _acf_agent_old\n"
        "    with open(r'%s', 'w', encoding='utf-8') as f:\n"
        "        f.write(_acf_agent_buf.getvalue())\n"
    ), *EscapedCode, *OutputPath);

    const bool bExecAccepted = PythonPlugin->ExecPythonCommand(*WrapperCode);

    FString Output;
    FFileHelper::LoadFileToString(Output, *OutputFile);
    IFileManager::Get().Delete(*OutputFile);

    const bool bTraceback = Output.Contains(TEXT("Traceback"));
    if (!bExecAccepted || bTraceback)
    {
        return MakeErrorResponse(Output.IsEmpty()
            ? TEXT("[ue_py] Python execution failed")
            : FString::Printf(TEXT("[ue_py] Python execution failed:\n%s"), *Output));
    }

    const TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("mode"), Mode);
    Data->SetStringField(TEXT("output"), Output.IsEmpty() ? TEXT("Executed successfully") : Output);
    Data->SetNumberField(TEXT("output_length"), Output.Len());
    return MakeSuccessResponse(Data);
}
