// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// Yarin's private instance of Epic's in-Editor MCP server. It runs on its own loopback port and publishes the complete module tool catalog (all Epic ToolsetRegistry tools plus Ascent toolsets),
// so it never competes with a project-owned or auto-started server on Epic's default port 8000.
namespace AscentEpicMCP
{
	/** Starts Yarin's MCP server instance on the game thread when needed. Safe to call from any thread. */
	ASCENTAIAGENT_API bool Ensure();

	/** Returns Yarin's loopback MCP URL, e.g. http://127.0.0.1:8890/mcp. Valid after Ensure() has succeeded. */
	ASCENTAIAGENT_API FString Url();

	/** Stops and destroys Yarin's server instance. Called on editor subsystem shutdown. */
	ASCENTAIAGENT_API void Shutdown();
}
