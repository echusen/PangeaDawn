// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

/** Small JSON/instruction helpers. Unreal and Ascent tools are exposed directly by Epic's MCP server, not adapted here. */
namespace ACFAssistantTools
{
	TSharedPtr<FJsonObject> Error(const FString& Message);
	TSharedPtr<FJsonObject> Success(const FString& Message);
	FString Encode(const TSharedPtr<FJsonObject>& Object);
	FString Instructions();
}
