// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "ClimbingSystemEditor.h"

#include "ACFSurfaceEditorStyle.h"

DEFINE_LOG_CATEGORY(LogACFFreeClimbEditor);

#define LOCTEXT_NAMESPACE "FClimbingSystemEditorModule"

void FClimbingSystemEditorModule::StartupModule()
{
    FACFSurfaceEditorStyle::Initialize();
}

void FClimbingSystemEditorModule::ShutdownModule()
{
    FACFSurfaceEditorStyle::Shutdown();
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FClimbingSystemEditorModule, ClimbingSystemEditor)
