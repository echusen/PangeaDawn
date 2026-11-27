// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/** Module entry point for the mining system runtime; exists so the game feature plugin can load this module. */
class FGF_PangeaMiningSystemRuntimeModule : public IModuleInterface
{
public:
	//~IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	//~End of IModuleInterface
};
