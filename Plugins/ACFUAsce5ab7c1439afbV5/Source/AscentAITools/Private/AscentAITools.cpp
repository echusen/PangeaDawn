// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.

#include "AscentAITools.h"
#include "SACFAIToolsWindow.h"

#include "Framework/Docking/TabManager.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Docking/SDockTab.h"
#include "ToolMenus.h"
#include "Styling/AppStyle.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"

#define LOCTEXT_NAMESPACE "FACFAIToolsModule"

const FName FACFAIToolsModule::TabId(TEXT("AscentAIToolsTab"));

void FACFAIToolsModule::StartupModule()
{
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
		TabId,
		FOnSpawnTab::CreateRaw(this, &FACFAIToolsModule::OnSpawnTab))
		.SetDisplayName(LOCTEXT("TabTitle", "AI Tools (Image / 3D)"))
		.SetTooltipText(LOCTEXT("TabTooltip", "Generate images and 3D meshes with AI and import them into the project."))
		.SetGroup(WorkspaceMenu::GetMenuStructure().GetToolsCategory())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Settings"));

	UToolMenus::RegisterStartupCallback(
		FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FACFAIToolsModule::RegisterMenus));
}

void FACFAIToolsModule::ShutdownModule()
{
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);

	if (FSlateApplication::IsInitialized())
	{
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(TabId);
	}
}

void FACFAIToolsModule::RegisterMenus()
{
	FToolMenuOwnerScoped OwnerScoped(this);

	// Add an entry under the main "Tools" menu.
	if (UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Tools"))
	{
		FToolMenuSection& Section = Menu->FindOrAddSection("Programming");
		Section.AddMenuEntry(
			"OpenAscentAITools",
			LOCTEXT("MenuEntry", "AI Tools (Image / 3D)"),
			LOCTEXT("MenuEntryTip", "Generate images and 3D meshes with AI and import them into the project."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Settings"),
			FUIAction(FExecuteAction::CreateRaw(this, &FACFAIToolsModule::OpenTab)));
	}
}

void FACFAIToolsModule::OpenTab()
{
	FGlobalTabmanager::Get()->TryInvokeTab(TabId);
}

TSharedRef<SDockTab> FACFAIToolsModule::OnSpawnTab(const FSpawnTabArgs& /*SpawnTabArgs*/)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(SACFAIToolsWindow)
		];
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FACFAIToolsModule, AscentAITools)
