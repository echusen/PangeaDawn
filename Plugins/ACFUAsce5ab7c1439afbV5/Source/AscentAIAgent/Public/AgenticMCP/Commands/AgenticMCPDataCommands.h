// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Json.h"

/**
 * DataAsset reflection and Gameplay Tag queries.
 * Wire-format commands: `da_*` / `gt_*`.
 *
 * DataTable, StringTable and generic DataAsset create/set tools are served by
 * Epic's EditorToolset, so they are not mirrored here.
 */
class AGENTICMCP_API FAgenticMCPDataCommands
{
public:
	FAgenticMCPDataCommands() = default;

	TSharedPtr<FJsonObject> HandleCommand(const FString& InCommandType, const TSharedPtr<FJsonObject>& InParams);

private:
	// DataAsset (da_*)

	/** Read-only FProperty dump of any UObject. */
	TSharedPtr<FJsonObject> HandleDaInspect(const TSharedPtr<FJsonObject>& InParams);

	/** Bulk-set a field on every element (or specific indices) of a struct-array property. */
	TSharedPtr<FJsonObject> HandleDaSetArrayProperty(const TSharedPtr<FJsonObject>& InParams);

	// DataTable (dt_*)

	// Gameplay Tags (gt_*)

	/** List configured/registered Gameplay Tags, optionally filtered by root or property hint. */
	TSharedPtr<FJsonObject> HandleGtList(const TSharedPtr<FJsonObject>& InParams);

	/** Search configured/registered Gameplay Tags by query, root, and property hint. */
	TSharedPtr<FJsonObject> HandleGtSearch(const TSharedPtr<FJsonObject>& InParams);

	/** Validate one exact Gameplay Tag and return nearby suggestions when missing. */
	TSharedPtr<FJsonObject> HandleGtValidate(const TSharedPtr<FJsonObject>& InParams);

	// StringTable (stbl_*)

};
