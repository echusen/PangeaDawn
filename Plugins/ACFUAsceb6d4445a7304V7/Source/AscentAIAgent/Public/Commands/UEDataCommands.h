// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "Commands/UECommandBase.h"

class FAgenticMCPDataCommands;

/**
 * Generic data authoring adapter onto the mirrored AgenticUEBridge data family.
 *
 * Wire formats:
 *   ue_da_*   DataAsset reflection
 *   ue_gt_*         GameplayTag queries
 *   ue_asset_list   assets and Blueprints deriving from a native class
 */
class FUEDataCommands : public FUECommandBase
{
public:
    explicit FUEDataCommands(UAscentAIAgentSubsystem* InOwner);
    virtual ~FUEDataCommands();

    virtual bool IsCommand(const FString& CommandType) const override;
    virtual FString HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params) override;

private:
    FString HandleListAssets(const TSharedPtr<FJsonObject>& Params) const;

    TUniquePtr<FAgenticMCPDataCommands> BridgeCommands;
};
