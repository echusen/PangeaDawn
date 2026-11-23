// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(LogAscentAIAgent, Log, All);

class FAscentAIAgentModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

    static FAscentAIAgentModule& Get();
    static bool IsAvailable();

    /** Opens the native Slate Yarin tab. */
    void SpawnAIAgentTab();

private:
    void RegisterMenus();
    void RegisterCommands();
    TSharedRef<class SDockTab> OnSpawnAIAgentTab(const class FSpawnTabArgs& SpawnTabArgs);

    TSharedPtr<class FUICommandList> PluginCommands;

    static const FName AIAgentTabName;
};
