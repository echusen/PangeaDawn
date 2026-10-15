// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "AscentLocalizationEditor.h"
#include "SACFLocalizationPanel.h"
#include "ToolMenus.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"
#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/SBoxPanel.h"
#include "Styling/AppStyle.h"

DEFINE_LOG_CATEGORY(LogAscentLocalizationEditor);

#define LOCTEXT_NAMESPACE "FAscentLocalizationEditorModule"

static const FName ACFLocalizationTabName(TEXT("ACFLocalization"));

void FAscentLocalizationEditorModule::StartupModule()
{
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
		ACFLocalizationTabName,
		FOnSpawnTab::CreateRaw(this, &FAscentLocalizationEditorModule::SpawnLocalizationTab))
		.SetDisplayName(LOCTEXT("TabTitle", "Yarin AI Localization"))
		.SetTooltipText(LOCTEXT("TabTooltip", "Translate a localization target with ChatGPT or Claude via Yarin"))
		.SetGroup(WorkspaceMenu::GetMenuStructure().GetToolsCategory())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.World"));

	UToolMenus::RegisterStartupCallback(
		FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FAscentLocalizationEditorModule::RegisterMenus));
}

void FAscentLocalizationEditorModule::ShutdownModule()
{
	UnregisterMenus();
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(ACFLocalizationTabName);
}

void FAscentLocalizationEditorModule::RegisterMenus()
{
	FToolMenuOwnerScoped OwnerScoped(this);

	if (UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Tools"))
	{
		FToolMenuSection& Section = Menu->FindOrAddSection("Yarin");
		Section.Label = LOCTEXT("YarinSection", "Yarin");
		Section.AddMenuEntry(
			"ACFLocalization",
			LOCTEXT("OpenLocalization", "AI Localization"),
			LOCTEXT("OpenLocalizationTooltip", "Translate game localization targets with ChatGPT or Claude (Yarin)"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.World"),
			FUIAction(FExecuteAction::CreateRaw(this, &FAscentLocalizationEditorModule::OpenLocalizationTab)));
	}

	// Same Play toolbar section as the Yarin button (section "ACF"), immediately after it.
	if (UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar.PlayToolBar"))
	{
		FToolMenuSection& Section = ToolbarMenu->FindOrAddSection("ACF");
		FToolMenuEntry& Entry = Section.AddEntry(FToolMenuEntry::InitToolBarButton(
			"ACFLocalizationToolbar",
			FUIAction(FExecuteAction::CreateRaw(this, &FAscentLocalizationEditorModule::OpenLocalizationTab)),
			FText::GetEmpty(),
			LOCTEXT("ToolbarTranslateTooltip", "AI Localization"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.World")));
		Entry.InsertPosition = FToolMenuInsert("OpenAIAgentTab", EToolMenuInsertType::After);
		Entry.StyleNameOverride = "CalloutToolbar";
	}
}

void FAscentLocalizationEditorModule::UnregisterMenus()
{
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);
}

void FAscentLocalizationEditorModule::OpenLocalizationTab()
{
	FGlobalTabmanager::Get()->TryInvokeTab(ACFLocalizationTabName);
}

TSharedRef<SDockTab> FAscentLocalizationEditorModule::SpawnLocalizationTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				SNew(SACFLocalizationPanel)
			]
		];
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FAscentLocalizationEditorModule, AscentLocalizationEditor)
