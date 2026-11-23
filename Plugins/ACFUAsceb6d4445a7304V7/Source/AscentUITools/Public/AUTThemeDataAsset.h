// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "AUTTypes.h"
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include <GameplayTagContainer.h>

#include "AUTThemeDataAsset.generated.h"

/**
 * A UI skin: one entry per style role, each identified by a tag from the UIStyle hierarchy.
 * Widgets never index into these arrays, they ask for a style by tag (see UAUTUIFunctionLibrary),
 * so a theme is free to reorder or omit styles.
 */
UCLASS()
class ASCENTUITOOLS_API UAUTThemeDataAsset : public UPrimaryDataAsset {
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Style)
    FGameplayTag ThemeTag;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Style, meta = (TitleProperty = "StyleTag", Categories = "UIStyle.Text"))
    TArray<FAUTBaseTextStyle> TitlesStyle;

    /** Style of the buttons for this theme */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Style, meta = (TitleProperty = "StyleTag", Categories = "UIStyle.Button"))
    TArray<FAUTButtonStyle> ButtonsStyle;

    /** Style of the combo boxes for this theme */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Style, meta = (TitleProperty = "StyleTag", Categories = "UIStyle.ComboBox"))
    TArray<FAUTComboBoxStyle> ComboBoxesStyle;

    /** Style of the spinners for this theme */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Style, meta = (TitleProperty = "StyleTag", Categories = "UIStyle.Spinner"))
    TArray<FAUTSpinnerStyle> SpinnersStyle;

    /** Style of the sliders for this theme */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Style, meta = (TitleProperty = "StyleTag", Categories = "UIStyle.Slider"))
    TArray<FAUTSliderStyle> SlidersStyle;

    /** Style of the resource bars (health, stamina, mana, XP) for this theme */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Style, meta = (TitleProperty = "StyleTag", Categories = "UIStyle.Bar"))
    TArray<FAUTBarStyle> BarsStyle;

    /** Style of the checkboxes for this theme */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Style, meta = (TitleProperty = "StyleTag", Categories = "UIStyle.CheckBox"))
    TArray<FAUTCheckBoxStyle> CheckBoxesStyle;

    /**
     * Brushes to draw as the background of panels, windows and slots, plus the named brushes of the
     * UIStyle.Brush vocabulary (cards, slots, icons, dividers) that widgets ask for by tag.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Style, meta = (TitleProperty = "StyleTag", Categories = "UIStyle.Background,UIStyle.Brush"))
    TArray<FAUTBrushStyle> Backgrounds;

    /** Brushes used by the separators */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Style, meta = (TitleProperty = "StyleTag", Categories = "UIStyle.Spacer"))
    TArray<FAUTBrushStyle> Spacers;

    UFUNCTION(BlueprintPure, Category = ACF)
    bool TryGetTitleStyle(UPARAM(meta = (Categories = "UIStyle.Text")) FGameplayTag StyleTag, FAUTBaseTextStyle& OutStyle) const;

    UFUNCTION(BlueprintPure, Category = ACF)
    bool TryGetButtonStyle(UPARAM(meta = (Categories = "UIStyle.Button")) FGameplayTag StyleTag, FAUTButtonStyle& OutStyle) const;

    UFUNCTION(BlueprintPure, Category = ACF)
    bool TryGetComboBoxStyle(UPARAM(meta = (Categories = "UIStyle.ComboBox")) FGameplayTag StyleTag, FAUTComboBoxStyle& OutStyle) const;

    UFUNCTION(BlueprintPure, Category = ACF)
    bool TryGetSpinnerStyle(UPARAM(meta = (Categories = "UIStyle.Spinner")) FGameplayTag StyleTag, FAUTSpinnerStyle& OutStyle) const;

    UFUNCTION(BlueprintPure, Category = ACF)
    bool TryGetSliderStyle(UPARAM(meta = (Categories = "UIStyle.Slider")) FGameplayTag StyleTag, FAUTSliderStyle& OutStyle) const;

    UFUNCTION(BlueprintPure, Category = ACF)
    bool TryGetBarStyle(UPARAM(meta = (Categories = "UIStyle.Bar")) FGameplayTag StyleTag, FAUTBarStyle& OutStyle) const;

    UFUNCTION(BlueprintPure, Category = ACF)
    bool TryGetCheckBoxStyle(UPARAM(meta = (Categories = "UIStyle.CheckBox")) FGameplayTag StyleTag, FAUTCheckBoxStyle& OutStyle) const;

    UFUNCTION(BlueprintPure, Category = ACF)
    bool TryGetBackgroundStyle(UPARAM(meta = (Categories = "UIStyle.Background,UIStyle.Brush")) FGameplayTag StyleTag, FSlateBrush& OutStyle) const;

    UFUNCTION(BlueprintPure, Category = ACF)
    bool TryGetSpacerStyle(UPARAM(meta = (Categories = "UIStyle.Spacer")) FGameplayTag StyleTag, FSlateBrush& OutStyle) const;

    /**
     * Finds the style matching StyleTag exactly.
     * An unset tag means "no style from the theme", the same way an out of range index used to, so the
     * widget keeps whatever look it was authored with.
     */
    template <typename StyleType>
    static const StyleType* FindStyleByTag(const TArray<StyleType>& Styles, const FGameplayTag& StyleTag)
    {
        if (!StyleTag.IsValid()) {
            return nullptr;
        }

        return Styles.FindByPredicate([&StyleTag](const StyleType& Style) {
            return Style.StyleTag.MatchesTagExact(StyleTag);
        });
    }

    /**
     * Finds the style matching StyleTag, falling back to FallbackTag when the theme has no entry for it.
     * Themes authored before a tag existed would otherwise hand the widget a default constructed style,
     * which reads as a blank button or an invisible icon; a plainer style is always the better answer.
     */
    template <typename StyleType>
    static const StyleType* FindStyleByTagOrFallback(
        const TArray<StyleType>& Styles, const FGameplayTag& StyleTag, const FGameplayTag& FallbackTag)
    {
        if (const StyleType* FoundStyle = FindStyleByTag(Styles, StyleTag)) {
            return FoundStyle;
        }

        // An unset tag still means "leave this widget alone", so only a real miss falls back.
        return StyleTag.IsValid() ? FindStyleByTag(Styles, FallbackTag) : nullptr;
    }

    /**
     * Tag the given index used to point at when widgets referenced the styles by index.
     * Only meant to migrate assets authored before the switch to tags.
     */
    static FGameplayTag GetLegacyTitleStyleTag(int32 LegacyStyleIndex);
    static FGameplayTag GetLegacyButtonStyleTag(int32 LegacyStyleIndex);
    static FGameplayTag GetLegacyBackgroundStyleTag(int32 LegacyStyleIndex);
    static FGameplayTag GetLegacySpacerStyleTag(int32 LegacyStyleIndex);

protected:
    virtual void PostLoad() override;

private:
    /** Tags the entries authored before styles moved from indices to tags, keeping their original order. */
    void MigrateLegacyIndexedStyles();

    /** Backgrounds of the themes authored before brushes became FAUTBrushStyle. Migrated by PostLoad. */
    UPROPERTY()
    TArray<FSlateBrush> Background;

    /** Spacers of the themes authored before brushes became FAUTBrushStyle. Migrated by PostLoad. */
    UPROPERTY()
    TArray<FSlateBrush> SpacerStyle;
};
