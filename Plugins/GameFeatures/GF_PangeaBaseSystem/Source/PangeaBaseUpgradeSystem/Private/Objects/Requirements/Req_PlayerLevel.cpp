// Fill out your copyright notice in the Description page of Project Settings.

#include "Req_PlayerLevel.h"

#include "ACFGASStatisticsComponent.h"
#include "Actors/ACFCharacter.h"

bool UReq_PlayerLevel::IsRequirementMetForContext(const FBaseUpgradeContext& Context) const
{
	return IsRequirementMet(Context.InteractingPawn != nullptr ? static_cast<UObject*>(Context.InteractingPawn) : Context.GetPreferredRequirementObject());
}

bool UReq_PlayerLevel::IsRequirementMet_Implementation(UObject* ContextObject) const
{
	AACFCharacter* const PlayerCharacter = Cast<AACFCharacter>(ContextObject);
	if (PlayerCharacter == nullptr)
	{
		return false;
	}

	UACFGASStatisticsComponent* const Statistics = PlayerCharacter->FindComponentByClass<UACFGASStatisticsComponent>();
	if (Statistics == nullptr)
	{
		return false;
	}

	const int32 PlayerLevel = Statistics->GetCurrentLevel();
	return PlayerLevel >= RequiredLevel;
}

FText UReq_PlayerLevel::GetRequirementDescription_Implementation() const
{
	return FText::Format(FText::FromString(TEXT("You need to be Level {0} or higher to unlock this upgrade.")), FText::AsNumber(RequiredLevel));
}
