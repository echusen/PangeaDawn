// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "Styling/SlateTypes.h"
#include "UObject/NoExportTypes.h"
#include "UObject/ObjectPtr.h"

#include "AUTTypes.generated.h"

class USoundCue;

/**
 *
 */
USTRUCT(BlueprintType)
struct FAUTBaseStyle {

    GENERATED_BODY()

public:
    /**
     * Identifies this style inside its theme: every widget asks the theme for the style with this tag.
     * Tags must be unique per style array, and the same tag is expected to exist in every theme, so that
     * switching theme reskins the whole UI without touching a single widget.
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, config, Category = Style)
    FGameplayTag StyleTag;
};

/** A themed brush (backgrounds, spacers), identified by its style tag. */
USTRUCT(BlueprintType)
struct FAUTBrushStyle : public FAUTBaseStyle {

    GENERATED_BODY()

public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, config, Category = Style)
    FSlateBrush Brush;
};

USTRUCT(BlueprintType, meta = (Categories = "UIStyle.Text"))
struct FAUTBaseTextStyle : public FAUTBaseStyle {

    GENERATED_BODY()

public:
    FAUTBaseTextStyle()
    {
        NormalColor = FSlateColor(FColor::Black);
    }

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, config, Category = Style)
    FSlateFontInfo Normal;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, config, Category = Style)
    FSlateColor NormalColor;
};

USTRUCT(BlueprintType, meta = (Categories = "UIStyle.Text"))
struct FAUTTextStyle : public FAUTBaseTextStyle {

    GENERATED_BODY()

public:
    FAUTTextStyle()
    {
        NormalColor = FSlateColor(FColor::Black);
        HoveredColor = FSlateColor(FColor::Black);
    }

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, config, Category = Style)
    FSlateFontInfo Hovered;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, config, Category = Style)
    FSlateColor HoveredColor;
};

/** Optional, non-interactive decoration owned by a button's theme style. */
USTRUCT(BlueprintType)
struct FAUTButtonOrnament {
    GENERATED_BODY()

    /** Opt-in: existing themes and buttons keep their original appearance. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Ornament)
    bool bEnabled = false;

    /** Keep its layout space while unfocused, avoiding text/ornament movement. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Ornament, meta = (EditCondition = "bEnabled"))
    bool bOnlyWhenFocused = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Ornament, meta = (EditCondition = "bEnabled"))
    FSlateBrush Brush;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Ornament, meta = (EditCondition = "bEnabled"))
    FMargin Padding = FMargin(0.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Ornament, meta = (EditCondition = "bEnabled"))
    bool bMirrorHorizontally = false;
};

USTRUCT(BlueprintType, meta = (Categories = "UIStyle.Button"))
struct FAUTButtonStyle : public FAUTBaseStyle {
    GENERATED_BODY()

public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, config, Category = Style)
    FButtonStyle ButtonStyle;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, config, Category = Style)
    FAUTTextStyle FontStyle;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, config, Category = "Style|Ornaments")
    FAUTButtonOrnament LeftOrnament;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, config, Category = "Style|Ornaments")
    FAUTButtonOrnament RightOrnament;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, config, Category = "Style|Ornaments")
    FAUTButtonOrnament BottomOrnament;
};

/** Optional decoration behind an entire control row. Disabled for existing themes. */
USTRUCT(BlueprintType)
struct FAUTControlFrame {
    GENERATED_BODY()

    FAUTControlFrame()
    {
        NormalBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
        FocusedBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
    }

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frame")
    bool bEnabled = false;

    /** Brush stretched over the row, without adding padding or changing hit testing. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frame", meta = (EditCondition = "bEnabled"))
    FSlateBrush NormalBrush;

    /** Used for mouse hover, gamepad focus and selected rows. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frame", meta = (EditCondition = "bEnabled"))
    FSlateBrush FocusedBrush;
};

USTRUCT(BlueprintType)
struct FAUTHoverableStyle : public FAUTBaseStyle {
    GENERATED_BODY()

public:
    FAUTHoverableStyle()
        : PressedSound(nullptr)
        , HoveredSound(nullptr)
    {
    }

    /** Opt-in row frame shared by slider, checkbox, spinner and combo-box styles. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, config, Category = "Style|Frame")
    FAUTControlFrame ControlFrame;

              UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, config, Category = Style)
                  FAUTTextStyle FontStyle;

    /**
     * The sound the button should play when pressed
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Sound, meta = (DisplayName = "Pressed Sound"))
    USoundCue* PressedSound;

    /**
     * The sound the button should play when initially hovered over
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Sound, meta = (DisplayName = "Hovered Sound"))
    USoundCue* HoveredSound;
};

USTRUCT(BlueprintType, meta = (Categories = "UIStyle.Slider"))
struct FAUTSliderStyle : public FAUTHoverableStyle {
    GENERATED_BODY()

public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, config, Category = Style)
    FSliderStyle SliderStyle;
};

/**
 * A horizontal resource bar (health, stamina, mana, XP): background and fill from BarStyle,
 * an optional frame drawn over them, and the damage trail that lags behind the fill.
 * Widgets ask the theme for one by tag, so reskinning a bar never touches the widget.
 */
USTRUCT(BlueprintType, meta = (Categories = "UIStyle.Bar"))
struct FAUTBarStyle : public FAUTBaseStyle {
    GENERATED_BODY()

public:
    /** Empty part and filled part of the bar. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, config, Category = Style)
    FProgressBarStyle BarStyle;

    /** Drawn on top of the bar. Leave it as NoDrawType for a frameless bar. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, config, Category = Style)
    FSlateBrush FrameBrush;

    /** Inset of background and fill inside the frame. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, config, Category = Style)
    FMargin FillPadding = FMargin(9.0f, 4.0f);
};

USTRUCT(BlueprintType, meta = (Categories = "UIStyle.ComboBox"))
struct FAUTComboBoxStyle : public FAUTHoverableStyle {
    GENERATED_BODY()

public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, config, Category = Style)
    FComboBoxStyle ComboBoxStyle;
};

USTRUCT(BlueprintType, meta = (Categories = "UIStyle.Spinner"))
struct FAUTSpinnerStyle : public FAUTHoverableStyle {
    GENERATED_BODY()

};

USTRUCT(BlueprintType, meta = (Categories = "UIStyle.CheckBox"))
struct FAUTCheckBoxStyle : public FAUTHoverableStyle {
    GENERATED_BODY()

public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, config, Category = Style)
    FCheckBoxStyle CheckBoxStyle;
};

USTRUCT(BlueprintType)
struct FAUTThemeConfig {
    GENERATED_BODY()

public:
    FAUTThemeConfig()
    {
        Theme = FGameplayTag();
    }

    UPROPERTY(EditAnywhere, BlueprintReadWrite, config, Category = Style)
    FGameplayTag Theme;
};

USTRUCT(BlueprintType)
struct FSliderConfig {
    GENERATED_BODY()

    FSliderConfig()
    {
        MinValue = 0.f;
        MaxValue = 1.f;
        DefaultValue = 1.f;
        StepSize = .1f;
    }

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, config, Category = AUT)
    float MinValue;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, config, Category = AUT)
    float MaxValue;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, config, Category = AUT)
    float DefaultValue;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, config, Category = AUT)
    float StepSize;
};

UCLASS()
class ASCENTUITOOLS_API UAUTTypes : public UObject {
    GENERATED_BODY()
};
