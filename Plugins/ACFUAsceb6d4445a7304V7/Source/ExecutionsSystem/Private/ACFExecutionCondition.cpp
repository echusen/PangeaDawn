// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved. 

#include "ACFExecutionCondition.h"


#include "ACFCombinedAnimSlaveComponent.h"
#include "ACFGASStatisticsComponent.h"
#include "CASAnimCondition.h"
#include "CASAnimSlaveComponent.h"
#include "CASTypes.h"
#include "Game/ACFFunctionLibrary.h"
#include <GameFramework/Character.h>
#include "GameplayTagsManager.h"

bool UACFExecutionCondition::VerifyCondition_Implementation(const FCombinedAnimConfig& contextAnim, const class ACharacter* animMaster)
{
	if (FMath::RandRange(0.f, 100.f) > ExecutionChance) {
		return false;
	}
	if (!animMaster || !GetOwnerComponent()) {
		return false;
	}

	const UACFGASStatisticsComponent* ownerStatComp = GetOwnerComponent()->GetOwner()->FindComponentByClass< UACFGASStatisticsComponent>();
	if (ownerStatComp) {

		const FGameplayTag healthTag = UACFFunctionLibrary::GetHealthTag();
		const float health = ownerStatComp->GetNormalizedValueForStatitstic(healthTag);

		if ((RemainingHealthPercentage / 100.f) > health) {
			return true;
		}
	}

	return false;

}

