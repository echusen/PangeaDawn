// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "Commands/UEDataTableCommands.h"
#include "AscentAIAgentSubsystem.h"
#include "Dom/JsonObject.h"

bool FUEDataTableCommands::IsCommand(const FString& CommandType) const
{
    return CommandType == TEXT("ue_dt_add_tag");
}

FString FUEDataTableCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    if (CommandType == TEXT("ue_dt_add_tag"))
    {
        return HandleAddTag(Params);
    }
    return MakeErrorResponse(FString::Printf(TEXT("[ue_dt] unhandled command: %s"), *CommandType));
}

FString FUEDataTableCommands::HandleAddTag(const TSharedPtr<FJsonObject>& Params)
{
    if (!Owner)
    {
        return MakeErrorResponse(TEXT("[ue_dt] subsystem owner not bound"));
    }
    return Owner->AddTagToDataTable(Params);
}
