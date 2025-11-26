// Copyright Epic Games, Inc. All Rights Reserved.
#include "IFabLauncherModule.h"
#include "FLTCPServerImp.h"

#include "Modules/ModuleManager.h"
#include "Interfaces/IPluginManager.h"

#define LOCTEXT_NAMESPACE "FabLauncher"
DEFINE_LOG_CATEGORY(LogFabLauncher);

class FabLauncherModule : public IFabLauncherModule
{
public:
	virtual void StartupModule() override
	{
		if (tcpServer == NULL)
		{
			tcpServer = new FTCPServer();
		}

		// Display a warning message if Bridge is enabled
		if (!FModuleManager::Get().IsModuleLoaded("Bridge") && IPluginManager::Get().FindPlugin(TEXT("Bridge")).Get()) {
			UE_LOG(LogFabLauncher, Warning, TEXT("Quixel Bridge plugin is enabled. Bridge settings previously set won't apply."));
		}

		// Display a message stating that the plugin is active
		const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("FabLauncher"));
		FString PluginVersion = Plugin.IsValid() ? Plugin->GetDescriptor().VersionName : "unknown";
		UE_LOG(LogFabLauncher, Display, TEXT("FabLauncher (version %s) active"), *PluginVersion);
	}

	virtual void ShutdownModule() override
	{
		if ((GIsEditor && !IsRunningCommandlet()))
		{
			
		}
	}
private:
	FTCPServer *tcpServer = NULL;
};

IMPLEMENT_MODULE(FabLauncherModule, FabLauncher);

#undef LOCTEXT_NAMESPACE
