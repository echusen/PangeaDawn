// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "AscentAIAgentStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Styling/SlateStyleMacros.h"
#include "Framework/Application/SlateApplication.h"
#include "Interfaces/IPluginManager.h"
#include "Styling/SlateTypes.h"
#include "Styling/CoreStyle.h"

#define RootToContentDir Style->RootToContentDir

TSharedPtr<FSlateStyleSet> FAscentAIAgentStyle::StyleInstance = nullptr;

void FAscentAIAgentStyle::Initialize()
{
    if (!StyleInstance.IsValid())
    {
        StyleInstance = Create();
        FSlateStyleRegistry::RegisterSlateStyle(*StyleInstance);
    }
}

void FAscentAIAgentStyle::Shutdown()
{
    FSlateStyleRegistry::UnRegisterSlateStyle(*StyleInstance);
    ensure(StyleInstance.IsUnique());
    StyleInstance.Reset();
}

FName FAscentAIAgentStyle::GetStyleSetName()
{
    static FName StyleSetName(TEXT("AscentAIAgentStyle"));
    return StyleSetName;
}

void FAscentAIAgentStyle::ReloadTextures()
{
    if (FSlateApplication::IsInitialized())
    {
        FSlateApplication::Get().GetRenderer()->ReloadTextureResources();
    }
}

const ISlateStyle& FAscentAIAgentStyle::Get()
{
    return *StyleInstance;
}

TSharedRef<FSlateStyleSet> FAscentAIAgentStyle::Create()
{
    TSharedRef<FSlateStyleSet> Style = MakeShareable(new FSlateStyleSet("AscentAIAgentStyle"));

    TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT(UE_PLUGIN_NAME));
    if (Plugin.IsValid())
    {
        Style->SetContentRoot(Plugin->GetBaseDir() / TEXT("Resources"));
    }

    const FVector2D Icon40x40(40.0f, 40.0f);
    const FVector2D Icon20x20(20.0f, 20.0f);
    const FVector2D Icon16x16(16.0f, 16.0f);
    // YarinLong.png is 1672x941; keep that aspect so the wordmark is not stretched.
    const FVector2D YarinLongSize(256.0f, 144.0f);

    Style->Set("AscentAIAgent.TabIcon",           new IMAGE_BRUSH(TEXT("YarinIcon"), Icon16x16));
    Style->Set("AscentAIAgent.ToolbarIcon",        new IMAGE_BRUSH(TEXT("YarinIcon"), Icon40x40));
    Style->Set("AscentAIAgent.ToolbarIcon.Small",  new IMAGE_BRUSH(TEXT("YarinIcon"), Icon20x20));
    Style->Set("AscentAIAgent.OpenAIAgentTab",     new IMAGE_BRUSH(TEXT("YarinIcon"), Icon40x40));
    Style->Set("AscentAIAgent.YarinLong",          new IMAGE_BRUSH(TEXT("YarinLong"), YarinLongSize));

    return Style;
}

#undef RootToContentDir
