// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "ACFFirstPersonEditorSettings.h"

#include "ACFProceduralEditorToolRegistry.h"
#include "ACFFirstPersonEditorStyle.h"
#include "Styling/AppStyle.h"
#include "Styling/SlateStyleRegistry.h"

#define LOCTEXT_NAMESPACE "ACFFirstPersonEditor"

FSlateIcon FACFProceduralToolbarIcon::Resolve(const FSlateIcon& Fallback) const
{
	switch (Source)
	{
	case EACFProceduralIconSource::PluginResource:
		if (!ResourceName.IsNone())
		{
			return FSlateIcon(
				FACFFirstPersonEditorStyle::GetStyleSetName(),
				FACFFirstPersonEditorStyle::MakeResourceStyleName(ResourceName, false),
				FACFFirstPersonEditorStyle::MakeResourceStyleName(ResourceName, true));
		}
		break;

	case EACFProceduralIconSource::EditorStyle:
		if (!StyleName.IsNone())
		{
			return FSlateIcon(FAppStyle::GetAppStyleSetName(), StyleName);
		}
		break;

	default:
		break;
	}

	return Fallback;
}

UACFFirstPersonEditorSettings::UACFFirstPersonEditorSettings()
{
	DropdownLabel = LOCTEXT("DropdownLabel", "Ascent FP");
	DropdownTooltip = LOCTEXT("DropdownTooltip", "Editor tools for Ascent First Person.");
}

FName UACFFirstPersonEditorSettings::GetContainerName() const
{
	return TEXT("Project");
}

FName UACFFirstPersonEditorSettings::GetCategoryName() const
{
	return TEXT("Game");
}

FName UACFFirstPersonEditorSettings::GetSectionName() const
{
	return TEXT("Ascent First Person Editor");
}

FSlateIcon UACFFirstPersonEditorSettings::MakeDropdownIcon() const
{
	const FSlateIcon DefaultIcon(FACFFirstPersonEditorStyle::GetStyleSetName(), TEXT("ACFFirstPerson.ToolbarIcon"));
	return DropdownIcon.Resolve(DefaultIcon);
}

const FACFProceduralToolOverride* UACFFirstPersonEditorSettings::FindToolOverride(const FName& ToolId) const
{
	return ToolOverrides.FindByPredicate([&ToolId](const FACFProceduralToolOverride& Override)
	{
		return Override.ToolId == ToolId;
	});
}

TArray<FString> UACFFirstPersonEditorSettings::GetPluginIconOptions() const
{
	return FACFFirstPersonEditorStyle::GetResourceIconNames();
}

TArray<FString> UACFFirstPersonEditorSettings::GetEditorStyleIconOptions() const
{
	TArray<FString> Options;

	const ISlateStyle* AppStyle = FSlateStyleRegistry::FindSlateStyle(FAppStyle::GetAppStyleSetName());
	if (!AppStyle)
	{
		return Options;
	}

	// GetStyleKeys returns every key, most of which are fonts, colours and widget styles. Only the ones
	// that resolve to a brush can be used as an icon, and offering the rest would make the list useless.
	for (const FName& Key : AppStyle->GetStyleKeys())
	{
		if (AppStyle->GetOptionalBrush(Key, nullptr, nullptr) != nullptr)
		{
			Options.Add(Key.ToString());
		}
	}

	Options.Sort();

	return Options;
}

TArray<FString> UACFFirstPersonEditorSettings::GetRegisteredToolOptions() const
{
	TArray<FString> Options;

	for (const FACFProceduralEditorToolDescriptor& Tool : FACFProceduralEditorToolRegistry::Get().GetTools())
	{
		Options.Add(Tool.Id.ToString());
	}

	Options.Sort();

	return Options;
}

#undef LOCTEXT_NAMESPACE
