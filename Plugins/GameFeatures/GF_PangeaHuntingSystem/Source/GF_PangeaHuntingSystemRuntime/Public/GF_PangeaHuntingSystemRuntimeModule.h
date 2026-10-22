// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once
#include "Modules/ModuleManager.h"

/** Component lifetime belongs exclusively to GameFeatureAction_AddComponents.
 * Loading this module must not independently inject gameplay components. */
class FGF_PangeaHuntingSystemRuntimeModule : public IModuleInterface
{
};