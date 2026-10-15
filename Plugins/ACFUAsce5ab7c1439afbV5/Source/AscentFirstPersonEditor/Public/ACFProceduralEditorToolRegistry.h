// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Docking/TabManager.h"
#include "Textures/SlateIcon.h"

class FWorkspaceItem;

/**
 * One editor tool: a nomad tab plus the metadata the toolbar dropdown needs to list it.
 *
 * Tab ids are global and stable, so a host project can invoke any of these from its own centralized
 * toolbar without depending on this plugin.
 */
struct FACFProceduralEditorToolDescriptor
{
	/** Nomad tab id. Also the menu entry name. */
	FName Id;

	FText Label;

	FText Tooltip;

	FSlateIcon Icon;

	/** Dropdown section this tool is grouped under. */
	FName Section = TEXT("Tools");

	/** Heading drawn above the section. Only the first tool of a section needs to set it. */
	FText SectionLabel;

	/** Sort order within the section. */
	int32 Order = 0;

	FOnSpawnTab SpawnTab;
};

/** A tool as it should be presented, after the settings have had their say. */
struct FACFProceduralResolvedTool
{
	FName Id;

	FText Label;

	FText Tooltip;

	FSlateIcon Icon;

	FName Section;

	FText SectionLabel;

	int32 Order = 0;
};

/**
 * The plugin's own tools, defined in C++ so a wiped editor config can never lose them.
 *
 * UACFFirstPersonEditorSettings customizes what is registered here (order, visibility) and adds
 * entries of its own. Registry defines, settings customize.
 */
class ASCENTFIRSTPERSONEDITOR_API FACFProceduralEditorToolRegistry
{
public:
	static FACFProceduralEditorToolRegistry& Get();

	/** Registers the tool and its nomad tab spawner. A duplicate id is ignored with a warning. */
	void RegisterTool(const FACFProceduralEditorToolDescriptor& Descriptor);

	void UnregisterTool(const FName& Id);

	/** Drops every tool and its tab spawner. Called on module shutdown. */
	void UnregisterAll();

	const FACFProceduralEditorToolDescriptor* FindTool(const FName& Id) const;

	const TArray<FACFProceduralEditorToolDescriptor>& GetTools() const { return Tools; }

	/** Registered tools with their overrides applied, hidden ones removed, sorted by effective order. */
	TArray<FACFProceduralResolvedTool> GetVisibleTools() const;

	/** Workspace menu group the tabs are filed under. Valid once the module has started up. */
	TSharedPtr<FWorkspaceItem> GetWorkspaceGroup() const { return WorkspaceGroup; }

	void SetWorkspaceGroup(const TSharedPtr<FWorkspaceItem>& InGroup) { WorkspaceGroup = InGroup; }

private:
	TArray<FACFProceduralEditorToolDescriptor> Tools;

	TSharedPtr<FWorkspaceItem> WorkspaceGroup;
};
