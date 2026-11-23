// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "Commands/UECommandBase.h"

/** Selected Blueprint nodes. Wire name: ue_sel_nodes. */
class FUESelectionCommands : public FUECommandBase
{
public:
    using FUECommandBase::FUECommandBase;

    virtual bool IsCommand(const FString& CommandType) const override;
    virtual FString HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params) override;

private:
    FString HandleNodes(const TSharedPtr<FJsonObject>& Params);
};
