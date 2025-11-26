// Copyright Epic Games, Inc. All Rights Reserved.

#include "AscentEditorStyle.h"
#include "AscentEditor.h"
#include "Framework/Application/SlateApplication.h"
#include "Styling/SlateStyleRegistry.h"
#include "Slate/SlateGameResources.h"
#include "Interfaces/IPluginManager.h"
#include "Styling/SlateStyleMacros.h"

#define RootToContentDir Style->RootToContentDir

TSharedPtr<FSlateStyleSet> FAscentEditorStyle::StyleInstance = nullptr;


void FAscentEditorStyle::Initialize()
{
	if (!StyleInstance.IsValid())
	{
		StyleInstance = Create();
		FSlateStyleRegistry::RegisterSlateStyle(*StyleInstance);
	}
}

void FAscentEditorStyle::Shutdown()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(*StyleInstance);
	ensure(StyleInstance.IsUnique());
	StyleInstance.Reset();
}

FName FAscentEditorStyle::GetStyleSetName()
{
	return StyleSetName;
}

const FVector2D Icon32x32(32.0f, 32.0f);
const FVector2D Icon20x20(20.0f, 20.0f);

TSharedRef< FSlateStyleSet > FAscentEditorStyle::Create()
{
	TSharedRef< FSlateStyleSet > Style = MakeShareable(new FSlateStyleSet(StyleSetName));
	Style->SetContentRoot(IPluginManager::Get().FindPlugin(PluginName)->GetBaseDir() / TEXT("Resources"));

	Style->Set("AscentEditor.PluginAction", new IMAGE_BRUSH(TEXT("ACF32"), Icon32x32));
	Style->Set("AscentEditor.SmallIcon", new IMAGE_BRUSH(TEXT("ACF32"), Icon20x20));

	return Style;
}

void FAscentEditorStyle::ReloadTextures()
{
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().GetRenderer()->ReloadTextureResources();
	}
}

const ISlateStyle& FAscentEditorStyle::Get()
{
	return *StyleInstance;
}
