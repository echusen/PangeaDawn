#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/** Editor-only module that hosts the mining system's editor and PIE automation tests. */
class FMiningSystemEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
