// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/TamingWidget.h"

void UTamingWidget::NativeConstruct()
{
	Super::NativeConstruct();

	CurrentPresses = 0;

	// Start countdown
	GetWorld()->GetTimerManager().SetTimer(TimerHandle, this, &UTamingWidget::OnTimeExpired, Duration, false);
}

void UTamingWidget::OnPressedE()
{
	CurrentPresses++;
	if (CurrentPresses >= RequiredPresses)
	{
		OnResult.Broadcast(true);
		RemoveFromParent();
		GetWorld()->GetTimerManager().ClearTimer(TimerHandle);
	}
}

void UTamingWidget::OnTimeExpired()
{
	OnResult.Broadcast(false);
	RemoveFromParent();
}


