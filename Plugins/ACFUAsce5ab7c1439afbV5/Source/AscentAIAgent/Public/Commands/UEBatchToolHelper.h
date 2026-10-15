// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

/**
 * Helpers for bulk-by-default command handlers.
 *
 * The chatbot side may pass an array parameter in two shapes:
 *   1. Native JSON array — `{"nodes": [{...}, {...}]}`
 *   2. JSON-stringified array — `{"nodes": "[{...}, {...}]"}`
 *
 * Some LLM tool-call serializers stringify nested arrays before
 * sending; reading the field as `JsonValueArray` would fail in that
 * case. `ExtractArray` accepts both and normalizes them into a
 * `TArray<TSharedPtr<FJsonObject>>` so handlers don't have to care.
 */
class FUEBatchToolHelper
{
public:
    /**
     * Extract an array of JSON objects from a field on Params. Handles
     * both native arrays and JSON-stringified arrays.
     *
     * @param Params      The full command params object.
     * @param FieldName   The field to read (e.g. TEXT("nodes")).
     * @param OutItems    Filled on success with the parsed objects.
     * @param OutError    Filled on failure with a human-readable reason.
     * @return true if the field exists and parses cleanly to an array
     *         of objects; false otherwise. An empty array is success.
     */
    static bool ExtractArray(
        const TSharedPtr<FJsonObject>& Params,
        const FString& FieldName,
        TArray<TSharedPtr<FJsonObject>>& OutItems,
        FString& OutError);

};
