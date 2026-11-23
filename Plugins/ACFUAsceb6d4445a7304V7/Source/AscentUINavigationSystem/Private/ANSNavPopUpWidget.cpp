// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "ANSNavPopUpWidget.h"
#include "ANSUIPlayerSubsystem.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"

UANSNavPopUpWidget::UANSNavPopUpWidget()
	: Super()
{
	bIsModal = true;
}

void UANSNavPopUpWidget::SetupPopup_Implementation(const TArray<FText>& InButtons, const FText& InDialogueText, UANSNavPageWidget* InPageOwner)
{
	WidgetButtons = InButtons;
	DialogueText = InDialogueText;
	pageOwner = InPageOwner;
}

void UANSNavPopUpWidget::SetupSliderPopup_Implementation(const FText& InSliderText, const FSliderConfig& InSliderSettings, const FText& InDialogueText, UANSNavPageWidget* InPageOwner)
{
	SliderText = InSliderText;
	SliderSettings = InSliderSettings;
	DialogueText = InDialogueText;
	pageOwner = InPageOwner;
}

void UANSNavPopUpWidget::NativeOnDeactivated()
{
	Super::NativeOnDeactivated();


}

void UANSNavPopUpWidget::ResetFocusOnOwner()
{
	if (const UGameInstance* GameInst = UGameplayStatics::GetGameInstance(this))
	{
		if (UANSUIPlayerSubsystem* Subsystem = GameInst->GetSubsystem<UANSUIPlayerSubsystem>())
		{
			Subsystem->RestoreSuspendedPages();
		}
	}
}
