// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "ACFFirstPersonEditorModule.h"

#include "ACFProceduralEditorToolRegistry.h"
#include "ACFFirstPersonEditorStyle.h"
#include "ACFFirstPersonEditorToolbar.h"

#include "Inspector/SACFProceduralMotionInspector.h"
#include "Presets/SACFProceduralPresetWorkbench.h"

#include "Framework/Application/SlateApplication.h"
#include "Widgets/Docking/SDockTab.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"

#define LOCTEXT_NAMESPACE "ACFFirstPersonEditor"

namespace ACFFirstPersonEditorModule
{
	static TSharedRef<SDockTab> SpawnPresetWorkbenchTab(const FSpawnTabArgs& Args)
	{
		return SNew(SDockTab)
			.TabRole(ETabRole::NomadTab)
			[
				SNew(SACFProceduralPresetWorkbench)
			];
	}

#if ACF_FP_DEBUG_CAPTURE
	static TSharedRef<SDockTab> SpawnInspectorTab(const FSpawnTabArgs& Args)
	{
		return SNew(SDockTab)
			.TabRole(ETabRole::NomadTab)
			[
				SNew(SACFProceduralMotionInspector)
			];
	}
#endif
}

void FACFFirstPersonEditorModule::StartupModule()
{
	FACFFirstPersonEditorStyle::Register();

	RegisterWorkspaceGroup();
	RegisterTools();

	Toolbar = MakeShared<FACFFirstPersonEditorToolbar>();
	Toolbar->Initialize();
}

void FACFFirstPersonEditorModule::ShutdownModule()
{
	if (Toolbar.IsValid())
	{
		Toolbar->Shutdown();
		Toolbar.Reset();
	}

	FACFProceduralEditorToolRegistry::Get().UnregisterAll();

	FACFFirstPersonEditorStyle::Unregister();
}

void FACFFirstPersonEditorModule::RegisterWorkspaceGroup()
{
	if (!FSlateApplication::IsInitialized())
	{
		return;
	}

	const TSharedRef<FWorkspaceItem> Group = WorkspaceMenu::GetMenuStructure().GetToolsCategory()->AddGroup(
		LOCTEXT("WorkspaceGroup", "Ascent First Person"),
		LOCTEXT("WorkspaceGroupTooltip", "Editor tools for Ascent First Person."),
		FSlateIcon(FACFFirstPersonEditorStyle::GetStyleSetName(), TEXT("ACFFirstPerson.TabIcon")));

	FACFProceduralEditorToolRegistry::Get().SetWorkspaceGroup(Group);
}

void FACFFirstPersonEditorModule::RegisterTools()
{
	if (!FSlateApplication::IsInitialized())
	{
		return;
	}

	const FSlateIcon PluginIcon(FACFFirstPersonEditorStyle::GetStyleSetName(), TEXT("ACFFirstPerson.TabIcon"));

	FACFProceduralEditorToolDescriptor Presets;
	Presets.Id = TEXT("ACF.FirstPerson.Presets");
	Presets.Label = LOCTEXT("PresetsLabel", "Presets");
	Presets.Tooltip = LOCTEXT("PresetsTooltip", "Which entities use which preset data, and the presets inside them, in one place.");
	Presets.Icon = PluginIcon;
	Presets.Section = TEXT("Setup");
	Presets.SectionLabel = LOCTEXT("SetupSection", "Setup");
	Presets.Order = 2;
	Presets.SpawnTab = FOnSpawnTab::CreateStatic(&ACFFirstPersonEditorModule::SpawnPresetWorkbenchTab);

	FACFProceduralEditorToolRegistry::Get().RegisterTool(Presets);

#if ACF_FP_DEBUG_CAPTURE
	FACFProceduralEditorToolDescriptor Inspector;
	Inspector.Id = TEXT("ACF.FirstPerson.Inspector");
	Inspector.Label = LOCTEXT("InspectorLabel", "Motion Inspector");
	Inspector.Tooltip = LOCTEXT("InspectorTooltip", "Live view of the motion stack: which component emits what, at which stage, with what weight.");
	Inspector.Icon = PluginIcon;
	Inspector.Section = TEXT("Debug");
	Inspector.SectionLabel = LOCTEXT("DebugSection", "Debug");
	Inspector.Order = 10;
	Inspector.SpawnTab = FOnSpawnTab::CreateStatic(&ACFFirstPersonEditorModule::SpawnInspectorTab);

	FACFProceduralEditorToolRegistry::Get().RegisterTool(Inspector);
#endif
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FACFFirstPersonEditorModule, AscentFirstPersonEditor)
