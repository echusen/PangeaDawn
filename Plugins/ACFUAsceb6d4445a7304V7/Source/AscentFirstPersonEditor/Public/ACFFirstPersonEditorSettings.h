// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Textures/SlateIcon.h"
#include "ACFFirstPersonEditorSettings.generated.h"

class UEditorUtilityWidgetBlueprint;

/** Where a toolbar icon comes from. */
UENUM()
enum class EACFProceduralIconSource : uint8
{
	/** Whatever the tool registered, or no icon for a custom entry. */
	Default,
	/** An image in the plugin's Resources folder. */
	PluginResource,
	/** A named brush in the editor's own style, e.g. Icons.Settings. */
	EditorStyle
};

/** Icon for a dropdown row or the toolbar button itself. */
USTRUCT()
struct FACFProceduralToolbarIcon
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category="Icon")
	EACFProceduralIconSource Source = EACFProceduralIconSource::Default;

	/** Drop a PNG into the plugin's Resources folder and it appears here. */
	UPROPERTY(EditAnywhere, Category="Icon", meta=(EditCondition="Source == EACFProceduralIconSource::PluginResource", EditConditionHides, GetOptions="GetPluginIconOptions"))
	FName ResourceName;

	UPROPERTY(EditAnywhere, Category="Icon", meta=(EditCondition="Source == EACFProceduralIconSource::EditorStyle", EditConditionHides, GetOptions="GetEditorStyleIconOptions"))
	FName StyleName;

	/** Falls back whenever the source is Default or the chosen name is unset. */
	FSlateIcon Resolve(const FSlateIcon& Fallback) const;
};

/**
 * Customisation for one of the plugin's own tools.
 *
 * The tools themselves are registered in C++ and cannot be removed, but everything about how they are
 * presented is yours: label, tooltip, section, order, icon, and whether they appear at all.
 */
USTRUCT()
struct FACFProceduralToolOverride
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category="Tool", meta=(GetOptions="GetRegisteredToolOptions"))
	FName ToolId;

	/** Leaves the tool registered and invokable by tab id, just absent from the menu. */
	UPROPERTY(EditAnywhere, Category="Tool")
	bool bHidden = false;

	/** Empty keeps the built-in text. */
	UPROPERTY(EditAnywhere, Category="Tool")
	FText Label;

	UPROPERTY(EditAnywhere, Category="Tool")
	FText Tooltip;

	/** None keeps the built-in section. */
	UPROPERTY(EditAnywhere, Category="Tool")
	FName Section;

	/** Negative keeps the built-in ordering. */
	UPROPERTY(EditAnywhere, Category="Tool")
	int32 Order = -1;

	UPROPERTY(EditAnywhere, Category="Tool")
	FACFProceduralToolbarIcon Icon;
};

/** What a dropdown entry does when clicked. */
UENUM()
enum class EACFProceduralToolbarEntryType : uint8
{
	/** Invokes a globally-registered nomad tab by id. */
	TabSpawner,
	/** Spawns an Editor Utility Widget asset. */
	EditorUtilityWidget,
	/** Runs an editor console command. */
	ConsoleCommand
};

/**
 * One customer-authored row in the Ascent First Person toolbar dropdown.
 *
 * The plugin's own tools come from FACFProceduralEditorToolRegistry and do not need an entry here. This is
 * for adding your own: any registered nomad tab, an Editor Utility Widget, or a console command.
 */
USTRUCT()
struct FACFProceduralToolbarEntry
{
	GENERATED_BODY()

	/** Stable, unique menu-entry name. */
	UPROPERTY(EditAnywhere, Category="Entry")
	FName Name = TEXT("ToolbarEntry");

	UPROPERTY(EditAnywhere, Category="Entry")
	FText Label;

	UPROPERTY(EditAnywhere, Category="Entry")
	FText Tooltip;

	/** Section the row is grouped under. A new section name produces a separator. */
	UPROPERTY(EditAnywhere, Category="Entry")
	FName Section = TEXT("Custom");

	/** Heading drawn above the section. Only the first entry of a section needs to set it. */
	UPROPERTY(EditAnywhere, Category="Entry")
	FText SectionLabel;

	UPROPERTY(EditAnywhere, Category="Entry")
	FACFProceduralToolbarIcon Icon;

	UPROPERTY(EditAnywhere, Category="Entry")
	EACFProceduralToolbarEntryType EntryType = EACFProceduralToolbarEntryType::TabSpawner;

	UPROPERTY(EditAnywhere, Category="Entry", meta=(EditCondition="EntryType == EACFProceduralToolbarEntryType::TabSpawner", EditConditionHides))
	FName TabId;

	UPROPERTY(EditAnywhere, Category="Entry", meta=(EditCondition="EntryType == EACFProceduralToolbarEntryType::EditorUtilityWidget", EditConditionHides))
	TSoftObjectPtr<UEditorUtilityWidgetBlueprint> EditorUtilityWidget;

	UPROPERTY(EditAnywhere, Category="Entry", meta=(EditCondition="EntryType == EACFProceduralToolbarEntryType::ConsoleCommand", EditConditionHides))
	FString ConsoleCommand;

};

/**
 * Editor-only configuration for the Ascent First Person toolbar dropdown.
 *
 * Leaving everything at its defaults is correct: the plugin's tools are registered in C++ and listed
 * automatically. These properties only exist to hide, extend or rebrand that list.
 */
UCLASS(Config=Editor, DefaultConfig, meta=(DisplayName="Ascent First Person (Editor)"))
class ASCENTFIRSTPERSONEDITOR_API UACFFirstPersonEditorSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UACFFirstPersonEditorSettings();

	//~ Begin UDeveloperSettings interface
	virtual FName GetContainerName() const override;
	virtual FName GetCategoryName() const override;
	virtual FName GetSectionName() const override;
	//~ End of UDeveloperSettings interface

	static const UACFFirstPersonEditorSettings* Get() { return GetDefault<UACFFirstPersonEditorSettings>(); }

	FSlateIcon MakeDropdownIcon() const;

	/** Customisation for ToolId, or null when it has none. */
	const FACFProceduralToolOverride* FindToolOverride(const FName& ToolId) const;

	//~ Begin GetOptions sources for the icon and tool pickers
	UFUNCTION()
	TArray<FString> GetPluginIconOptions() const;

	UFUNCTION()
	TArray<FString> GetEditorStyleIconOptions() const;

	UFUNCTION()
	TArray<FString> GetRegisteredToolOptions() const;
	//~ End of GetOptions sources

public:
	/** Turn off when a host project aggregates these tools into its own toolbar dropdown. */
	// FOR NOW: hidden by default until the first-person tools are ready to ship.
	UPROPERTY(Config, EditAnywhere, Category="Toolbar")
	bool bShowToolbarButton = false;

	/** Also mount the same menu under Tools in the main menu bar. */
	UPROPERTY(Config, EditAnywhere, Category="Toolbar")
	bool bShowInToolsMenu = true;

	UPROPERTY(Config, EditAnywhere, Category="Toolbar")
	FText DropdownLabel;

	UPROPERTY(Config, EditAnywhere, Category="Toolbar")
	FText DropdownTooltip;

	UPROPERTY(Config, EditAnywhere, Category="Toolbar")
	FACFProceduralToolbarIcon DropdownIcon;

	/** Section of the level editor's User toolbar to place the button in. */
	UPROPERTY(Config, EditAnywhere, Category="Toolbar", AdvancedDisplay)
	FName ToolbarSectionName = TEXT("ACFFirstPerson");

	/** Relabel, re-icon, reorder or hide the plugin's own tools. Anything left blank keeps its built-in value. */
	UPROPERTY(Config, EditAnywhere, Category="Tools", meta=(TitleProperty="ToolId"))
	TArray<FACFProceduralToolOverride> ToolOverrides;

	/**
	 * Presets panel: while playing, write a preset data change to the blueprint defaults as well as the
	 * live instance, so it survives stopping play.
	 *
	 * Turn off to change only the running instance — useful for trying something without touching the asset.
	 */
	UPROPERTY(Config, EditAnywhere, Category="Presets")
	bool bWriteToBlueprintDefaults = true;

	/** Extra rows appended after the plugin's own tools. */
	UPROPERTY(Config, EditAnywhere, Category="Tools")
	TArray<FACFProceduralToolbarEntry> Entries;

};
