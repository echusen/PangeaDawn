// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "Toolsets/AscentToolsetUtils.h"

#include "AscentAIAgentSubsystem.h"
#include "Editor.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Toolsets/AscentAnimGraphToolset.h"
#include "Toolsets/AscentDataToolset.h"
#include "Toolsets/AscentGenerationToolset.h"
#include "Toolsets/AscentGraphToolset.h"
#include "Toolsets/AscentSystemToolset.h"

FString FAscentToolsetUtils::JsonToString(const TSharedPtr<FJsonObject>& Obj)
{
    FString Result;
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Result);
    FJsonSerializer::Serialize(Obj.ToSharedRef(), Writer);
    return Result;
}

FString FAscentToolsetUtils::JsonToString(const TArray<TSharedPtr<FJsonValue>>& Arr)
{
    FString Result;
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Result);
    FJsonSerializer::Serialize(Arr, Writer);
    return Result;
}

FString FAscentToolsetUtils::MakeSuccessResponse(const TSharedPtr<FJsonObject>& Data)
{
    const TSharedRef<FJsonObject> Response = MakeShared<FJsonObject>();
    Response->SetStringField(TEXT("status"), TEXT("success"));
    Response->SetBoolField(TEXT("success"), true);
    if (Data.IsValid())
    {
        for (const auto& Pair : Data->Values)
        {
            Response->SetField(Pair.Key, Pair.Value);
        }
    }
    return JsonToString(Response);
}

FString FAscentToolsetUtils::MakeErrorResponse(const FString& ErrorMessage)
{
    const TSharedRef<FJsonObject> Response = MakeShared<FJsonObject>();
    Response->SetStringField(TEXT("status"), TEXT("error"));
    Response->SetBoolField(TEXT("success"), false);
    Response->SetStringField(TEXT("error"), ErrorMessage);
    return JsonToString(Response);
}

FString FAscentToolsetUtils::ForwardBridgeResult(
    const TSharedPtr<FJsonObject>& Result,
    const TCHAR* DomainTag,
    const FString& ToolName)
{
    if (!Result.IsValid())
    {
        return MakeErrorResponse(FString::Printf(
            TEXT("[%s] handler returned null for '%s'"), DomainTag, *ToolName));
    }

    if (Result->HasField(TEXT("success")) && !Result->GetBoolField(TEXT("success")))
    {
        return JsonToString(Result);
    }
    return MakeSuccessResponse(Result);
}

TSharedPtr<FJsonObject> FAscentToolsetUtils::ParseObject(const FString& Json)
{
    if (Json.TrimStartAndEnd().IsEmpty())
    {
        return nullptr;
    }
    TSharedPtr<FJsonObject> Obj;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
    if (!FJsonSerializer::Deserialize(Reader, Obj) || !Obj.IsValid())
    {
        return nullptr;
    }
    return Obj;
}

bool FAscentToolsetUtils::ParseArray(const FString& Json, TArray<TSharedPtr<FJsonValue>>& OutArray)
{
    OutArray.Reset();
    if (Json.TrimStartAndEnd().IsEmpty())
    {
        return true;
    }
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
    return FJsonSerializer::Deserialize(Reader, OutArray);
}

FString FAscentToolsetUtils::Dispatch(const TCHAR* WireName, const TSharedPtr<FJsonObject>& Params)
{
    UAscentAIAgentSubsystem* Subsystem =
        GEditor ? GEditor->GetEditorSubsystem<UAscentAIAgentSubsystem>() : nullptr;
    if (!Subsystem)
    {
        return MakeErrorResponse(TEXT("AscentAIAgent subsystem unavailable"));
    }
    const TSharedPtr<FJsonObject> SafeParams = Params.IsValid() ? Params : MakeShared<FJsonObject>();
    return Subsystem->ExecuteCommand(WireName, SafeParams);
}

TArray<TSubclassOf<UToolsetDefinition>> FAscentToolsetUtils::GetToolsetClasses()
{
    return {
        UAscentAnimGraphToolset::StaticClass(),
        UAscentDataToolset::StaticClass(),
        UAscentGenerationToolset::StaticClass(),
        UAscentGraphToolset::StaticClass(),
        UAscentSystemToolset::StaticClass()
    };
}
