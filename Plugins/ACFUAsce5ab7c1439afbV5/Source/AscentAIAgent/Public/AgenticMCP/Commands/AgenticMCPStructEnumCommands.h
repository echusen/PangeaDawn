// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Json.h"

/**
 * UserDefinedStruct (UDS) + UserDefinedEnum (UDE) authoring surface — Phase 5
 * of the bp-expert plan. Wire-format commands: `uds_*` / `ude_*`.
 *
 * Backs the `agentic_uds_*` / `agentic_ude_*` MCP tool families.
 *
 * Implementation leans on the canonical engine helpers
 *   - `FStructureEditorUtils` (UnrealEd) for UDS
 *   - `FEnumEditorUtils`     (UnrealEd) for UDE
 * which means GUID stability across rename and CompileStructure-driven
 * variable layout updates come for free.
 */
class AGENTICMCP_API FAgenticMCPStructEnumCommands
{
public:
	FAgenticMCPStructEnumCommands() = default;

	TSharedPtr<FJsonObject> HandleCommand(const FString& InCommandType, const TSharedPtr<FJsonObject>& InParams);

private:
	// ─── UserDefinedStruct ──────────────────────────────────────────────

	/** Create a UUserDefinedStruct at a content path. Optional `members[]` seeds initial variables. */
	TSharedPtr<FJsonObject> HandleUdsCreate(const TSharedPtr<FJsonObject>& InParams);

	/** Read-only dump of a UUserDefinedStruct: members with guid/type/default/metadata + struct-level tooltip. */
	TSharedPtr<FJsonObject> HandleUdsInspect(const TSharedPtr<FJsonObject>& InParams);

	/** Bulk add typed members. Each entry: name, type{...}, default_value?, tooltip?, save_game?, multi_line?, editable_on_instance?, metadata?. */
	TSharedPtr<FJsonObject> HandleUdsAddMembers(const TSharedPtr<FJsonObject>& InParams);

	/** Bulk remove members by name OR guid. */
	TSharedPtr<FJsonObject> HandleUdsRemoveMembers(const TSharedPtr<FJsonObject>& InParams);

	/** Rename a single member via FStructureEditorUtils::RenameVariable(guid, newName). GUID-stable. */
	TSharedPtr<FJsonObject> HandleUdsRenameMember(const TSharedPtr<FJsonObject>& InParams);

	/** Set a member's literal default value via ChangeVariableDefaultValue. */
	TSharedPtr<FJsonObject> HandleUdsSetMemberDefault(const TSharedPtr<FJsonObject>& InParams);

	/** Reorder members to match a target sequence using MoveVariable(PositionBelow). */
	TSharedPtr<FJsonObject> HandleUdsReorderMembers(const TSharedPtr<FJsonObject>& InParams);

	// ─── UserDefinedEnum ────────────────────────────────────────────────

	/** Create a UUserDefinedEnum at a content path. Optional `entries[]` seeds initial enumerators. */
	TSharedPtr<FJsonObject> HandleUdeCreate(const TSharedPtr<FJsonObject>& InParams);

	/** Read-only dump of a UUserDefinedEnum: entries with index/raw_name/display_name/tooltip, bitflags state. */
	TSharedPtr<FJsonObject> HandleUdeInspect(const TSharedPtr<FJsonObject>& InParams);

	/** Bulk add enumerators with optional display names + tooltips. */
	TSharedPtr<FJsonObject> HandleUdeAddEntries(const TSharedPtr<FJsonObject>& InParams);

	/** Bulk remove enumerators by index. Refuses to touch the auto-managed `_MAX` sentinel. */
	TSharedPtr<FJsonObject> HandleUdeRemoveEntries(const TSharedPtr<FJsonObject>& InParams);

	/** Set the display name on a single enumerator via SetEnumeratorDisplayName. */
	TSharedPtr<FJsonObject> HandleUdeRenameEntry(const TSharedPtr<FJsonObject>& InParams);
};
