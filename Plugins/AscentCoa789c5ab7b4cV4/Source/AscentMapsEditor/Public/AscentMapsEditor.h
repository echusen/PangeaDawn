// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

static const FString PluginName = "AscentCombatFramework";

class FAscentMapsEditor : public IModuleInterface {
public:
    /** IModuleInterface implementation */
    void StartupModule() override;
    void ShutdownModule() override;

    /** This function will be bound to Command. */
    void PluginButtonClicked();

private:
    void RegisterMenus();

	TSharedPtr<class FUICommandList> PluginCommands;
};
