// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.

#include "AscentSensorGridShadersModule.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Interfaces/IPluginManager.h"
#include "ShaderCore.h"

IMPLEMENT_MODULE(FAscentSensorGridShadersModule, AscentSensorGridShaders);

namespace AscentSensorGridShadersLocal
{
	// ACF ships under different plugin folder/descriptor names (AscentCombatFramework, ACFUltimate, ...),
	// so resolve the owning plugin from this module instead of hardcoding a single name.
	static FString ResolveOwningPluginName()
	{
		for (const TSharedRef<IPlugin>& Plugin : IPluginManager::Get().GetEnabledPlugins())
		{
			for (const FModuleDescriptor& Module : Plugin->GetDescriptor().Modules)
			{
				if (Module.Name == TEXT("AscentSensorGridShaders"))
				{
					return Plugin->GetName();
				}
			}
		}

		return TEXT("AscentCombatFramework");
	}
}

void FAscentSensorGridShadersModule::StartupModule()
{
	// Maps the virtual shader source directory /ACFSensorGrid to the plugin's actual sensor grid shader directory.
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(AscentSensorGridShadersLocal::ResolveOwningPluginName());
	if (!Plugin.IsValid())
	{
		return;
	}

	const FString PluginShaderDir = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Shaders"), TEXT("SensorGrid"));
	AddShaderSourceDirectoryMapping(TEXT("/ACFSensorGrid"), PluginShaderDir);
}
