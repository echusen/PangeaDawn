// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UToolMenu;
struct FPropertyChangedEvent;

/**
 * Owns the Ascent First Person combo button on the level editor toolbar and the dropdown it opens.
 *
 * The dropdown is generated from the tool registry and the editor settings every time it opens, so new
 * tools and settings edits appear without a restart. The button itself is re-registered on a settings
 * change so its own label and icon stay in sync.
 */
class FACFFirstPersonEditorToolbar : public TSharedFromThis<FACFFirstPersonEditorToolbar>
{
public:
	void Initialize();

	void Shutdown();

private:
	/** Registers, or re-registers, the menus from current settings. */
	void RegisterMenus();

	void UnregisterMenus();

	/** Fills the dropdown: registered tools first, then the settings' own entries. */
	void PopulateDropdown(UToolMenu* Menu);

	/** Documentation, support and settings shortcuts, appended to every dropdown. */
	void PopulateFooter(UToolMenu* Menu) const;

	void HandleSettingChanged(UObject* Object, FPropertyChangedEvent& PropertyChangedEvent);

private:
	FDelegateHandle ToolMenusStartupHandle;

	FDelegateHandle SettingsChangedHandle;

	bool bMenusRegistered = false;
};
