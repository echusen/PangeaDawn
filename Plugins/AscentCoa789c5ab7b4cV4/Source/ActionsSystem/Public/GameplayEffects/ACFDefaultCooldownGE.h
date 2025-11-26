// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "ACFDefaultCooldownGE.generated.h"

/**
 * 
 */
UCLASS()
class ACTIONSSYSTEM_API UACFDefaultCooldownGE : public UGameplayEffect
{
	GENERATED_BODY()

	UACFDefaultCooldownGE() {
            DurationPolicy = EGameplayEffectDurationType::HasDuration;
	}

	
};
