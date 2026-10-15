// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

/**
* AscentSensorGridShaders module interface. Loaded at PostConfigInit so the sensor grid global shaders
* and their virtual shader directory are registered before shader compilation starts.
*/
class FAscentSensorGridShadersModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;

private:
};

