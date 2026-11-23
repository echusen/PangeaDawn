// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "AUTThemeDataAsset.h"
#include "AUTUIStyleTags.h"

/**
 * Order the styles had when widgets referenced them by index. Used only to tag themes authored before
 * the switch to tags: the Nth entry of a legacy theme gets the Nth tag of the matching list.
 */
static TArray<FGameplayTag> GetLegacyTitleTags()
{
    return { FAUTStyleTag::UIStyle_Text_Default, FAUTStyleTag::UIStyle_Text_Compass,
        FAUTStyleTag::UIStyle_Text_Title, FAUTStyleTag::UIStyle_Text_SemiTransparent,
        FAUTStyleTag::UIStyle_Text_Outline, FAUTStyleTag::UIStyle_Text_Monospaced,
        FAUTStyleTag::UIStyle_Text_Highlight, FAUTStyleTag::UIStyle_Text_BigOutline,
        FAUTStyleTag::UIStyle_Text_Accent, FAUTStyleTag::UIStyle_Text_Desaturated };
}

static TArray<FGameplayTag> GetLegacyButtonTags()
{
    return { FAUTStyleTag::UIStyle_Button_NoBackground, FAUTStyleTag::UIStyle_Button_Default,
        FAUTStyleTag::UIStyle_Button_ItemIcon, FAUTStyleTag::UIStyle_Button_Framed,
        FAUTStyleTag::UIStyle_Button_Secondary, FAUTStyleTag::UIStyle_Button_Flat,
        FAUTStyleTag::UIStyle_Button_MainMenu, FAUTStyleTag::UIStyle_Button_PauseMenu,
        FAUTStyleTag::UIStyle_Button_Ornate };
}

static TArray<FGameplayTag> GetLegacyBackgroundTags()
{
    return { FAUTStyleTag::UIStyle_Background_Panel, FAUTStyleTag::UIStyle_Background_PanelClean,
        FAUTStyleTag::UIStyle_Background_Outline, FAUTStyleTag::UIStyle_Background_Window,
        FAUTStyleTag::UIStyle_Background_WindowAccent, FAUTStyleTag::UIStyle_Background_Fullscreen,
        FAUTStyleTag::UIStyle_Background_Tooltip, FAUTStyleTag::UIStyle_Background_ItemSlot,
        FAUTStyleTag::UIStyle_Background_Soft };
}

static TArray<FGameplayTag> GetLegacySpacerTags()
{
    return { FAUTStyleTag::UIStyle_Spacer_Default, FAUTStyleTag::UIStyle_Spacer_Half };
}

FGameplayTag UAUTThemeDataAsset::GetLegacyTitleStyleTag(int32 LegacyStyleIndex)
{
    const TArray<FGameplayTag> LegacyTags = GetLegacyTitleTags();
    return LegacyTags.IsValidIndex(LegacyStyleIndex) ? LegacyTags[LegacyStyleIndex] : FGameplayTag();
}

FGameplayTag UAUTThemeDataAsset::GetLegacyButtonStyleTag(int32 LegacyStyleIndex)
{
    const TArray<FGameplayTag> LegacyTags = GetLegacyButtonTags();
    return LegacyTags.IsValidIndex(LegacyStyleIndex) ? LegacyTags[LegacyStyleIndex] : FGameplayTag();
}

FGameplayTag UAUTThemeDataAsset::GetLegacyBackgroundStyleTag(int32 LegacyStyleIndex)
{
    const TArray<FGameplayTag> LegacyTags = GetLegacyBackgroundTags();
    return LegacyTags.IsValidIndex(LegacyStyleIndex) ? LegacyTags[LegacyStyleIndex] : FGameplayTag();
}

FGameplayTag UAUTThemeDataAsset::GetLegacySpacerStyleTag(int32 LegacyStyleIndex)
{
    const TArray<FGameplayTag> LegacyTags = GetLegacySpacerTags();
    return LegacyTags.IsValidIndex(LegacyStyleIndex) ? LegacyTags[LegacyStyleIndex] : FGameplayTag();
}

/** Fills the still untagged entries of Styles with the tag their index used to mean. */
template <typename StyleType>
static void AssignLegacyStyleTags(TArray<StyleType>& Styles, const TArray<FGameplayTag>& LegacyOrder)
{
    for (int32 StyleIndex = 0; StyleIndex < Styles.Num(); ++StyleIndex) {
        if (!Styles[StyleIndex].StyleTag.IsValid() && LegacyOrder.IsValidIndex(StyleIndex)) {
            Styles[StyleIndex].StyleTag = LegacyOrder[StyleIndex];
        }
    }
}

void UAUTThemeDataAsset::PostLoad()
{
    Super::PostLoad();

    MigrateLegacyIndexedStyles();
}

void UAUTThemeDataAsset::MigrateLegacyIndexedStyles()
{
    AssignLegacyStyleTags(TitlesStyle, GetLegacyTitleTags());
    AssignLegacyStyleTags(ButtonsStyle, GetLegacyButtonTags());
    AssignLegacyStyleTags(ComboBoxesStyle, { FAUTStyleTag::UIStyle_ComboBox_Default });
    AssignLegacyStyleTags(SpinnersStyle, { FAUTStyleTag::UIStyle_Spinner_Default });
    AssignLegacyStyleTags(SlidersStyle, { FAUTStyleTag::UIStyle_Slider_Default });
    AssignLegacyStyleTags(CheckBoxesStyle, { FAUTStyleTag::UIStyle_CheckBox_Default });

    // Brushes used to be bare FSlateBrush arrays, so they need to be moved into the tagged ones.
    if (Backgrounds.IsEmpty() && !Background.IsEmpty()) {
        Backgrounds.Reserve(Background.Num());
        for (const FSlateBrush& LegacyBrush : Background) {
            FAUTBrushStyle& BrushStyle = Backgrounds.AddDefaulted_GetRef();
            BrushStyle.Brush = LegacyBrush;
        }
    }

    if (Spacers.IsEmpty() && !SpacerStyle.IsEmpty()) {
        Spacers.Reserve(SpacerStyle.Num());
        for (const FSlateBrush& LegacyBrush : SpacerStyle) {
            FAUTBrushStyle& BrushStyle = Spacers.AddDefaulted_GetRef();
            BrushStyle.Brush = LegacyBrush;
        }
    }

    AssignLegacyStyleTags(Backgrounds, GetLegacyBackgroundTags());
    AssignLegacyStyleTags(Spacers, GetLegacySpacerTags());
}

bool UAUTThemeDataAsset::TryGetTitleStyle(FGameplayTag StyleTag, FAUTBaseTextStyle& OutStyle) const
{
    if (const FAUTBaseTextStyle* FoundStyle = FindStyleByTagOrFallback(TitlesStyle, StyleTag, FAUTStyleTag::UIStyle_Text_Default)) {
        OutStyle = *FoundStyle;
        return true;
    }
    return false;
}

bool UAUTThemeDataAsset::TryGetButtonStyle(FGameplayTag StyleTag, FAUTButtonStyle& OutStyle) const
{
    if (const FAUTButtonStyle* FoundStyle = FindStyleByTagOrFallback(ButtonsStyle, StyleTag, FAUTStyleTag::UIStyle_Button_Default)) {
        OutStyle = *FoundStyle;
        return true;
    }
    return false;
}

bool UAUTThemeDataAsset::TryGetComboBoxStyle(FGameplayTag StyleTag, FAUTComboBoxStyle& OutStyle) const
{
    if (const FAUTComboBoxStyle* FoundStyle = FindStyleByTagOrFallback(ComboBoxesStyle, StyleTag, FAUTStyleTag::UIStyle_ComboBox_Default)) {
        OutStyle = *FoundStyle;
        return true;
    }
    return false;
}

bool UAUTThemeDataAsset::TryGetSpinnerStyle(FGameplayTag StyleTag, FAUTSpinnerStyle& OutStyle) const
{
    if (const FAUTSpinnerStyle* FoundStyle = FindStyleByTagOrFallback(SpinnersStyle, StyleTag, FAUTStyleTag::UIStyle_Spinner_Default)) {
        OutStyle = *FoundStyle;
        return true;
    }
    return false;
}

bool UAUTThemeDataAsset::TryGetSliderStyle(FGameplayTag StyleTag, FAUTSliderStyle& OutStyle) const
{
    if (const FAUTSliderStyle* FoundStyle = FindStyleByTagOrFallback(SlidersStyle, StyleTag, FAUTStyleTag::UIStyle_Slider_Default)) {
        OutStyle = *FoundStyle;
        return true;
    }
    return false;
}

bool UAUTThemeDataAsset::TryGetCheckBoxStyle(FGameplayTag StyleTag, FAUTCheckBoxStyle& OutStyle) const
{
    if (const FAUTCheckBoxStyle* FoundStyle = FindStyleByTagOrFallback(CheckBoxesStyle, StyleTag, FAUTStyleTag::UIStyle_CheckBox_Default)) {
        OutStyle = *FoundStyle;
        return true;
    }
    return false;
}

bool UAUTThemeDataAsset::TryGetBarStyle(FGameplayTag StyleTag, FAUTBarStyle& OutStyle) const
{
    if (const FAUTBarStyle* FoundStyle = FindStyleByTagOrFallback(BarsStyle, StyleTag, FAUTStyleTag::UIStyle_Bar_Default)) {
        OutStyle = *FoundStyle;
        return true;
    }
    return false;
}

bool UAUTThemeDataAsset::TryGetBackgroundStyle(FGameplayTag StyleTag, FSlateBrush& OutStyle) const
{
    if (const FAUTBrushStyle* FoundStyle = FindStyleByTag(Backgrounds, StyleTag)) {
        OutStyle = FoundStyle->Brush;
        return true;
    }
    return false;
}

bool UAUTThemeDataAsset::TryGetSpacerStyle(FGameplayTag StyleTag, FSlateBrush& OutStyle) const
{
    if (const FAUTBrushStyle* FoundStyle = FindStyleByTag(Spacers, StyleTag)) {
        OutStyle = FoundStyle->Brush;
        return true;
    }
    return false;
}
