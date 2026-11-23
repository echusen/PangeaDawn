// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/IPluginManager.h"

namespace ACFAssistantPlugin
{
	/**
	 * The plugin that ships this module: AscentCombatFramework inside ACF Ultimate, Yarin in the standalone package.
	 * UBT defines UE_PLUGIN_NAME per module from the owning .uplugin, so Resources/ (icons, skills, instructions, phone
	 * scripts) resolves in both products. Never look the plugin up by a literal name: that breaks the standalone.
	 */
	inline TSharedPtr<IPlugin> Owner()
	{
		return IPluginManager::Get().FindPlugin(TEXT(UE_PLUGIN_NAME));
	}
}
