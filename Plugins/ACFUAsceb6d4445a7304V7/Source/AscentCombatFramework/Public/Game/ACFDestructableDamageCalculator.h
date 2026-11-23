// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Game/ACFDamageCalculation.h"

#include "ACFDestructableDamageCalculator.generated.h"

/**
 * Damage calculator for destructible props. Performs no attribute / AbilitySystem lookups:
 * damage passes through unmodified and every hit that reaches the damage pipeline triggers
 * the Chaos destruction directly on the receiver's UACFDestructableComponent. Props have no
 * Health attribute, so the statistics pipeline could never bring them to zero on its own.
 */
UCLASS()
class ASCENTCOMBATFRAMEWORK_API UACFDestructableDamageCalculator : public UACFDamageCalculation {
    GENERATED_BODY()

public:
    virtual float CalculateFinalDamage_Implementation(const FACFDamageEvent& inDamageEvent, TArray<FAttributeData>& otherAffectedAttributes) override;
};
