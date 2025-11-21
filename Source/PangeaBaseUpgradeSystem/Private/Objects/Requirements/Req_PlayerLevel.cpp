// Fill out your copyright notice in the Description page of Project Settings.


#include "Req_PlayerLevel.h"

#include "ACFGASStatisticsComponent.h"
#include "Actors/ACFCharacter.h"

bool UReq_PlayerLevel::IsRequirementMet_Implementation(UObject* ContextObject) const
{
	AACFCharacter* PlayerCharacter = CastChecked<AACFCharacter>(ContextObject);
	int32 PlayerLevel = PlayerCharacter->FindComponentByClass<UACFGASStatisticsComponent>()->GetCurrentLevel();
	
	return PlayerLevel >= RequiredLevel;
}
	

FText UReq_PlayerLevel::GetRequirementDescription_Implementation() const
{
	return FText::Format(
		FText::FromString(TEXT("You need to be Level {0} or higher to unlock this upgrade.")),
		FText::AsNumber(RequiredLevel)
	);
}
