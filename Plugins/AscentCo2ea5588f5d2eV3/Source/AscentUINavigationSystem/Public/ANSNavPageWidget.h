// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.

#pragma once

#include "ANSUITypes.h"
#include "Blueprint/UserWidget.h"
#include "CommonActivatableWidget.h"
#include "CoreMinimal.h"
#include "Layout/Geometry.h"

#include "ANSNavPageWidget.generated.h"

class UANSNavWidget;
class UANSNavbarWidget;

/**
 *
 */

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnWidgetRemoved);

UCLASS()
class ASCENTUINAVIGATIONSYSTEM_API UANSNavPageWidget : public UCommonActivatableWidget {
    GENERATED_BODY()

public:
    /* Constructor for the navigation page widget */
     UANSNavPageWidget();

    /* Sets the initial focus for gamepad navigation to the provided widget.
     * If no widget is specified, it will use the default one.
     */
    UFUNCTION(BlueprintCallable, Category = ANS)
    void SetStartFocus(UANSNavWidget* navWidget = nullptr);

    /* Resets the focus to the default focusable widget,
     * which is obtained from GetDesiredFocusTarget.
     */
    UFUNCTION(BlueprintCallable, Category = ANS)
    void ResetDefaultFocus();

    /* Sets the focus to a specific widget.
     * If no widget is provided, the focus remains unchanged.
     */
    UFUNCTION(BlueprintCallable, Category = ANS)
    void SetFocusToWidget(UANSNavWidget* navWidget = nullptr);

    /* Returns the currently focused navigation widget */
    UFUNCTION(BlueprintPure, Category = ANS)
    UANSNavWidget* GetFocusedWidget() const
    {
        return focusedWidget.parentNavWidget;
    }

    /* Enables or disables gamepad navigation for this page */
    UFUNCTION(BlueprintCallable, Category = ANS)
    void SetNavigationEnabled(bool bNavEnabled);

    /* Disables gamepad navigation */
    UFUNCTION(BlueprintCallable, Category = ANS)
    void DisableNavigation();

    /* Checks whether gamepad navigation is enabled */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = ANS)
    bool GetNavigationEnabled() const
    {
        return bIsNavEnabled;
    }

    /* Returns the currently active navigation bar widget */
    UFUNCTION(BlueprintPure, Category = ANS)
    UANSNavbarWidget* GetCurrentNavbar();

    /* Removes the current widget from the screen and spawns the provided one.
     * Used to transition between navigation pages.
     */
    UFUNCTION(BlueprintCallable, Category = ANS)
    void GoToWidget(TSubclassOf<UUserWidget> nextPage);

    /* Removes the current widget from the screen and re-spawns
     * the last removed widget, allowing users to go back to the previous page.
     */
    UFUNCTION(BlueprintCallable, Category = ANS)
    void GoToPreviousWidget();

    /* Checks whether the system automatically switches input focus
     * between mouse and gamepad navigation.
     */
    UFUNCTION(BlueprintPure, Category = ANS)
    bool GetAutoSwitchFromMouseAndGamepad() const { return bAutoSwitchFromMouseAndGamepad; }

    /* Enables or disables the automatic switching between
     * mouse and gamepad navigation input.
     */
    UFUNCTION(BlueprintCallable, Category = ANS)
    void SetAutoSwitchFromMouseAndGamepad(bool val) { bAutoSwitchFromMouseAndGamepad = val; }

    /* Delegate that is triggered when the widget is removed from the viewport */
    UPROPERTY(BlueprintAssignable, Category = ANS)
    FOnWidgetRemoved OnWidgetRemoved;

protected:
    UFUNCTION(BlueprintNativeEvent, Category = ANS)
    void OnFocusedWidgetChanged(const FFocusedWidget& newFocusedWidget);

    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    UCommonInputSubsystem* GetInputSubsystem() const;
    void GatherNavbar();

    virtual void NativePreConstruct() override;
    virtual FReply NativeOnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
    virtual FNavigationReply NativeOnNavigation(const FGeometry& MyGeometry, const FNavigationEvent& InNavigationEvent, const FNavigationReply& InDefaultReply) override;

    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    void CheckFocusedWidget(const FGeometry& MyGeometry);

    virtual void NativeOnAddedToFocusPath(const FFocusEvent& InFocusEvent) override;

    UPROPERTY(BlueprintReadOnly, Category = ANS)
    UANSNavbarWidget* navBar;

    UPROPERTY(EditAnywhere, Category = ANS)
    bool bAutoSwitchFromMouseAndGamepad = true;

private:
    void Internal_SetFocusToWidget(const FFocusedWidget& widget, const FGeometry& MyGeometry);
    bool Internal_SetFocusToNavWidget(UANSNavWidget* widget, const FGeometry& MyGeometry);

    void Internal_SetStartFocus();
    void RemoveFocusToCurrentWidget();

    TObjectPtr<class UANSUIPlayerSubsystem> UISubsystem;

    // class UWidget* currentlyFocusedWidget;
    FFocusedWidget startFocusedWidget;
    bool bPendingFocusRequest = false;
    bool bIsNavEnabled;

    FFocusedWidget focusedWidget;

    bool bNeedsReset;

    UFUNCTION()
    void HandleInputChanged(ECommonInputType bNewInputType);
};
