// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "AscentFirstPerson.h"

#include "ACFFirstPersonLog.h"
#include "Debug/ACFProceduralMotionDebug.h"

#define LOCTEXT_NAMESPACE "FAscentFirstPersonModule"

DEFINE_LOG_CATEGORY(LogACFFirstPerson);

void FAscentFirstPersonModule::StartupModule()
{
#if ACF_FP_DEBUG_CAPTURE
    FACFProceduralMotionDebugRegistry::Register();
#endif
}

void FAscentFirstPersonModule::ShutdownModule()
{
#if ACF_FP_DEBUG_CAPTURE
    FACFProceduralMotionDebugRegistry::Unregister();
#endif
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FAscentFirstPersonModule, AscentFirstPerson)
