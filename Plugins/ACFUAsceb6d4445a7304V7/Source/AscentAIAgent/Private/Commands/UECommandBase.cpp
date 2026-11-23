// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "Commands/UECommandBase.h"

#include "Toolsets/AscentToolsetUtils.h"

FString FUECommandBase::MakeSuccessResponse(const TSharedPtr<FJsonObject>& Data)
{
    return FAscentToolsetUtils::MakeSuccessResponse(Data);
}

FString FUECommandBase::MakeErrorResponse(const FString& ErrorMessage)
{
    return FAscentToolsetUtils::MakeErrorResponse(ErrorMessage);
}

FString FUECommandBase::JsonToString(const TSharedPtr<FJsonObject>& Obj)
{
    return FAscentToolsetUtils::JsonToString(Obj);
}

FString FUECommandBase::JsonToString(const TArray<TSharedPtr<FJsonValue>>& Arr)
{
    return FAscentToolsetUtils::JsonToString(Arr);
}

FString FUECommandBase::StripUEPrefix(const FString& CommandType)
{
    if (CommandType.StartsWith(TEXT("ue_")))
    {
        return CommandType.RightChop(3);
    }
    return CommandType;
}

FString FUECommandBase::ForwardBridgeResult(
    const TSharedPtr<FJsonObject>& Result,
    const TCHAR* DomainTag,
    const FString& CommandType)
{
    return FAscentToolsetUtils::ForwardBridgeResult(Result, DomainTag, CommandType);
}
