// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "AscentAIAgentSettings.generated.h"

/**
 * Config backing Yarin's private Editor tool server.
 * Users edit it from Project Settings > Engine > Ascent Yarin, which mirrors MCP Server Port here.
 */
UCLASS(config = Plugins, Defaultconfig)
class ASCENTAIAGENT_API UACFAIAgentDeveloperSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	static const UACFAIAgentDeveloperSettings* Get();
	static UACFAIAgentDeveloperSettings* GetMutable();

	virtual bool SupportsAutoRegistration() const override { return false; }

	// First loopback TCP port of Yarin's own Editor tool service (its private MCP server). If the port is busy, the next 9 ports are probed automatically. Applied on the next Connect.
	// Yarin never uses Unreal's shared MCP server port (default 8000).
	UPROPERTY(config)
	int32 McpServerPort = 8890;
};
