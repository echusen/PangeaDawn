// Copyright Epic Games, Inc. All Rights Reserved.

#include "IFabLauncherQuixelDependenciesModule.h"
#include "Modules/ModuleManager.h"

#include "InterchangeManager.h"
#include "Pipelines/Factories/FabInterchangeInstancedFoliageTypeFactory.h"

#define LOCTEXT_NAMESPACE "FabLauncherQuixelDependencies"
DEFINE_LOG_CATEGORY(LogFabLauncherQuixelDependencies)

class FFabLauncherQuixelDependencies : public IFabLauncherQuixelDependenciesModule
{
public:
    virtual void StartupModule() override {

        auto RegisterItems = []()
        {
            UInterchangeManager::GetInterchangeManager().RegisterFactory(UFabInterchangeInstancedFoliageTypeFactory::StaticClass());
        };

        if (GEngine)
        {
            RegisterItems();
        }
        else
        {
            FCoreDelegates::OnPostEngineInit.AddLambda(RegisterItems);
        }
    };
    virtual void ShutdownModule() override {};
};

IMPLEMENT_MODULE(FFabLauncherQuixelDependencies, FabLauncherQuixelDependencies);
