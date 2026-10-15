// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Templates/SubclassOf.h"

class UToolsetDefinition;

/**
 * Shared helpers for the Ascent Epic-MCP toolsets (UToolsetDefinition subclasses).
 *
 * Preserves the wire payload contract of the retired bespoke MCP server:
 *   success → {"status":"success","success":true, ...data}
 *   error   → {"status":"error","success":false,"error":msg}
 * so existing consumers (Yarin, skills, runbooks) keep parsing the same shapes.
 */
struct FAscentToolsetUtils
{
    /** Compact-serialize a JSON object. */
    [[nodiscard]] static FString JsonToString(const TSharedPtr<FJsonObject>& Obj);

    /** Compact-serialize a JSON array. */
    [[nodiscard]] static FString JsonToString(const TArray<TSharedPtr<FJsonValue>>& Arr);

    /** Build {"status":"success","success":true,...Data}. */
    [[nodiscard]] static FString MakeSuccessResponse(const TSharedPtr<FJsonObject>& Data);

    /** Build {"status":"error","success":false,"error":Msg}. */
    [[nodiscard]] static FString MakeErrorResponse(const FString& ErrorMessage);

    /**
     * Forward a bridge handler's JSON result through the standard envelope:
     *  - null result → MakeErrorResponse with DomainTag + ToolName
     *  - success=false on the result → pass through as-is (preserves error fields)
     *  - otherwise → MakeSuccessResponse
     */
    [[nodiscard]] static FString ForwardBridgeResult(
        const TSharedPtr<FJsonObject>& Result,
        const TCHAR* DomainTag,
        const FString& ToolName);

    /** Parse a JSON object string. Returns nullptr on parse failure or empty input. */
    [[nodiscard]] static TSharedPtr<FJsonObject> ParseObject(const FString& Json);

    /** Parse a JSON array string. Returns false on parse failure; empty input yields an empty array. */
    [[nodiscard]] static bool ParseArray(const FString& Json, TArray<TSharedPtr<FJsonValue>>& OutArray);

    /**
     * Route a tool call through UAscentAIAgentSubsystem::ExecuteCommand — the same
     * dispatch the bespoke MCP server used. Preserves per-domain param normalization
     * and the response envelope. WireName is the legacy wire-format command name
     * (e.g. "ue_bp_add_nodes", "acf_action_list").
     */
    [[nodiscard]] static FString Dispatch(const TCHAR* WireName, const TSharedPtr<FJsonObject>& Params);

    /** Every Epic-MCP toolset class this module serves. Keep in sync with Public/Toolsets/. */
    [[nodiscard]] static TArray<TSubclassOf<UToolsetDefinition>> GetToolsetClasses();
};
