// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "Game/ACFDestructableDamageCalculator.h"
#include "Components/ACFDestructableComponent.h"
#include <GameFramework/Actor.h>

float UACFDestructableDamageCalculator::CalculateFinalDamage_Implementation(const FACFDamageEvent& inDamageEvent, TArray<FAttributeData>& otherAffectedAttributes)
{
    if (inDamageEvent.DamageReceiver) {
        if (UACFDestructableComponent* Destructable = inDamageEvent.DamageReceiver->FindComponentByClass<UACFDestructableComponent>()) {
            Destructable->HandleHitDestruction(inDamageEvent);
        }
    }
    return inDamageEvent.FinalDamage;
}
