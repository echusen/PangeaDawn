// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "ACFFirstPersonEditorToolbar.h"

#include "ACFProceduralEditorToolRegistry.h"
#include "ACFFirstPersonEditorSettings.h"
#include "ACFFirstPersonEditorStyle.h"

#include "Editor.h"
#include "EditorUtilitySubsystem.h"
#include "EditorUtilityWidgetBlueprint.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "ISettingsModule.h"
#include "Interfaces/IPluginManager.h"
#include "Styling/AppStyle.h"
#include "ToolMenus.h"

#define LOCTEXT_NAMESPACE "ACFFirstPersonEditor"

namespace ACFFirstPersonEditorToolbar
{
	/** Single owner, so the whole extension can be dropped and re-added on a settings change. */
	static const FName OwnerName(TEXT("ACFFirstPersonEditorToolbar"));

	/** Extension point on the level editor toolbar reserved for project and plugin tools. */
	static const FName UserToolbarMenu(TEXT("LevelEditor.LevelEditorToolBar.User"));

	static const FName MainToolsMenu(TEXT("LevelEditor.MainMenu.Tools"));

	static const FName ComboEntryName(TEXT("ACFFirstPersonTools"));

	static const FName DefaultSectionName(TEXT("Tools"));

	static const FName FooterSectionName(TEXT("ACFFirstPersonHelp"));

	/** The plugin is found through this module rather than by name, the way ACF's own editor modules do. */
	static const FName ModuleName(TEXT("AscentFirstPersonEditor"));

	static void ExecuteEntry(FACFProceduralToolbarEntry Entry)
	{
		switch (Entry.EntryType)
		{
		case EACFProceduralToolbarEntryType::TabSpawner:
			if (!Entry.TabId.IsNone())
			{
				FGlobalTabmanager::Get()->TryInvokeTab(Entry.TabId);
			}
			break;

		case EACFProceduralToolbarEntryType::EditorUtilityWidget:
			if (GEditor)
			{
				if (UEditorUtilityWidgetBlueprint* Blueprint = Entry.EditorUtilityWidget.LoadSynchronous())
				{
					if (UEditorUtilitySubsystem* Subsystem = GEditor->GetEditorSubsystem<UEditorUtilitySubsystem>())
					{
						Subsystem->SpawnAndRegisterTab(Blueprint);
					}
				}
			}
			break;

		case EACFProceduralToolbarEntryType::ConsoleCommand:
			if (GEditor && !Entry.ConsoleCommand.IsEmpty())
			{
				GEditor->Exec(GEditor->GetEditorWorldContext().World(), *Entry.ConsoleCommand);
			}
			break;
		}
	}

	static void InvokeTab(FName TabId)
	{
		FGlobalTabmanager::Get()->TryInvokeTab(TabId);
	}

	static void LaunchURL(FString Url)
	{
		if (!Url.IsEmpty())
		{
			FPlatformProcess::LaunchURL(*Url, nullptr, nullptr);
		}
	}

	static void OpenSettings()
	{
		if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
		{
			const UACFFirstPersonEditorSettings* Settings = UACFFirstPersonEditorSettings::Get();
			SettingsModule->ShowViewer(Settings->GetContainerName(), Settings->GetCategoryName(), Settings->GetSectionName());
		}
	}
}

void FACFFirstPersonEditorToolbar::Initialize()
{
	// Commandlets and cookers have no toolbar to extend.
	if (!FSlateApplication::IsInitialized())
	{
		return;
	}

	ToolMenusStartupHandle = UToolMenus::RegisterStartupCallback(
		FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FACFFirstPersonEditorToolbar::RegisterMenus));

	SettingsChangedHandle = GetMutableDefault<UACFFirstPersonEditorSettings>()
		->OnSettingChanged().AddRaw(this, &FACFFirstPersonEditorToolbar::HandleSettingChanged);
}

void FACFFirstPersonEditorToolbar::Shutdown()
{
	if (SettingsChangedHandle.IsValid())
	{
		if (UObjectInitialized())
		{
			GetMutableDefault<UACFFirstPersonEditorSettings>()->OnSettingChanged().Remove(SettingsChangedHandle);
		}

		SettingsChangedHandle.Reset();
	}

	if (UObjectInitialized() && ToolMenusStartupHandle.IsValid())
	{
		UToolMenus::UnRegisterStartupCallback(this);
		UnregisterMenus();
		ToolMenusStartupHandle.Reset();
	}
}

void FACFFirstPersonEditorToolbar::RegisterMenus()
{
	using namespace ACFFirstPersonEditorToolbar;

	if (!UToolMenus::IsToolMenuUIEnabled())
	{
		return;
	}

	// Start clean so this doubles as a refresh.
	UnregisterMenus();

	const UACFFirstPersonEditorSettings* Settings = UACFFirstPersonEditorSettings::Get();

	FToolMenuOwnerScoped OwnerScoped(OwnerName);

	if (Settings->bShowToolbarButton)
	{
		UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu(UserToolbarMenu);
		FToolMenuSection& Section = ToolbarMenu->FindOrAddSection(Settings->ToolbarSectionName);

		FToolMenuEntry ComboEntry = FToolMenuEntry::InitComboButton(
			ComboEntryName,
			FUIAction(),
			FNewToolMenuChoice(FNewToolMenuDelegate::CreateRaw(this, &FACFFirstPersonEditorToolbar::PopulateDropdown)),
			Settings->DropdownLabel,
			Settings->DropdownTooltip,
			Settings->MakeDropdownIcon());
		ComboEntry.StyleNameOverride = "CalloutToolbar";

		Section.AddEntry(ComboEntry);
	}

	if (Settings->bShowInToolsMenu)
	{
		UToolMenu* ToolsMenu = UToolMenus::Get()->ExtendMenu(MainToolsMenu);
		FToolMenuSection& Section = ToolsMenu->FindOrAddSection(ComboEntryName, LOCTEXT("ToolsMenuSection", "Ascent First Person"));

		Section.AddSubMenu(
			ComboEntryName,
			Settings->DropdownLabel,
			Settings->DropdownTooltip,
			FNewToolMenuChoice(FNewToolMenuDelegate::CreateRaw(this, &FACFFirstPersonEditorToolbar::PopulateDropdown)),
			false,
			Settings->MakeDropdownIcon());
	}

	bMenusRegistered = true;
}

void FACFFirstPersonEditorToolbar::UnregisterMenus()
{
	if (bMenusRegistered && UToolMenus::IsToolMenuUIEnabled())
	{
		UToolMenus::Get()->UnregisterOwnerByName(ACFFirstPersonEditorToolbar::OwnerName);
		UToolMenus::Get()->RefreshAllWidgets();
	}

	bMenusRegistered = false;
}

void FACFFirstPersonEditorToolbar::PopulateDropdown(UToolMenu* Menu)
{
	using namespace ACFFirstPersonEditorToolbar;

	if (!Menu)
	{
		return;
	}

	for (const FACFProceduralResolvedTool& Tool : FACFProceduralEditorToolRegistry::Get().GetVisibleTools())
	{
		const FName SectionName = Tool.Section.IsNone() ? DefaultSectionName : Tool.Section;
		FToolMenuSection& Section = Menu->FindOrAddSection(SectionName, Tool.SectionLabel);

		Section.AddMenuEntry(
			Tool.Id,
			Tool.Label,
			Tool.Tooltip,
			Tool.Icon,
			FUIAction(FExecuteAction::CreateStatic(&InvokeTab, Tool.Id)));
	}

	for (const FACFProceduralToolbarEntry& Entry : UACFFirstPersonEditorSettings::Get()->Entries)
	{
		const FName SectionName = Entry.Section.IsNone() ? DefaultSectionName : Entry.Section;
		FToolMenuSection& Section = Menu->FindOrAddSection(SectionName, Entry.SectionLabel);

		const FText Label = Entry.Label.IsEmpty() ? FText::FromName(Entry.Name) : Entry.Label;

		Section.AddMenuEntry(
			Entry.Name,
			Label,
			Entry.Tooltip,
			Entry.Icon.Resolve(FSlateIcon()),
			FUIAction(FExecuteAction::CreateStatic(&ExecuteEntry, Entry)));
	}

	PopulateFooter(Menu);
}

void FACFFirstPersonEditorToolbar::PopulateFooter(UToolMenu* Menu) const
{
	using namespace ACFFirstPersonEditorToolbar;

	FToolMenuSection& Section = Menu->FindOrAddSection(FooterSectionName, LOCTEXT("FooterSection", "Help"));

	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().GetModuleOwnerPlugin(ModuleName);
	if (Plugin.IsValid())
	{
		const FPluginDescriptor& Descriptor = Plugin->GetDescriptor();

		if (!Descriptor.DocsURL.IsEmpty())
		{
			Section.AddMenuEntry(
				TEXT("Documentation"),
				LOCTEXT("Documentation", "Documentation"),
				LOCTEXT("DocumentationTooltip", "Open the online documentation."),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Documentation"),
				FUIAction(FExecuteAction::CreateStatic(&LaunchURL, Descriptor.DocsURL)));
		}

		if (!Descriptor.SupportURL.IsEmpty())
		{
			Section.AddMenuEntry(
				TEXT("Support"),
				LOCTEXT("Support", "Support"),
				LOCTEXT("SupportTooltip", "Open the support channel."),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Help"),
				FUIAction(FExecuteAction::CreateStatic(&LaunchURL, Descriptor.SupportURL)));
		}
	}

	Section.AddMenuEntry(
		TEXT("EditorSettings"),
		LOCTEXT("EditorSettings", "Editor Settings"),
		LOCTEXT("EditorSettingsTooltip", "Configure which tools appear in this menu."),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Settings"),
		FUIAction(FExecuteAction::CreateStatic(&OpenSettings)));
}

void FACFFirstPersonEditorToolbar::HandleSettingChanged(UObject* Object, FPropertyChangedEvent& PropertyChangedEvent)
{
	RegisterMenus();
}

#undef LOCTEXT_NAMESPACE
