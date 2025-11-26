// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.

#pragma once

#include "ANSUITypes.h"
#include "CoreMinimal.h"
#include "Input/CommonUIInputSettings.h"
#include "InputCoreTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UITag.h"

#include "ANSUIPlayerSubsystem.generated.h"

class UUserWidget;
class UCommonUIInputSettings;
class UANSDeveloperSettings;
class UANSUIConfigComponent;
class UANSWidgetInteractionComponent;

/**
 * Delegate for notifying when the focused navigation widget changes.
 *
 * @param focusedWidget The new widget that has received focus.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnFocusedWidgetChanged, UANSNavWidget*, focusedWidget);

/**
 * Player UI Subsystem for managing in-game UI widgets and input.
 *
 * This subsystem handles the spawning and removal of widgets on the local viewport,
 * navigation between UI widgets, retrieving input actions and icons, and more.
 */
UCLASS()
class ASCENTUINAVIGATIONSYSTEM_API UANSUIPlayerSubsystem : public UGameInstanceSubsystem {
	GENERATED_BODY()

public:
	/**
	 * Spawns a widget and adds it to the local viewport.
	 *
	 * The widget is created and automatically added to the player's viewport. Optionally,
	 * the game can be paused and UI/game input can be locked while the widget is active.
	 *
	 * @param widgetClass The class of the widget to spawn.
	 * @param bPauseGame If true, the game will be paused when the widget is shown. Default is true.
	 * @param bLockGameInput If true, the game input will be locked while the widget is active. Default is true.
	 * @return A pointer to the spawned UUserWidget instance.
	 */
	UFUNCTION(BlueprintCallable, meta = (DeterminesOutputType = "widgetClass"), Category = ANS)
	UUserWidget* SpawnInGameWidget(TSubclassOf<UUserWidget> widgetClass, bool bPauseGame = true, bool bLockGameInput = true);

	/**
	 * Displays an already spawned widget and adds it to the local viewport.
	 *
	 * @param widgetRef The widget do be displayed, which must be a valid UUserWidget instance.
	 * @param bPauseGame If true, the game will be paused when the widget is shown. Default is true.
	 * @param bLockGameInput If true, the game input will be locked while the widget is active. Default is true.
	 * @return A pointer to the spawned UUserWidget instance.
	 */
	UFUNCTION(BlueprintCallable, Category = ANS)
	void DisplayInGameWidget(UUserWidget* widgetRef, bool bPauseGame = true, bool bLockGameInput = true);

	/**
	 * Removes a widget from the local viewport.
	 *
	 * The specified widget is removed from the player's viewport. Optionally, UI input is unlocked
	 * and the game is unpaused.
	 *
	 * @param widget The widget instance to remove.
	 * @param bUnlockUIInput If true, UI/game input will be unlocked. Default is true.
	 * @param bRemovePause If true, the game pause state will be removed. Default is true.
	 */
	UFUNCTION(BlueprintCallable, Category = ANS)
	void RemoveInGameWidget(UUserWidget* widget, bool bUnlockUIInput = true, bool bRemovePause = true);

	/**
	 * Navigates back to the previous widget in the widget stack.
	 *
	 * This function switches focus to the previously active widget.
	 */
	UFUNCTION(BlueprintCallable, Category = ANS)
	void GoToPreviousWidget();

	/**
	 * Retrieves UI action tags associated with a specific key.
	 *
	 * This function queries the input settings and returns a list of UI action tags that are mapped to the provided key.
	 *
	 * @param key The key for which to retrieve action tags.
	 * @param outActionsTag An array that will be filled with the corresponding UI action tags.
	 * @return True if one or more action tags were found for the key; otherwise, false.
	 */
	UFUNCTION(BlueprintCallable, Category = ANS)
	bool TryGetActionsFromKey(const FKey& key, TArray<FUIActionTag>& outActionsTag);

	/**
	 * Retrieves keys mapped to a specific UI action.
	 *
	 * This function queries the input settings to obtain a list of keys associated with the given UI action tag.
	 *
	 * @param UIAction The UI action tag to query.
	 * @param outKeys An array that will be filled with the corresponding keys.
	 * @return True if one or more keys were found for the action; otherwise, false.
	 */
	UFUNCTION(BlueprintCallable, Category = ANS)
	bool TryGetKeysForAction(const FUIActionTag& UIAction, TArray<FKey>& outKeys);

	/**
	 * Retrieves an icon associated with a gameplay tag.
	 *
	 * This function returns the texture representing the icon for the specified gameplay tag.
	 *
	 * @param iconTag The gameplay tag used to identify the icon.
	 * @return A pointer to the UTexture2D representing the icon, or nullptr if not found.
	 */
	UFUNCTION(BlueprintCallable, Category = ANS)
	UTexture2D* GetIconByTag(FGameplayTag iconTag);

	/**
	 * Retrieves the configuration for a specific UI action.
	 *
	 * This function queries the action configuration based on the UI action tag and input type.
	 *
	 * @param actionName The UI action tag.
	 * @param inputType The input type (e.g., keyboard, gamepad).
	 * @param outAction An output parameter that will contain the action configuration if found.
	 * @return True if the action configuration was found; otherwise, false.
	 */
	UFUNCTION(BlueprintCallable, Category = ANS)
	bool TryGetActionConfig(FUIActionTag actionName, const ECommonInputType& inputType, FANSActionConfig& outAction);

	/**
	 * Retrieves an icon for a specific UI action.
	 *
	 * This function returns the icon associated with the given UI action tag and input type.
	 *
	 * @param actionName The UI action tag.
	 * @param inputType The input type (e.g., keyboard, gamepad).
	 * @return A pointer to the UTexture2D representing the icon, or nullptr if not found.
	 */
	UFUNCTION(BlueprintCallable, Category = ANS)
	UTexture2D* GetIconForUIAction(FUIActionTag actionName, const ECommonInputType& inputType);

	/**
	 * Retrieves the icon for a specific key based on the current platform.
	 *
	 * This function returns the appropriate icon for the given key according to the current platform settings.
	 *
	 * @param key The key for which to retrieve the icon.
	 * @return A pointer to the UTexture2D representing the icon, or nullptr if not found.
	 */
	UFUNCTION(BlueprintCallable, Category = ANS)
	UTexture2D* GetCurrentPlatformIconForKey(const FKey& key);

	/**
	 * Retrieves the icon for a specific key for a given platform.
	 *
	 * This function returns the icon associated with the specified key and platform.
	 *
	 * @param key The key for which to retrieve the icon.
	 * @param platform The platform identifier as a string.
	 * @return A pointer to the UTexture2D representing the icon, or nullptr if not found.
	 */
	UFUNCTION(BlueprintCallable, Category = ANS)
	UTexture2D* GetIconForKey(const FKey& key, const FString& platform);

	/**
	* Returns the widget interaction component owned by the local player controller.
	 *
	 * @return The local UANSWidgetInteractionComponent, or nullptr if not found.
	*/
	UFUNCTION(BlueprintPure, Category = ANS)
	UANSWidgetInteractionComponent* GetLocalInteractionComponent() const;

	/**
	 * Event triggered when the focused navigation widget changes.
	 */
	UPROPERTY(BlueprintAssignable, Category = ANS)
	FOnFocusedWidgetChanged OnFocusChanged;

private:
	/**
	 * Retrieves the common UI input settings.
	 *
	 * This internal function returns the current UCommonUIInputSettings used by the system.
	 *
	 * @return A pointer to the UCommonUIInputSettings instance.
	 */
	UCommonUIInputSettings* GetInputSettings() const;

	/**
	 * Retrieves the UI developer settings.
	 *
	 * This internal function returns the current UANSDeveloperSettings used by the system.
	 *
	 * @return A pointer to the UANSDeveloperSettings instance.
	 */
	UANSUIConfigComponent* GetUIConfig() const;

	/** The currently active widget on the viewport. */
	UPROPERTY()
	TObjectPtr<UUserWidget> currentWidget;

	/** A stack of widget classes representing the navigation history. */
	UPROPERTY()
	TArray<UUserWidget*> widgetStack;

	/** Default value for pausing the game when a widget is displayed. */
	bool bDefaultPauseGame = true;

	/** Default value for showing the mouse cursor when a widget is displayed. */
	bool bDefaultShowMouseCursor = true;
};
