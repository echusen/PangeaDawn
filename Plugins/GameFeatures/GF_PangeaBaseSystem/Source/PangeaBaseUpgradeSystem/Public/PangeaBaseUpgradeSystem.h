#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/** Module entry point for the base/village upgrade system. It has no startup or shutdown work of its own. */
class FPangeaBaseUpgradeSystemModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
