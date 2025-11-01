// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/ACFHUDUserWidget.h"
#include "TamingWidget.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTamingMinigameResult, bool, bSuccess);

/**
 * 
 */
UCLASS()
class PANGEATAMINGSYSTEM_API UTamingWidget : public UACFHUDUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, Category="Taming")
	float Duration = 5.f;

	UPROPERTY(BlueprintReadWrite, Category="Taming")
	float RequiredPresses = 5;

	UPROPERTY(BlueprintReadWrite, Category="Taming")
	float CurrentPresses = 0;

	UPROPERTY(BlueprintAssignable, Category="Taming")
	FOnTamingMinigameResult OnResult;

	virtual void NativeConstruct() override;

	UFUNCTION(BlueprintCallable, Category="Taming")
	void OnPressedE();

protected:
	FTimerHandle TimerHandle;

	void OnTimeExpired();

};
