// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class UAscentAIAgentSubsystem;

/**
 * Base for the per-domain command handlers behind the Ascent toolsets.
 *
 * Each domain (Blueprint, Data, Action, Dialogue, ...) inherits this and:
 *  - declares which wire-format command names it owns via IsCommand()
 *  - dispatches them via HandleCommand()
 *
 * Subclasses are pure C++ (no UCLASS); the owning UAscentAIAgentSubsystem
 * holds them as TUniquePtr members and routes FAscentToolsetUtils::Dispatch calls to them.
 */
class FUECommandBase
{
public:
    explicit FUECommandBase(UAscentAIAgentSubsystem* InOwner)
        : Owner(InOwner)
    {
    }

    virtual ~FUECommandBase() = default;

    FUECommandBase(const FUECommandBase&) = delete;
    FUECommandBase& operator=(const FUECommandBase&) = delete;

    /** Returns true if this domain handles the given wire-format command. */
    [[nodiscard]] virtual bool IsCommand(const FString& CommandType) const = 0;

    /** Dispatch within the domain. Caller must have checked IsCommand() first. */
    [[nodiscard]] virtual FString HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params) = 0;

protected:
    /** Build {"status":"success","success":true,...Data}. */
    [[nodiscard]] static FString MakeSuccessResponse(const TSharedPtr<FJsonObject>& Data);

    /** Build {"status":"error","success":false,"error":Msg}. */
    [[nodiscard]] static FString MakeErrorResponse(const FString& ErrorMessage);

    /** Compact-serialize a JSON object. */
    [[nodiscard]] static FString JsonToString(const TSharedPtr<FJsonObject>& Obj);

    /** Compact-serialize a JSON array. */
    [[nodiscard]] static FString JsonToString(const TArray<TSharedPtr<FJsonValue>>& Arr);

    /** Strip an optional `ue_` prefix to reach the upstream AUB-native wire name. */
    [[nodiscard]] static FString StripUEPrefix(const FString& CommandType);

    /**
     * Forward a bridge handler's JSON result through the standard envelope:
     *  - null result → MakeErrorResponse with DomainTag + CommandType
     *  - success=false on the result → pass through as-is (preserves AUB error fields)
     *  - otherwise → MakeSuccessResponse
     */
    [[nodiscard]] static FString ForwardBridgeResult(
        const TSharedPtr<FJsonObject>& Result,
        const TCHAR* DomainTag,
        const FString& CommandType);

    /** Non-owning pointer back to the bridge subsystem. */
    UAscentAIAgentSubsystem* Owner = nullptr;
};
