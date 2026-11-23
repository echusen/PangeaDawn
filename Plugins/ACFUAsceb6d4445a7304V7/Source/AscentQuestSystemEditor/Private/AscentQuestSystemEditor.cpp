// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "AscentQuestSystemEditor.h"
#include "AQSQuestToolset.h"
#include "ToolsetRegistry/UToolsetRegistry.h"

#define LOCTEXT_NAMESPACE "FAscentQuestSystemModule"

void FAscentQuestSystemEditorModule::StartupModule()
{
	UToolsetRegistry::RegisterToolsetClass(UAQSQuestToolset::StaticClass());
}

void FAscentQuestSystemEditorModule::ShutdownModule()
{
	UToolsetRegistry::UnregisterToolsetClass(UAQSQuestToolset::StaticClass());
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FAscentQuestSystemEditorModule, AscentQuestSystemEditor)
