// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "Commands/UECommandBase.h"

class FAgenticMCPBlueprintCommands;
class FAgenticMCPBpAuthorCommands;
class FAgenticMCPStructEnumCommands;

/**
 * Blueprint authoring command surface. Adapter onto the verbatim AgenticUEBridge mirrors.
 *
 * AUB sources live under Source/AscentAIAgent/{Public,Private}/AgenticMCP/Commands/. To refresh from
 * upstream: replace those files in place. This adapter is the only touch point into this plugin.
 *
 * Owns three AUB command class instances:
 *   - FAgenticMCPBlueprintCommands   — introspection, per-node graph editing,
 *                                      variables/functions, components, analysis
 *   - FAgenticMCPBpAuthorCommands    — v2 atomic authoring: bp_actions_search + bp_author_graph
 *   - FAgenticMCPStructEnumCommands  — UserDefinedStruct (uds_*) + UserDefinedEnum (ude_*)
 *
 * Wire formats accepted:
 *   ue_bp_<verb>    - Blueprint operations
 *   ue_uds_<verb>   - UserDefinedStruct operations
 *   ue_ude_<verb>   - UserDefinedEnum operations
 */
class FUEBlueprintCommands : public FUECommandBase
{
public:
    explicit FUEBlueprintCommands(UAscentAIAgentSubsystem* InOwner);
    virtual ~FUEBlueprintCommands();

    virtual bool IsCommand(const FString& CommandType) const override;
    virtual FString HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params) override;

private:
    TUniquePtr<FAgenticMCPBlueprintCommands> BridgeCommands;
    TUniquePtr<FAgenticMCPBpAuthorCommands> BpAuthorCommands;
    TUniquePtr<FAgenticMCPStructEnumCommands> StructEnumCommands;

    static bool IsBpAuthorCommand(const FString& NativeCommand);
    static bool IsStructEnumCommand(const FString& NativeCommand);
};
