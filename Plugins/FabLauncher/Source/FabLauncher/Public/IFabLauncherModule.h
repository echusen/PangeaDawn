// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"	

#define FL_MODULE_NAME TEXT("FabLauncher")
DECLARE_LOG_CATEGORY_EXTERN(LogFabLauncher, Log, All);

class IFabLauncherModule : public IModuleInterface
{

public:

	static inline IFabLauncherModule& Get()
	{
		return FModuleManager::LoadModuleChecked<IFabLauncherModule>(FL_MODULE_NAME);
	}

	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded(FL_MODULE_NAME);
	}
};

