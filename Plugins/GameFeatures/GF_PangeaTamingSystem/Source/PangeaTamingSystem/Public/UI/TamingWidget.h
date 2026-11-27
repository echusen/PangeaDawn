// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/ACFHUDUserWidget.h"
#include "TamingWidget.generated.h"

/** Broadcast when the taming minigame ends. bSuccess is true when enough presses were made in time. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTamingMinigameResult, bool, bSuccess);

/**
 * Taming minigame page: the player must press the confirm key RequiredPresses times before
 * the countdown expires. Reports the outcome through OnResult to UPangeaTamingComponent.
 */
UCLASS()
class PANGEATAMINGSYSTEM_API UTamingWidget : public UACFHUDUserWidget
{
	GENERATED_BODY()

public:
	/** Length of the countdown, in seconds. */
	UPROPERTY(BlueprintReadWrite, Category = "Taming")
	float Duration = 5.f;

	/** Number of confirm presses needed to win the minigame. */
	UPROPERTY(BlueprintReadWrite, Category = "Taming")
	float RequiredPresses = 5;

	/** Confirm presses counted since the countdown started. */
	UPROPERTY(BlueprintReadWrite, Category = "Taming")
	float CurrentPresses = 0;

	/** Broadcast when the minigame ends, with true on success and false on timeout, cancel or teardown. */
	UPROPERTY(BlueprintAssignable, Category = "Taming")
	FOnTamingMinigameResult OnResult;

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnDeactivated() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** Counts one confirm press while the countdown runs, and ends the minigame with success once RequiredPresses is reached. */
	UFUNCTION(BlueprintCallable, Category = "Taming")
	void OnPressedE();

protected:
	FTimerHandle TimerHandle;

	void OnTimeExpired();

private:
	friend class UPangeaTamingComponent;

	// Native initialization detail: called after runtime duration and result binding.
	void StartCountdown(float InDuration);
};
