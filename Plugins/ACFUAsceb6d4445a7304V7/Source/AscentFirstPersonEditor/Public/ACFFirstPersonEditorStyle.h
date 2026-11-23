// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FSlateStyleSet;
class ISlateStyle;

/** Slate style set for the plugin's editor tooling. Icons live in the plugin's Resources folder. */
class ASCENTFIRSTPERSONEDITOR_API FACFFirstPersonEditorStyle
{
public:
	static void Register();

	static void Unregister();

	static const ISlateStyle& Get();

	static FName GetStyleSetName();

	/**
	 * Base filenames of every image in the plugin's Resources folder, sorted.
	 *
	 * Each is registered as "ACFFirstPerson.<Name>" plus a ".Small" variant, so dropping a PNG in
	 * that folder is all it takes to make it selectable as a tool icon.
	 */
	static const TArray<FString>& GetResourceIconNames();

	/** Style name for a resource icon, and its small variant. */
	static FName MakeResourceStyleName(const FName& ResourceName, bool bSmall = false);

private:
	static TSharedRef<FSlateStyleSet> Create();

	static TSharedPtr<FSlateStyleSet> StyleInstance;
};
