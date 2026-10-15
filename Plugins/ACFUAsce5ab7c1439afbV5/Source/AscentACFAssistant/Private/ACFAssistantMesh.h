// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Chat cards for 3D mesh generation. A mesh tool call has no image file to show, so the thread gets its own message roles:
 * a live progress card while the cloud service works, then a 3D viewer of the imported mesh, or the reason it failed.
 * Everything here is plain data and parsing so it can be tested without Slate.
 */
namespace ACFAssistantMesh
{
	/** The generation is running. Text is empty. */
	inline constexpr TCHAR RolePending[] = TEXT("MeshPending");
	/** The mesh was imported. Text is its object path, for example /Game/Yarin/Chest/SM_Chest.SM_Chest. */
	inline constexpr TCHAR RoleReady[] = TEXT("Mesh");
	/** The generation failed. Text is the reason. */
	inline constexpr TCHAR RoleFailed[] = TEXT("MeshFailed");

	inline bool IsMeshRole(const FString& Role)
	{
		return Role == RolePending || Role == RoleReady || Role == RoleFailed;
	}

	/** True for the Editor tools that generate a 3D mesh. Matches the bare tool name, case insensitive. */
	bool IsMeshGenerationTool(const FString& ToolName);

	struct FToolResult
	{
		bool bOk = false;
		/** Object path of the imported mesh when bOk. */
		FString AssetPath;
		/** What went wrong when not bOk. */
		FString Error;
	};

	/**
	 * Reads the answer of a mesh tool call. Body is the HTTP body of the MCP tools/call response: a JSON-RPC object, or a
	 * server sent event stream whose last data line is one. The tool's own JSON arrives as a text block inside it.
	 */
	FToolResult ParseToolResponse(const FString& Body);

	/** The phone cannot draw a 3D view, so it receives a one line status instead. */
	FString DescribeForPhone(const FString& Role, const FString& Text);
}
