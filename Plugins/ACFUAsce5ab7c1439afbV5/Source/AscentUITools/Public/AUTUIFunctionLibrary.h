// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "AUTTypes.h"
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "AUTUIFunctionLibrary.generated.h"

class USoundClass;
class UUserWidget;
class UImage;



/**
 * Blueprint Function Library providing utility functions for the Ascent UI Tools system.
 * Exposes style retrieval, settings access, and audio configuration to Blueprints.
 */
UCLASS()
class ASCENTUITOOLS_API UAUTUIFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

    /** Applies one theme decoration. Does not change button input, sounds or focus.
     * Call after applying the button style, passing ANS's existing focus/selection state.
     * Supports horizontal, vertical and overlay slots; disabled decorations collapse.
     */
    UFUNCTION(BlueprintCallable, Category = "AUT|Styles")
    static void ApplyButtonOrnament(UImage* Image, const FAUTButtonOrnament& Ornament, bool bFocused);

	/**
	 * Retrieves the game user settings instance.
	 * @return The AUT game user settings singleton.
	 */
	UFUNCTION(BlueprintPure, Category = "AUT|Settings")
	static class UAUTGameUserSettings* GetGameUserSettings();

	/**
	 * Attempts to retrieve a button style from the current theme.
	 * @param StyleTag - Tag of the button style to retrieve.
	 * @param OutStyle - The retrieved button style if found.
	 * @return True if the style was found, false otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category = "AUT|Styles")
	static bool TryGetButtonStyle(UPARAM(meta = (Categories = "UIStyle.Button")) FGameplayTag StyleTag, FAUTButtonStyle& OutStyle);

	/**
	 * Attempts to retrieve a slider style from the current theme.
	 * @param StyleTag - Tag of the slider style to retrieve.
	 * @param OutStyle - The retrieved slider style if found.
	 * @return True if the style was found, false otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category = "AUT|Styles")
	static bool TryGetSliderStyle(UPARAM(meta = (Categories = "UIStyle.Slider")) FGameplayTag StyleTag, FAUTSliderStyle& OutStyle);

	/**
	 * Attempts to retrieve a spinner style from the current theme.
	 * @param StyleTag - Tag of the spinner style to retrieve.
	 * @param OutStyle - The retrieved spinner style if found.
	 * @return True if the style was found, false otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category = "AUT|Styles")
	static bool TryGetSpinnerStyle(UPARAM(meta = (Categories = "UIStyle.Spinner")) FGameplayTag StyleTag, FAUTSpinnerStyle& OutStyle);

	/**
	 * Attempts to retrieve a combo box style from the current theme.
	 * @param StyleTag - Tag of the combo box style to retrieve.
	 * @param OutStyle - The retrieved combo box style if found.
	 * @return True if the style was found, false otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category = "AUT|Styles")
	static bool TryGetComboBoxStyle(UPARAM(meta = (Categories = "UIStyle.ComboBox")) FGameplayTag StyleTag, FAUTComboBoxStyle& OutStyle);

	/**
	 * Replaces only the fill brush of a progress bar, keeping the rest of its style.
	 * Blueprint can only assign a whole FProgressBarStyle, so swapping one layer from a theme
	 * brush is not expressible there; the damage trail bar needs exactly this.
	 *
	 * @param ProgressBar - The bar to restyle. Does nothing when null.
	 * @param FillBrush - Brush to draw as the filled part.
	 */
	UFUNCTION(BlueprintCallable, Category = "AUT|Styles")
	static void SetProgressBarFillBrush(class UProgressBar* ProgressBar, const FSlateBrush& FillBrush);

	/**
	 * Replaces only the background brush of a progress bar, keeping the rest of its style.
	 *
	 * @param ProgressBar - The bar to restyle. Does nothing when null.
	 * @param BackgroundBrush - Brush to draw as the empty part.
	 */
	UFUNCTION(BlueprintCallable, Category = "AUT|Styles")
	static void SetProgressBarBackgroundBrush(class UProgressBar* ProgressBar, const FSlateBrush& BackgroundBrush);

	/**
	 * Attempts to retrieve a resource bar style from the current theme.
	 * @param StyleTag - Tag of the bar style to retrieve.
	 * @param OutStyle - The retrieved bar style if found.
	 * @return True if the style was found, false otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category = "AUT|Styles")
	static bool TryGetBarStyle(UPARAM(meta = (Categories = "UIStyle.Bar")) FGameplayTag StyleTag, FAUTBarStyle& OutStyle);

	/**
	 * The one bar look of the current theme. Every resource bar shares it and only differs by fill
	 * colour, so widgets never have to name a style.
	 * @param OutStyle - The retrieved bar style if the theme defines one.
	 * @return True if the theme defines a bar style.
	 */
	UFUNCTION(BlueprintCallable, Category = "AUT|Styles")
	static bool GetDefaultBarStyle(FAUTBarStyle& OutStyle);

	/**
	 * Attempts to retrieve a check box style from the current theme.
	 * @param StyleTag - Tag of the check box style to retrieve.
	 * @param OutStyle - The retrieved check box style if found.
	 * @return True if the style was found, false otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category = "AUT|Styles")
	static bool TryGetCheckBoxStyle(UPARAM(meta = (Categories = "UIStyle.CheckBox")) FGameplayTag StyleTag, FAUTCheckBoxStyle& OutStyle);

	/**
	 * Attempts to retrieve a title text style from the current theme.
	 * @param StyleTag - Tag of the title style to retrieve.
	 * @param OutStyle - The retrieved title text style if found.
	 * @return True if the style was found, false otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category = "AUT|Styles")
	static bool TryGetTitleStyle(UPARAM(meta = (Categories = "UIStyle.Text")) FGameplayTag StyleTag, FAUTBaseTextStyle& OutStyle);

	/**
	 * Attempts to retrieve a background brush from the current theme.
	 * @param StyleTag - Tag of the background style to retrieve.
	 * @param OutStyle - The retrieved slate brush if found.
	 * @return True if the style was found, false otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category = "AUT|Styles")
	static bool TryGetBackgroundStyle(UPARAM(meta = (Categories = "UIStyle.Background,UIStyle.Brush")) FGameplayTag StyleTag, FSlateBrush& OutStyle);

	/**
	 * Attempts to retrieve a spacer brush from the current theme.
	 * @param StyleTag - Tag of the spacer style to retrieve.
	 * @param OutStyle - The retrieved slate brush if found.
	 * @return True if the style was found, false otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category = "AUT|Styles")
	static bool TryGetSpacerStyle(UPARAM(meta = (Categories = "UIStyle.Spacer")) FGameplayTag StyleTag, FSlateBrush& OutStyle);

	/** Returns the theme currently selected in the Ascent UI Settings. */
	UFUNCTION(BlueprintPure, Category = ACF)
	static class UAUTThemeDataAsset* GetCurrentTheme();

	/**
	 * Returns the default menu level configured in the settings.
	 * @return Soft reference to the default menu world asset.
	 */
	UFUNCTION(BlueprintPure, Category = "AUT|Levels")
	static TSoftObjectPtr<UWorld> GetDefaultMenuLevel();

	/**
	 * Returns the default new game level configured in the settings.
	 * @return Soft reference to the default new game world asset.
	 */
	UFUNCTION(BlueprintPure, Category = "AUT|Levels")
	static TSoftObjectPtr<UWorld> GetDefaultNewGameLevel();

	/**
	 * Returns the array of sound classes configured for volume control.
	 * @return Array of soft references to sound class assets.
	 */
	UFUNCTION(BlueprintPure, Category = "AUT|Audio")
	static TArray<TSoftObjectPtr<USoundClass>> GetDefaultSoundClasses();

	/**
	 * Sets the volume multiplier for a specific sound class.
	 * @param TargetClass - The sound class to modify.
	 * @param NewVolume - The new volume multiplier (0.0 to 1.0).
	 */
	UFUNCTION(BlueprintCallable, Category = "AUT|Audio")
	static void SetSoundClassVolume(USoundClass* TargetClass, float NewVolume);

	/**
	 * Checks if a user widget is both valid and currently displayed on screen.
	 * @param widget - The user widget to check.
	 * @return True if the widget is valid and added to the viewport, false otherwise.
	 */
	UFUNCTION(BlueprintPure, Category = "AUT|UI")
	static bool IsValidAndOnScreen(UUserWidget* widget);

};
