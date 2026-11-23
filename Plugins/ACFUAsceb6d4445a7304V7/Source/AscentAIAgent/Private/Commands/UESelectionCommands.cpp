// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "Commands/UESelectionCommands.h"
#include "AscentAIAgentSubsystem.h"

bool FUESelectionCommands::IsCommand(const FString& CommandType) const
{
    return CommandType == TEXT("ue_sel_nodes");
}

FString FUESelectionCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    if (CommandType == TEXT("ue_sel_nodes"))
    {
        return HandleNodes(Params);
    }
    return MakeErrorResponse(FString::Printf(TEXT("[ue_sel] unhandled command: %s"), *CommandType));
}

FString FUESelectionCommands::HandleNodes(const TSharedPtr<FJsonObject>& /*Params*/)
{
    if (!Owner)
    {
        return MakeErrorResponse(TEXT("[ue_sel] subsystem owner not bound"));
    }
    return Owner->GetSelectedBlueprintNodes();
}
