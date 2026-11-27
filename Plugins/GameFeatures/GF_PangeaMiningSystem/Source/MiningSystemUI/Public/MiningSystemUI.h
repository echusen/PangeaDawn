#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/** Module entry point for the mining site, chest and stockpile menu widgets. It has no startup or shutdown work. */
class FMiningSystemUIModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
