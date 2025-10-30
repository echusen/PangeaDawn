// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "AscentDialogueSystemEditor.h"
#include "ADSGraphNodeDetailCustomization.h"
#include "Graph/ADSGraphNode.h"
#include "PropertyEditorModule.h"
#include "ADSDialogueDeveloperSettings.h"
#include "ADSVoiceConfigDataAsset.h"

#define LOCTEXT_NAMESPACE "FAscentDialogueSystemEditorModule"

void FAscentDialogueSystemEditorModule::StartupModule()
{
    // Register custom detail panel for ADSGraphNode
    FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

    PropertyModule.RegisterCustomClassLayout(
        UADSGraphNode::StaticClass()->GetFName(),
        FOnGetDetailCustomizationInstance::CreateStatic(&FADSGraphNodeDetailCustomization::MakeInstance));
    PropertyModule.RegisterCustomClassLayout(
        UADSVoiceConfigDataAsset::StaticClass()->GetFName(),
        FOnGetDetailCustomizationInstance::CreateStatic(&FADSAIVoiceGeneratorComponentDetails::MakeInstance));
}

void FAscentDialogueSystemEditorModule::ShutdownModule()
{
    // Unregister customizations
    if (FModuleManager::Get().IsModuleLoaded("PropertyEditor")) {
        FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
        PropertyModule.UnregisterCustomClassLayout(UADSGraphNode::StaticClass()->GetFName());
        PropertyModule.UnregisterCustomClassLayout(UADSVoiceConfigDataAsset::StaticClass()->GetFName());
    }

}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FAscentDialogueSystemEditorModule, AscentDialogueSystemEditor)