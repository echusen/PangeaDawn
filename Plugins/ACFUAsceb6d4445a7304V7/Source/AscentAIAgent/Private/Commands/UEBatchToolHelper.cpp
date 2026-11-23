// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "Commands/UEBatchToolHelper.h"

#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

bool FUEBatchToolHelper::ExtractArray(
    const TSharedPtr<FJsonObject>& Params,
    const FString& FieldName,
    TArray<TSharedPtr<FJsonObject>>& OutItems,
    FString& OutError)
{
    OutItems.Reset();

    if (!Params.IsValid())
    {
        OutError = TEXT("missing params object");
        return false;
    }

    const TSharedPtr<FJsonValue> Field = Params->TryGetField(FieldName);
    if (!Field.IsValid() || Field->IsNull())
    {
        // Missing field is treated as "no items" — caller-friendly default.
        return true;
    }

    // Shape 1: native JSON array.
    if (Field->Type == EJson::Array)
    {
        const TArray<TSharedPtr<FJsonValue>>& Raw = Field->AsArray();
        OutItems.Reserve(Raw.Num());
        for (int32 Index = 0; Index < Raw.Num(); ++Index)
        {
            const TSharedPtr<FJsonValue>& Item = Raw[Index];
            if (!Item.IsValid() || Item->Type != EJson::Object)
            {
                OutError = FString::Printf(
                    TEXT("'%s'[%d] is not a JSON object"),
                    *FieldName, Index);
                OutItems.Reset();
                return false;
            }
            OutItems.Add(Item->AsObject());
        }
        return true;
    }

    // Shape 2: JSON-stringified array.
    if (Field->Type == EJson::String)
    {
        const FString Raw = Field->AsString();
        if (Raw.IsEmpty())
        {
            return true;
        }

        TArray<TSharedPtr<FJsonValue>> Parsed;
        const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Raw);
        if (!FJsonSerializer::Deserialize(Reader, Parsed))
        {
            OutError = FString::Printf(
                TEXT("'%s' looks like a stringified array but failed to parse"),
                *FieldName);
            return false;
        }

        OutItems.Reserve(Parsed.Num());
        for (int32 Index = 0; Index < Parsed.Num(); ++Index)
        {
            const TSharedPtr<FJsonValue>& Item = Parsed[Index];
            if (!Item.IsValid() || Item->Type != EJson::Object)
            {
                OutError = FString::Printf(
                    TEXT("'%s'[%d] in stringified array is not a JSON object"),
                    *FieldName, Index);
                OutItems.Reset();
                return false;
            }
            OutItems.Add(Item->AsObject());
        }
        return true;
    }

    OutError = FString::Printf(
        TEXT("'%s' must be a JSON array or a JSON-stringified array"),
        *FieldName);
    return false;
}

