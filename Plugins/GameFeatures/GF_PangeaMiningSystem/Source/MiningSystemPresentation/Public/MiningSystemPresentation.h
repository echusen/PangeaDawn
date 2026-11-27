#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/** Module entry point for the cosmetic mining agent presentation code. It has no startup or shutdown work. */
class FMiningSystemPresentationModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
