// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "AUTUIFunctionLibrary.h"
#include "AUTUIStyleTags.h"
#include "AUTDeveloperSettings.h"
#include "AUTGameUserSettings.h"
#include "AUTThemeDataAsset.h"
#include "AUTTypes.h"
#include "Engine/Engine.h"
#include "Blueprint/UserWidget.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/OverlaySlot.h"

void UAUTUIFunctionLibrary::ApplyButtonOrnament(UImage* Image, const FAUTButtonOrnament& Ornament, bool bFocused)
{
    if (!IsValid(Image)) {
        return;
    }

    if (!Ornament.bEnabled) {
        Image->SetVisibility(ESlateVisibility::Collapsed);
        return;
    }

    Image->SetBrush(Ornament.Brush);

    // Ornament geometry comes exclusively from the theme, never from the label or cached layout.
    Image->SetDesiredSizeOverride(FVector2D(Ornament.Brush.ImageSize.X, Ornament.Brush.ImageSize.Y));
    Image->SetPixelSnapping(EWidgetPixelSnapping::Inherit);
    Image->SetRenderScale(FVector2D(Ornament.bMirrorHorizontally ? -1.0f : 1.0f, 1.0f));
    if (UHorizontalBoxSlot* HorizontalSlot = Cast<UHorizontalBoxSlot>(Image->Slot)) {
        HorizontalSlot->SetPadding(Ornament.Padding);
    } else if (UVerticalBoxSlot* VerticalSlot = Cast<UVerticalBoxSlot>(Image->Slot)) {
        VerticalSlot->SetPadding(Ornament.Padding);
    } else if (UOverlaySlot* OverlaySlot = Cast<UOverlaySlot>(Image->Slot)) {
        OverlaySlot->SetPadding(Ornament.Padding);
    }

    // Hidden reserves space; HitTestInvisible never intercepts CommonUI pointer input.
    Image->SetVisibility(Ornament.bOnlyWhenFocused && !bFocused
        ? ESlateVisibility::Hidden : ESlateVisibility::HitTestInvisible);
}

UAUTGameUserSettings* UAUTUIFunctionLibrary::GetGameUserSettings()
{
    return Cast<UAUTGameUserSettings>(GEngine->GetGameUserSettings());
}

UAUTThemeDataAsset* UAUTUIFunctionLibrary::GetCurrentTheme()
{
    const UAUTDeveloperSettings* settings = GetDefault<UAUTDeveloperSettings>();
    return settings ? settings->GetTheme() : nullptr;
}

bool UAUTUIFunctionLibrary::TryGetButtonStyle(FGameplayTag styleTag, FAUTButtonStyle& outStyle)
{
    const UAUTThemeDataAsset* theme = GetCurrentTheme();
    return theme && theme->TryGetButtonStyle(styleTag, outStyle);
}

bool UAUTUIFunctionLibrary::TryGetSliderStyle(FGameplayTag styleTag, FAUTSliderStyle& outStyle)
{
    const UAUTThemeDataAsset* theme = GetCurrentTheme();
    return theme && theme->TryGetSliderStyle(styleTag, outStyle);
}

bool UAUTUIFunctionLibrary::TryGetComboBoxStyle(FGameplayTag styleTag, FAUTComboBoxStyle& outStyle)
{
    const UAUTThemeDataAsset* theme = GetCurrentTheme();
    return theme && theme->TryGetComboBoxStyle(styleTag, outStyle);
}

bool UAUTUIFunctionLibrary::TryGetSpinnerStyle(FGameplayTag styleTag, FAUTSpinnerStyle& outStyle)
{
    const UAUTThemeDataAsset* theme = GetCurrentTheme();
    return theme && theme->TryGetSpinnerStyle(styleTag, outStyle);
}

void UAUTUIFunctionLibrary::SetProgressBarFillBrush(UProgressBar* ProgressBar, const FSlateBrush& FillBrush)
{
    if (!IsValid(ProgressBar)) {
        return;
    }

    FProgressBarStyle Style = ProgressBar->GetWidgetStyle();
    Style.FillImage = FillBrush;
    ProgressBar->SetWidgetStyle(Style);
}

void UAUTUIFunctionLibrary::SetProgressBarBackgroundBrush(UProgressBar* ProgressBar, const FSlateBrush& BackgroundBrush)
{
    if (!IsValid(ProgressBar)) {
        return;
    }

    FProgressBarStyle Style = ProgressBar->GetWidgetStyle();
    Style.BackgroundImage = BackgroundBrush;
    ProgressBar->SetWidgetStyle(Style);
}

bool UAUTUIFunctionLibrary::TryGetBarStyle(FGameplayTag styleTag, FAUTBarStyle& outStyle)
{
    const UAUTThemeDataAsset* theme = GetCurrentTheme();
    return theme && theme->TryGetBarStyle(styleTag, outStyle);
}

bool UAUTUIFunctionLibrary::GetDefaultBarStyle(FAUTBarStyle& outStyle)
{
    return TryGetBarStyle(FAUTStyleTag::UIStyle_Bar_Default, outStyle);
}

bool UAUTUIFunctionLibrary::TryGetCheckBoxStyle(FGameplayTag styleTag, FAUTCheckBoxStyle& outStyle)
{
    const UAUTThemeDataAsset* theme = GetCurrentTheme();
    return theme && theme->TryGetCheckBoxStyle(styleTag, outStyle);
}

bool UAUTUIFunctionLibrary::TryGetTitleStyle(FGameplayTag styleTag, FAUTBaseTextStyle& outStyle)
{
    const UAUTThemeDataAsset* theme = GetCurrentTheme();
    return theme && theme->TryGetTitleStyle(styleTag, outStyle);
}

bool UAUTUIFunctionLibrary::TryGetBackgroundStyle(FGameplayTag styleTag, FSlateBrush& outStyle)
{
    const UAUTThemeDataAsset* theme = GetCurrentTheme();
    return theme && theme->TryGetBackgroundStyle(styleTag, outStyle);
}

bool UAUTUIFunctionLibrary::TryGetSpacerStyle(FGameplayTag styleTag, FSlateBrush& outStyle)
{
    const UAUTThemeDataAsset* theme = GetCurrentTheme();
    return theme && theme->TryGetSpacerStyle(styleTag, outStyle);
}

TSoftObjectPtr<UWorld> UAUTUIFunctionLibrary::GetDefaultMenuLevel()
{
    const UAUTDeveloperSettings* settings = GetMutableDefault<UAUTDeveloperSettings>();
    return settings->GetDefaultMenuMapRef();
}



TSoftObjectPtr<UWorld> UAUTUIFunctionLibrary::GetDefaultNewGameLevel()
{
    const UAUTDeveloperSettings* settings = GetMutableDefault<UAUTDeveloperSettings>();
    return settings->GetDefaultNewGameMapRef();
}

TArray<TSoftObjectPtr<USoundClass>> UAUTUIFunctionLibrary::GetDefaultSoundClasses()
{
    const UAUTDeveloperSettings* settings = GetMutableDefault<UAUTDeveloperSettings>();
    TArray<TSoftObjectPtr<USoundClass>> outSounds;
    const TArray<FSoftObjectPath> softPtrs = settings->GetDefaultSoundClasses();

    for (FSoftObjectPath soundclass : softPtrs) {
        const TSoftObjectPtr<USoundClass> defaultClass = Cast<USoundClass>( soundclass.TryLoad());
        outSounds.Add(defaultClass);
    }
    return outSounds;
}

void UAUTUIFunctionLibrary::SetSoundClassVolume(USoundClass* TargetClass, float NewVolume)
{
    if (!TargetClass) {
        return;
    }
    TargetClass->Properties.Volume = NewVolume;
}

bool UAUTUIFunctionLibrary::IsValidAndOnScreen(UUserWidget* widget)
{
	if (!IsValid(widget))
    {
        return false;
    }
    
    return widget->IsInViewport();
}
