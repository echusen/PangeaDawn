// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/FacilityUnlockEntryWidget.h"

#include "Components/TextBlock.h"

void UFacilityUnlockEntryWidget::InitFromFacilityTag(FGameplayTag InTag)
{
	FacilityTag = InTag;

	if (FacilityNameText)
	{
		FacilityNameText->SetText(FText::FromString(FacilityTag.ToString()));
	}
}

void UFacilityUnlockEntryWidget::InitFromFacilityDisplayName(FGameplayTag InTag, const FText& DisplayName)
{
	FacilityTag = InTag;

	if (FacilityNameText)
	{
		FacilityNameText->SetText(DisplayName);
	}
}
