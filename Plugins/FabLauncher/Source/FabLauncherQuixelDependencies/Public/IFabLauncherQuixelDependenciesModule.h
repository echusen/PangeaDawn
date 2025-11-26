// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"	

#define FL_MODULE_NAME TEXT("FabLauncherQuixelDependencies")
DECLARE_LOG_CATEGORY_EXTERN(LogFabLauncherQuixelDependencies, Log, All);

class IFabLauncherQuixelDependenciesModule : public IModuleInterface
{

public:

	static inline IFabLauncherQuixelDependenciesModule& Get()
	{
		return FModuleManager::LoadModuleChecked<IFabLauncherQuixelDependenciesModule>(FL_MODULE_NAME);
	}

	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded(FL_MODULE_NAME);
	}
};
