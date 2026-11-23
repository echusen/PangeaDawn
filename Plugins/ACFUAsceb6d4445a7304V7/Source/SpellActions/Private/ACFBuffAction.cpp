// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "ACFBuffAction.h"
#include "ACFGASStatisticsComponent.h"
#include "Actions/ACFActionAbility.h"
#include "Game/ACFFunctionLibrary.h"

void UACFBuffAction::OnNotablePointReached_Implementation()
{
    Super::OnNotablePointReached_Implementation();
    UACFGASStatisticsComponent* statComp = GetCharacterOwner()->FindComponentByClass<UACFGASStatisticsComponent>();
    if (statComp) {
        statComp->AddTimedAttributeSetModifier(BuffToApply.modifier, BuffToApply.duration);
        FComponentFX compFX;
        UACFFunctionLibrary::PlayActionEffectLocally(ActionConfig.ActionEffect, GetCharacterOwner(), this,compFX);
    }
}
