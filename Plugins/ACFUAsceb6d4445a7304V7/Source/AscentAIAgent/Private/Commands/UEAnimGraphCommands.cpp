// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "Commands/UEAnimGraphCommands.h"

#include "Commands/AgenticMCPGraphAnalysis.h"
#include "Dom/JsonObject.h"

namespace
{
    const TMap<FString, FString>& GetNativeCommandNames()
    {
        static const TMap<FString, FString> Names = {
            { TEXT("ue_anim_analyze"), TEXT("analyze_animgraph") },
            { TEXT("ue_anim_add_node"), TEXT("add_animgraph_node") },
            { TEXT("ue_anim_connect_nodes"), TEXT("connect_animgraph_nodes") },
            { TEXT("ue_anim_set_node_properties"), TEXT("set_animgraph_node_properties") },
            { TEXT("ue_anim_remove_node"), TEXT("remove_animgraph_node") },
            { TEXT("ue_anim_pin"), TEXT("animgraph_pin") },
            { TEXT("ue_anim_blend_profile"), TEXT("blend_profile") },
            { TEXT("ue_anim_state_machine"), TEXT("state_machine") },
        };
        return Names;
    }
}

FUEAnimGraphCommands::FUEAnimGraphCommands(UAscentAIAgentSubsystem* InOwner)
    : FUECommandBase(InOwner)
    , GraphAnalysisCommands(MakeUnique<FAgenticMCPGraphAnalysis>())
{
}

FUEAnimGraphCommands::~FUEAnimGraphCommands() = default;

bool FUEAnimGraphCommands::IsCommand(const FString& CommandType) const
{
    return GetNativeCommandNames().Contains(CommandType);
}

FString FUEAnimGraphCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    const FString* NativeCommand = GetNativeCommandNames().Find(CommandType);
    if (!NativeCommand)
    {
        return MakeErrorResponse(FString::Printf(TEXT("[ue_anim] unknown command '%s'"), *CommandType));
    }

    const TSharedPtr<FJsonObject> SafeParams = Params.IsValid() ? Params : MakeShared<FJsonObject>();
    const TSharedPtr<FJsonObject> Result = GraphAnalysisCommands->HandleCommand(*NativeCommand, SafeParams);
    return ForwardBridgeResult(Result, TEXT("ue_anim"), CommandType);
}
