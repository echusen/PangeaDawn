// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FACFFirstPersonEditorToolbar;

class FACFFirstPersonEditorModule : public IModuleInterface
{
public:
	//~ Begin IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~ End of IModuleInterface

private:
	/** Files the plugin's nomad tabs under their own workspace menu group. */
	void RegisterWorkspaceGroup();

	/** Adds the plugin's own tools to the registry. */
	void RegisterTools();

private:
	TSharedPtr<FACFFirstPersonEditorToolbar> Toolbar;
};
