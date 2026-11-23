// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "ANSNavPageWidget.h"
#include "AUTTypes.h"
#include "CoreMinimal.h"

#include "ANSNavPopUpWidget.generated.h"

/**
 * Modal pop-up page that inherits from ANSNavPageWidget.
 *
 * Push this widget onto a separate UI layer (e.g. UI.Layer.Modal) so the page
 * underneath stays visible. Common UI automatically routes input to the topmost
 * active widget and restores focus when this pop-up is deactivated.
 *
 * The Setup functions and the delegates below are the contract every pop-up shares,
 * so callers can spawn a pop-up by tag from the Widget Registry and drive it without
 * knowing its concrete Blueprint class. A pop-up implements the Setup it needs and
 * broadcasts the matching delegate: buttons use SetupPopup / OnSelected, sliders use
 * SetupSliderPopup / OnValueSelected.
 */
UCLASS()
class ASCENTUINAVIGATIONSYSTEM_API UANSNavPopUpWidget : public UANSNavPageWidget {
	GENERATED_BODY()

public:
	UANSNavPopUpWidget();

	/**
	 * Broadcast when the user picks one of the pop-up buttons.
	 * Branch on SelectedIndex: SelectedButton is the localized label and changes with
	 * the game language, so it must never be used in a Switch on String.
	 *
	 * Declared inside the class on purpose: a Blueprint function parameter can only be typed
	 * on a delegate whose signature function is owned by a UClass, so a file-scope delegate
	 * cannot be passed as an input to a Blueprint function.
	 */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPopUpSelected, FString, SelectedButton, int32, SelectedIndex);

	/** Broadcast when the user confirms a slider pop-up, carrying the value the slider was left on. */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPopUpValueSelected, float, SelectedValue);

	/**
	 * Fills the pop-up with the content to display. The default implementation only
	 * stores the data into DialogueText, WidgetButtons and pageOwner; override it in
	 * Blueprint to build the actual buttons, and call Parent: Setup Popup first so
	 * those values are available to the rest of the graph.
	 *
	 * @param InButtons      Labels of the buttons to build, in order. The index of a
	 *                       label here is the SelectedIndex broadcast by OnSelected.
	 * @param InDialogueText Message shown in the body of the pop-up.
	 * @param InPageOwner    The page that spawned this pop-up, restored on close.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = ANS)
	void SetupPopup(const TArray<FText>& InButtons, const FText& InDialogueText, UANSNavPageWidget* InPageOwner);
	virtual void SetupPopup_Implementation(const TArray<FText>& InButtons, const FText& InDialogueText, UANSNavPageWidget* InPageOwner);

	/**
	 * Fills a pop-up that asks for a value instead of a choice. The default implementation
	 * only stores the data into SliderText, SliderSettings and DialogueText; override it in
	 * Blueprint to push those values into the slider, and call Parent: Setup Slider Popup
	 * first so they are available to the rest of the graph.
	 *
	 * @param InSliderText     Label shown next to the slider.
	 * @param InSliderSettings Range, step and default value of the slider.
	 * @param InDialogueText   Message shown in the body of the pop-up.
	 * @param InPageOwner     The page that spawned this pop-up, restored on close.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = ANS)
	void SetupSliderPopup(const FText& InSliderText, const FSliderConfig& InSliderSettings, const FText& InDialogueText, UANSNavPageWidget* InPageOwner);
	virtual void SetupSliderPopup_Implementation(const FText& InSliderText, const FSliderConfig& InSliderSettings, const FText& InDialogueText, UANSNavPageWidget* InPageOwner);

	/** Fired when the user picks one of the buttons. Broadcast it from the pop-up Blueprint. */
	UPROPERTY(BlueprintAssignable, BlueprintCallable, Category = ANS)
	FOnPopUpSelected OnSelected;

	/** Fired when the user confirms a slider pop-up. Broadcast it from the pop-up Blueprint. */
	UPROPERTY(BlueprintAssignable, BlueprintCallable, Category = ANS)
	FOnPopUpValueSelected OnValueSelected;

protected:
	virtual void NativeOnDeactivated() override;

	/** The page that spawned this pop-up. Available for Blueprint logic that needs a reference back to the owner. */
	UPROPERTY(BlueprintReadWrite, Category = ANS, Meta = (ExposeOnSpawn = true))
	UANSNavPageWidget* pageOwner;

	/** Message shown in the body of the pop-up. Filled by SetupPopup and SetupSliderPopup. */
	UPROPERTY(BlueprintReadWrite, Category = ANS)
	FText DialogueText;

	/** Labels of the buttons to build, in order. Filled by SetupPopup. */
	UPROPERTY(BlueprintReadWrite, Category = ANS)
	TArray<FText> WidgetButtons;

	/** Label shown next to the slider. Filled by SetupSliderPopup. */
	UPROPERTY(BlueprintReadWrite, Category = ANS)
	FText SliderText;

	/** Range, step and default value of the slider. Filled by SetupSliderPopup. */
	UPROPERTY(BlueprintReadWrite, Category = ANS)
	FSliderConfig SliderSettings;

	/** Restores navigation on the owning page and returns focus to its last focused widget. */
	UFUNCTION(BlueprintCallable, Category = ANS)
	void ResetFocusOnOwner();
};
