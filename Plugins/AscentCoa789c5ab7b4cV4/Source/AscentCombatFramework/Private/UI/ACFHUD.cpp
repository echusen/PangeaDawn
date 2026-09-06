// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "UI/ACFHUD.h"
#include "Blueprint/UserWidget.h"
#include <Kismet/GameplayStatics.h>

void AACFHUD::BeginPlay()
{
	Super::BeginPlay();
}

void AACFHUD::PostInitProperties()
{
	Super::PostInitProperties();
	InitHUD();
}

void AACFHUD::PostRender()
{
	Super::PostRender();
	if (HUDWidget) {
		if (bShowHUD != enabled) {
			SetHudEnabled(bShowHUD);
		}

	}
}

void AACFHUD::InitHUD()
{
	APlayerController* playerController = UGameplayStatics::GetPlayerController(this, 0);
	if (HUDClass && playerController) {
		HUDWidget = CreateWidget<UUserWidget>(playerController, HUDClass);
		HUDWidget->AddToViewport(0);
	}
}



void AACFHUD::SetHudEnabled(bool bEnabled)
{
	bShowHUD = bEnabled;
	enabled = bShowHUD;
	if (!HUDWidget) {
		return;
	}

	if (bEnabled) {
		HUDWidget->SetVisibility(ESlateVisibility::Visible);

	}
	else {
		HUDWidget->SetVisibility(ESlateVisibility::Hidden);
	}
}
