// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/TamingWidget.h"
#include "ANSUIPlayerSubsystem.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"

void UTamingWidget::NativeConstruct()
{
	Super::NativeConstruct();

	CurrentPresses = 0;

}

void UTamingWidget::StartCountdown(float InDuration)
{
	Duration = FMath::Max(0.1f, InDuration);
	CurrentPresses = 0.f;
	if (UWorld* World = GetWorld()) World->GetTimerManager().SetTimer(TimerHandle, this, &UTamingWidget::OnTimeExpired, Duration, false);
}

void UTamingWidget::NativeOnDeactivated()
{
	const bool bWasRunning = GetWorld() && GetWorld()->GetTimerManager().IsTimerActive(TimerHandle);
	if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(TimerHandle);
	Super::NativeOnDeactivated();
	if (bWasRunning) OnResult.Broadcast(false);
}

void UTamingWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	APlayerController* PC = GetOwningPlayer();
	if (!PC || !GetWorld() || !GetWorld()->GetTimerManager().IsTimerActive(TimerHandle)) return;
	UANSUIPlayerSubsystem* UI = GetGameInstance()->GetSubsystem<UANSUIPlayerSubsystem>();
	TArray<FKey> ConfirmKeys, CancelKeys;
	if (UI)
	{
		UI->TryGetKeysForAction(FUIActionTag::TryConvert(FGameplayTag::RequestGameplayTag(TEXT("UI.Action.Confirm"))), ConfirmKeys);
		UI->TryGetKeysForAction(FUIActionTag::TryConvert(FGameplayTag::RequestGameplayTag(TEXT("UI.Action.Cancel"))), CancelKeys);
	}
	if (ConfirmKeys.IsEmpty()) { ConfirmKeys.Add(EKeys::E); ConfirmKeys.Add(EKeys::Gamepad_FaceButton_Bottom); }
	if (CancelKeys.IsEmpty()) { CancelKeys.Add(EKeys::Escape); CancelKeys.Add(EKeys::Gamepad_FaceButton_Right); }
	for (const FKey& Key : CancelKeys) if (PC->WasInputKeyJustPressed(Key)) { OnTimeExpired(); return; }
	for (const FKey& Key : ConfirmKeys) if (PC->WasInputKeyJustPressed(Key)) { OnPressedE(); break; }
	if (UProgressBar* Bar = Cast<UProgressBar>(GetWidgetFromName(TEXT("ProgressBar_323"))))
		Bar->SetPercent(FMath::Clamp(CurrentPresses / FMath::Max(1.f, RequiredPresses), 0.f, 1.f));
	if (UTextBlock* Label = Cast<UTextBlock>(GetWidgetFromName(TEXT("TextBlock_97"))))
		Label->SetText(FText::Format(NSLOCTEXT("PangeaUI", "TamingProgress", "Taming: {0}/{1}  |  {2}s\nPress {3} repeatedly"),
			FText::AsNumber(FMath::RoundToInt(CurrentPresses)), FText::AsNumber(FMath::CeilToInt(RequiredPresses)),
			FText::AsNumber(FMath::Max(0, FMath::CeilToInt(GetWorld()->GetTimerManager().GetTimerRemaining(TimerHandle)))), ConfirmKeys[0].GetDisplayName()));
}

void UTamingWidget::NativeDestruct()
{
	const bool bWasRunning = GetWorld() && GetWorld()->GetTimerManager().IsTimerActive(TimerHandle);
	if (UWorld* World = GetWorld())
		World->GetTimerManager().ClearTimer(TimerHandle);
	Super::NativeDestruct();
	if (bWasRunning) OnResult.Broadcast(false);
}

void UTamingWidget::OnPressedE()
{
	if (!GetWorld() || !GetWorld()->GetTimerManager().IsTimerActive(TimerHandle)) return;
	CurrentPresses++;
	if (CurrentPresses >= RequiredPresses)
	{
		GetWorld()->GetTimerManager().ClearTimer(TimerHandle);
		OnResult.Broadcast(true);
	}
}

void UTamingWidget::OnTimeExpired()
{
	if (UWorld* World = GetWorld())
		World->GetTimerManager().ClearTimer(TimerHandle);
	OnResult.Broadcast(false);
}


