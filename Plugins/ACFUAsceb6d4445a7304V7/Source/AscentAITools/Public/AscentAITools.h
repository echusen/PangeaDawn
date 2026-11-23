// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

class SDockTab;
class FSpawnTabArgs;
class FToolBarBuilder;
class FMenuBuilder;

/**
 * Editor module that adds AI generation tools (Nano Banana image generation and
 * Meshy 3D generation) to the editor, accessible from a dockable tab.
 */
class FACFAIToolsModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	void RegisterMenus();
	TSharedRef<SDockTab> OnSpawnTab(const FSpawnTabArgs& SpawnTabArgs);
	void OpenTab();

	static const FName TabId;
};
