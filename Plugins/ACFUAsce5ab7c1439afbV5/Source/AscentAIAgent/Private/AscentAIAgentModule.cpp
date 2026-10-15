// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "AscentAIAgent.h"
#include "AscentAIAgentCommands.h"
#include "AscentAIAgentStyle.h"
#include "ACFAssistantPanelFactory.h"
#include "Toolsets/AscentToolsetUtils.h"
#include "ToolsetRegistry/UToolsetRegistry.h"
#include "AgenticMCP/AgenticMCPModule.h"

#include "Framework/Docking/TabManager.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Text/STextBlock.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"

#define LOCTEXT_NAMESPACE "FAscentAIAgentModule"

DEFINE_LOG_CATEGORY(LogAscentAIAgent);
DEFINE_LOG_CATEGORY(LogAgenticMCP);

const FName FAscentAIAgentModule::AIAgentTabName(TEXT("AscentAIAgent"));

void FAscentAIAgentModule::StartupModule()
{
    UE_LOG(LogAscentAIAgent, Log, TEXT("AscentAIAgent module starting up"));

    FAscentAIAgentStyle::Initialize();
    FAscentAIAgentStyle::ReloadTextures();

    FAscentAIAgentCommands::Register();
    RegisterCommands();

    // Yarin's private MCP server (AscentEpicMCP) serves these together with Epic's toolsets.
    for (const TSubclassOf<UToolsetDefinition>& ToolsetClass : FAscentToolsetUtils::GetToolsetClasses())
    {
        UToolsetRegistry::RegisterToolsetClass(ToolsetClass);
    }

    FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
        AIAgentTabName,
        FOnSpawnTab::CreateRaw(this, &FAscentAIAgentModule::OnSpawnAIAgentTab))
        .SetDisplayName(LOCTEXT("AIAgentTabTitle", "Yarin AI"))
        .SetTooltipText(LOCTEXT("AIAgentTabTooltip", "Open Yarin AI"))
        .SetGroup(WorkspaceMenu::GetMenuStructure().GetToolsCategory())
        .SetIcon(FSlateIcon(FAscentAIAgentStyle::GetStyleSetName(), "AscentAIAgent.TabIcon"));

    if (UToolMenus::IsToolMenuUIEnabled())
    {
        RegisterMenus();
    }
    else
    {
        UToolMenus::Get()->RegisterStartupCallback(
            FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FAscentAIAgentModule::RegisterMenus));
    }

    UE_LOG(LogAscentAIAgent, Log, TEXT("AscentAIAgent module started"));
}

void FAscentAIAgentModule::ShutdownModule()
{
    UE_LOG(LogAscentAIAgent, Log, TEXT("AscentAIAgent module shutting down"));

    for (const TSubclassOf<UToolsetDefinition>& ToolsetClass : FAscentToolsetUtils::GetToolsetClasses())
    {
        UToolsetRegistry::UnregisterToolsetClass(ToolsetClass);
    }

    if (UToolMenus::IsToolMenuUIEnabled())
    {
        UToolMenus::Get()->RemoveSection("LevelEditor.MainMenu.Window", "ACFTools");
        UToolMenus::Get()->RemoveSection("LevelEditor.LevelEditorToolBar.PlayToolBar", "ACF");
    }

    FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(AIAgentTabName);

    FAscentAIAgentCommands::Unregister();
    FAscentAIAgentStyle::Shutdown();

    PluginCommands.Reset();
}

FAscentAIAgentModule& FAscentAIAgentModule::Get()
{
    return FModuleManager::LoadModuleChecked<FAscentAIAgentModule>("AscentAIAgent");
}

bool FAscentAIAgentModule::IsAvailable()
{
    return FModuleManager::Get().IsModuleLoaded("AscentAIAgent");
}

void FAscentAIAgentModule::RegisterCommands()
{
    PluginCommands = MakeShareable(new FUICommandList);
    PluginCommands->MapAction(
        FAscentAIAgentCommands::Get().OpenAIAgentTab,
        FExecuteAction::CreateRaw(this, &FAscentAIAgentModule::SpawnAIAgentTab),
        FCanExecuteAction());
}

void FAscentAIAgentModule::RegisterMenus()
{
    UToolMenu* WindowMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window");
    if (WindowMenu)
    {
        FToolMenuSection& Section = WindowMenu->FindOrAddSection("ACFTools");
        Section.AddMenuEntryWithCommandList(
            FAscentAIAgentCommands::Get().OpenAIAgentTab,
            PluginCommands,
            LOCTEXT("YarinMenuEntry", "Yarin AI"),
            LOCTEXT("YarinMenuTooltip", "Open Yarin AI"),
            FSlateIcon(FAscentAIAgentStyle::GetStyleSetName(), "AscentAIAgent.TabIcon"));
    }

    UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar.PlayToolBar");
    if (ToolbarMenu)
    {
        FToolMenuSection& ToolbarSection = ToolbarMenu->FindOrAddSection("ACF");
        FToolMenuEntry& ToolbarEntry = ToolbarSection.AddEntry(FToolMenuEntry::InitToolBarButton(
            FAscentAIAgentCommands::Get().OpenAIAgentTab,
            LOCTEXT("AIAgentToolbar", "Yarin"),
            LOCTEXT("AIAgentToolbarTooltip", "Open Yarin (Ctrl+Shift+A)"),
            FSlateIcon(FAscentAIAgentStyle::GetStyleSetName(), "AscentAIAgent.ToolbarIcon")));
        ToolbarEntry.StyleNameOverride = "CalloutToolbar";
        ToolbarEntry.SetCommandList(PluginCommands);
    }
}

void FAscentAIAgentModule::SpawnAIAgentTab()
{
    FGlobalTabmanager::Get()->TryInvokeTab(AIAgentTabName);
}

TSharedRef<SDockTab> FAscentAIAgentModule::OnSpawnAIAgentTab(const FSpawnTabArgs& SpawnTabArgs)
{
    TSharedRef<SDockTab> DockTab = SNew(SDockTab)
        .TabRole(ETabRole::NomadTab)
        .Label(LOCTEXT("AIAgentTabLabel", "Yarin AI"));

    FACFAssistantPanelFactory& NativeFactory = GetACFAssistantPanelFactory();
    if (NativeFactory.IsBound())
    {
        DockTab->SetContent(NativeFactory.Execute());
        return DockTab;
    }

    DockTab->SetContent(
        SNew(STextBlock)
        .Text(LOCTEXT("NativePanelMissing", "Yarin's native panel is not available.")));
    return DockTab;
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FAscentAIAgentModule, AscentAIAgent)
