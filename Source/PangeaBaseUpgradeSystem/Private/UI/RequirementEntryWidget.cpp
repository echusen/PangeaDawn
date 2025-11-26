// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/RequirementEntryWidget.h"

#include "Components/TextBlock.h"
#include "Objects/UpgradeRequirement.h"

void URequirementEntryWidget::InitFromRequirement(UUpgradeRequirement* InRequirement, bool bMet)
{
	Requirement = InRequirement;

	if (RequirementText && Requirement)
	{
		RequirementText->SetText(Requirement->GetRequirementDescription());
	}

	if (StatusText)
	{
		StatusText->SetText(bMet ? FText::FromString(TEXT("Met"))
								 : FText::FromString(TEXT("Not Met")));
	}
}
