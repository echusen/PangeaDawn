// Fill out your copyright notice in the Description page of Project Settings.


#include "Objects/UpgradeRequirement.h"

bool UUpgradeRequirement::IsRequirementMet_Implementation(UObject* ContextObject) const
{
	return false; // Default: fail
}

FText UUpgradeRequirement::GetFailureMessage() const
{
	return FText::GetEmpty();
}
