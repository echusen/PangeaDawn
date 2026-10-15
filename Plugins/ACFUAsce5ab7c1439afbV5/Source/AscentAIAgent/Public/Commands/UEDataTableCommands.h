// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "Commands/UECommandBase.h"

/**
 * DataTable mutation commands.
 *
 * Wire formats:
 *   ue_dt_add_tag
 */
class FUEDataTableCommands : public FUECommandBase
{
public:
    using FUECommandBase::FUECommandBase;

    virtual bool IsCommand(const FString& CommandType) const override;
    virtual FString HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params) override;

private:
    FString HandleAddTag(const TSharedPtr<FJsonObject>& Params);
};
