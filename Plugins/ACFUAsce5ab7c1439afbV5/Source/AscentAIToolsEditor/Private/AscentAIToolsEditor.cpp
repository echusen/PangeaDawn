// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "AscentAIToolsEditor.h"

#include "ACFAbilityToolset.h"
#include "ACFCharacterToolset.h"
#include "ACFDataAssetToolset.h"
#include "ACFItemToolset.h"
#include "ToolsetRegistry/UToolsetRegistry.h"

void FAscentAIToolsEditorModule::StartupModule()
{
	UToolsetRegistry::RegisterToolsetClass(UACFItemToolset::StaticClass());
	UToolsetRegistry::RegisterToolsetClass(UACFCharacterToolset::StaticClass());
	UToolsetRegistry::RegisterToolsetClass(UACFAbilityToolset::StaticClass());
	UToolsetRegistry::RegisterToolsetClass(UACFDataAssetToolset::StaticClass());
}

void FAscentAIToolsEditorModule::ShutdownModule()
{
	UToolsetRegistry::UnregisterToolsetClass(UACFDataAssetToolset::StaticClass());
	UToolsetRegistry::UnregisterToolsetClass(UACFAbilityToolset::StaticClass());
	UToolsetRegistry::UnregisterToolsetClass(UACFCharacterToolset::StaticClass());
	UToolsetRegistry::UnregisterToolsetClass(UACFItemToolset::StaticClass());
}

IMPLEMENT_MODULE(FAscentAIToolsEditorModule, AscentAIToolsEditor)
