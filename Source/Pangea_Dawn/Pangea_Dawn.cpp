// Copyright Epic Games, Inc. All Rights Reserved.

#include "Pangea_Dawn.h"
#include "Modules/ModuleManager.h"

class FPangeaDawnModule : public FDefaultGameModuleImpl
{
public:
	virtual void StartupModule() override
	{
		FDefaultGameModuleImpl::StartupModule();
		// BUD_Village contains an instanced UA_UnlockMiningSite action. Its native
		// class must exist before the base Game Feature loads that definition.
		// Load only the type module here; Game Features still owns activation,
		// component injection, and the mining presentation/UI modules.
		FModuleManager::LoadModuleChecked<IModuleInterface>(TEXT("GF_PangeaMiningSystemRuntime"));
	}
};

IMPLEMENT_PRIMARY_GAME_MODULE( FPangeaDawnModule, Pangea_Dawn, "Pangea_Dawn" );

DEFINE_LOG_CATEGORY(LogPangea_Dawn)
