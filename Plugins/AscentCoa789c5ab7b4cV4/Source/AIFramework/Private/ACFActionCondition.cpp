// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved. 


#include "ACFActionCondition.h"
#include "Actors/ACFCharacter.h"
#include "ACFAIController.h"
#include "ARSFunctionLibrary.h"
#include "ARSStatisticsComponent.h"
#include "Game/ACFFunctionLibrary.h"


bool UACFDistanceActionCondition::IsConditionMet_Implementation(const AACFCharacter* character)
  {
	if (!character)
		return false;

	const AACFCharacter* target = Cast<AACFCharacter>(character->GetTarget());

	if ( !target)
		return false;

	switch (ConditionType)
	{
	case EConditionType::EAbove:
		return UACFFunctionLibrary::CalculateDistanceBetweenActors(character, target) > Distance;
		break;
	case EConditionType::EBelow:
		return UACFFunctionLibrary::CalculateDistanceBetweenActors(character, target) < Distance;
		break;
	case EConditionType::EEqual:
		return (UACFFunctionLibrary::CalculateDistanceBetweenActors(character, target) > Distance - NearlyEqualAcceptance &&
			UACFFunctionLibrary::CalculateDistanceBetweenActors(character, target) < Distance + NearlyEqualAcceptance);
		break;
	}

	return false;

}

bool UACFStatisticActionCondition::IsConditionMet_Implementation(const AACFCharacter* character)
{
	if (!character)
		return false;

	if (!UARSFunctionLibrary::IsValidStatisticTag(StatisticTag))
		return false;

	const float statValue = character->GetStatisticsComponent()->GetNormalizedValueForStatitstic(StatisticTag);

	switch (ConditionType)
	{
	case EConditionType::EAbove:
            return statValue  > StatisticValuePercentage / 100.f;
		break;
	case EConditionType::EBelow:
            return statValue < StatisticValuePercentage / 100.f;
		break;
	case EConditionType::EEqual:
            return (statValue > StatisticValuePercentage / 100.f - NearlyEqualAcceptance || 
				statValue < StatisticValuePercentage / 100.f +NearlyEqualAcceptance);
		break;
	}

	return false;
}
