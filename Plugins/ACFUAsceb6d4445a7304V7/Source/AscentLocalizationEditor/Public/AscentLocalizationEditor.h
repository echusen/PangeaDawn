// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(LogAscentLocalizationEditor, Log, All);

class FAscentLocalizationEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	void RegisterMenus();
	void UnregisterMenus();
	void OpenLocalizationTab();  
	TSharedRef<class SDockTab> SpawnLocalizationTab(const class FSpawnTabArgs& Args);
};
