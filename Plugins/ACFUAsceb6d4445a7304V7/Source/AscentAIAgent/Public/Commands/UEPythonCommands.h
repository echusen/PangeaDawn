// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "Commands/UECommandBase.h"

/**
 * Constrained Unreal Python fallback commands.
 *
 * Wire formats:
 *   ue_py_run
 *
 * This is intentionally a fallback surface, not the primary authoring API.
 * Prefer typed UE bridge tools first; use Python only for focused probes or
 * recovery when a typed bridge command is missing or failing.
 */
class FUEPythonCommands : public FUECommandBase
{
public:
    using FUECommandBase::FUECommandBase;

    virtual bool IsCommand(const FString& CommandType) const override;
    virtual FString HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params) override;

private:
    FString HandleRun(const TSharedPtr<FJsonObject>& Params);
    FString ValidateRequest(const FString& Code, const FString& Mode, const TArray<FString>& ExpectedPaths) const;
};
