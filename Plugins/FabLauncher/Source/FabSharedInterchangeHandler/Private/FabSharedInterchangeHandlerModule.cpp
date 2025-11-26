// Copyright Epic Games, Inc. All Rights Reserved.

#include "FabSharedInterchangeHandlerModule.h"

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "FabSharedInterchangeHandler.h"

class FFabSharedInterchangeHandlerModule : public IFabSharedInterchangeHandlerModule
{
	virtual void StartupModule() override {}
	virtual void ShutdownModule() override {}
};

IMPLEMENT_MODULE(FFabSharedInterchangeHandlerModule, FabSharedInterchangeHandler)
