// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

CLIMBINGSYSTEMEDITOR_API DECLARE_LOG_CATEGORY_EXTERN(LogACFFreeClimbEditor, Log, All);

class FClimbingSystemEditorModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
