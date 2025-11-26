// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2025. All Rights Reserved.

#include "AscentAttributesEditor.h"
#include "Logging.h"

#include "Modules/ModuleManager.h"
#include "ACFGameplayAttributeGraphPin.h"
#include <EdGraphUtilities.h>

#define LOCTEXT_NAMESPACE "FAscentAttributesEditor"



void FAscentAttributesEditor::StartupModule()
{
	ACFFactory = MakeShared<FACFGameplayAttributeGraphPanelPinFactory>();

	FEdGraphUtilities::RegisterVisualPinFactory(ACFFactory);
}

void FAscentAttributesEditor::ShutdownModule()
{

	if (ACFFactory.IsValid())
	{
		FEdGraphUtilities::UnregisterVisualPinFactory(ACFFactory);
		ACFFactory.Reset();
	}

}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FAscentAttributesEditor, AscentAttributesEditor);