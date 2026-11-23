// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

// Stub header so verbatim-mirrored AgenticUEBridge sources compile inside the AscentAIAgent module.
// Source mirror layout: Public/AgenticMCP/Commands/, Private/AgenticMCP/Commands/.
// To refresh from upstream: copy AgenticUEBridge/Source/AgenticMCP/{Public,Private}/Commands/* over the matching folders here.
// Do NOT add ACF-specific changes to mirrored files; integrate via the adapter (FUEBlueprintCommands).

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Engine/Engine.h"
#include "GameplayTagContainer.h"
#include "Json.h"
#include "JsonUtilities.h"
#include "UObject/UObjectGlobals.h"

// AUB's module-API macro is force-defined via Build.cs (PublicDefinitions:AGENTICMCP_API=).
// Mirrored AUB code logs via LogAgenticMCP. Defined in AscentAIAgentModule.cpp.
DECLARE_LOG_CATEGORY_EXTERN(LogAgenticMCP, Log, All);
