// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "ACFFirstPersonEditorStyle.h"

#include "HAL/FileManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleRegistry.h"

TSharedPtr<FSlateStyleSet> FACFFirstPersonEditorStyle::StyleInstance;

namespace ACFFirstPersonEditorStyle
{
	static const FVector2D IconSize16(16.0f, 16.0f);
	static const FVector2D IconSize20(20.0f, 20.0f);
	static const FVector2D IconSize64(64.0f, 64.0f);

	/** The plugin is found through this module rather than by name, the way ACF's own editor modules do. */
	static const FName ModuleName(TEXT("AscentFirstPersonEditor"));
}

void FACFFirstPersonEditorStyle::Register()
{
	if (StyleInstance.IsValid())
	{
		return;
	}

	StyleInstance = Create();
	FSlateStyleRegistry::RegisterSlateStyle(*StyleInstance);
}

void FACFFirstPersonEditorStyle::Unregister()
{
	if (!StyleInstance.IsValid())
	{
		return;
	}

	FSlateStyleRegistry::UnRegisterSlateStyle(*StyleInstance);
	StyleInstance.Reset();
}

const ISlateStyle& FACFFirstPersonEditorStyle::Get()
{
	return *StyleInstance;
}

FName FACFFirstPersonEditorStyle::GetStyleSetName()
{
	static const FName StyleSetName(TEXT("ACFFirstPersonEditorStyle"));
	return StyleSetName;
}

TSharedRef<FSlateStyleSet> FACFFirstPersonEditorStyle::Create()
{
	using namespace ACFFirstPersonEditorStyle;

	TSharedRef<FSlateStyleSet> Style = MakeShared<FSlateStyleSet>(GetStyleSetName());

	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().GetModuleOwnerPlugin(ModuleName);
	if (!Plugin.IsValid())
	{
		return Style;
	}

	const FString ResourcesDir = Plugin->GetBaseDir() / TEXT("Resources");
	Style->SetContentRoot(ResourcesDir);

	const FString IconPath = Style->RootToContentDir(TEXT("Icon128"), TEXT(".png"));

	Style->Set("ACFFirstPerson.Icon", new FSlateImageBrush(IconPath, IconSize64));
	Style->Set("ACFFirstPerson.ToolbarIcon", new FSlateImageBrush(IconPath, IconSize20));
	Style->Set("ACFFirstPerson.TabIcon", new FSlateImageBrush(IconPath, IconSize16));

	// Every image in Resources becomes selectable, so adding an icon is a file copy rather than a code change.
	for (const FString& ResourceName : GetResourceIconNames())
	{
		const FString Path = ResourcesDir / ResourceName + TEXT(".png");

		Style->Set(MakeResourceStyleName(*ResourceName, false), new FSlateImageBrush(Path, IconSize20));
		Style->Set(MakeResourceStyleName(*ResourceName, true), new FSlateImageBrush(Path, IconSize16));
	}

	return Style;
}

FName FACFFirstPersonEditorStyle::MakeResourceStyleName(const FName& ResourceName, bool bSmall)
{
	return FName(*FString::Printf(TEXT("ACFFirstPerson.Resource.%s%s"),
		*ResourceName.ToString(),
		bSmall ? TEXT(".Small") : TEXT("")));
}

const TArray<FString>& FACFFirstPersonEditorStyle::GetResourceIconNames()
{
	using namespace ACFFirstPersonEditorStyle;

	static TArray<FString> Names;
	static bool bGathered = false;

	if (bGathered)
	{
		return Names;
	}

	bGathered = true;

	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().GetModuleOwnerPlugin(ModuleName);
	if (!Plugin.IsValid())
	{
		return Names;
	}

	const FString ResourcesDir = Plugin->GetBaseDir() / TEXT("Resources");

	TArray<FString> Files;
	IFileManager::Get().FindFiles(Files, *(ResourcesDir / TEXT("*.png")), true, false);

	for (const FString& File : Files)
	{
		Names.Add(FPaths::GetBaseFilename(File));
	}

	Names.Sort();

	return Names;
}
