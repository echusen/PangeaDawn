// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

CLIMBINGSYSTEM_API DECLARE_LOG_CATEGORY_EXTERN(LogACFFreeClimb, Log, All);

class FClimbingSystemModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
