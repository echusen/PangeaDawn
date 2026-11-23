// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "ACFSurfaceAssetFactory.h"

#include "ACFSurfaceAsset.h"

UACFSurfaceAssetFactory::UACFSurfaceAssetFactory()
{
    SupportedClass = UACFSurfaceAsset::StaticClass();
    bCreateNew = true;
    bEditAfterNew = true;
}

UObject* UACFSurfaceAssetFactory::FactoryCreateNew(
    UClass* Class,
    UObject* InParent,
    FName Name,
    EObjectFlags Flags,
    UObject* Context,
    FFeedbackContext* Warn)
{
    return NewObject<UACFSurfaceAsset>(
        InParent,
        Class,
        Name,
        Flags);
}
