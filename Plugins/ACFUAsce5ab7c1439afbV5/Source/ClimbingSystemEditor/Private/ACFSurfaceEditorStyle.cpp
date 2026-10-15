// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "ACFSurfaceEditorStyle.h"

#include "Brushes/SlateImageBrush.h"
#include "Interfaces/IPluginManager.h"
#include "Styling/SlateStyleRegistry.h"

TSharedPtr<FSlateStyleSet> FACFSurfaceEditorStyle::StyleSet;

void FACFSurfaceEditorStyle::Initialize()
{
    StyleSet = MakeShared<FSlateStyleSet>("ACFSurfaceEditorStyle");

    StyleSet->SetContentRoot(
        IPluginManager::Get()
            .FindPlugin(TEXT("AscentCombatFramework"))
            ->GetBaseDir() / TEXT("Resources"));

    StyleSet->Set(
        "ACFSurface.AssetThumbnail",
        new FSlateImageBrush(
            StyleSet->RootToContentDir(TEXT("ACFSurfaceIcon128"), TEXT(".png")),
            FVector2D(128.0f, 128.0f)));

    FSlateStyleRegistry::RegisterSlateStyle(*StyleSet);
}

void FACFSurfaceEditorStyle::Shutdown()
{
    if (!StyleSet.IsValid()) return;

    FSlateStyleRegistry::UnRegisterSlateStyle(*StyleSet);
    StyleSet.Reset();
}

const ISlateStyle& FACFSurfaceEditorStyle::Get()
{
    return *StyleSet;
}

FName FACFSurfaceEditorStyle::GetStyleSetName()
{
    static FName StyleSetName(TEXT("ACFSurfaceEditorStyle"));
    return StyleSetName;
}
