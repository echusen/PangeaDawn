// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2025. All Rights Reserved.

#include "ACFCombinedAnimSlaveComponent.h"
#include "CASTypes.h"
#include <GameplayTagContainer.h>
#include "CASAnimCondition.h"

// Sets default values for this component's properties
UACFCombinedAnimSlaveComponent::UACFCombinedAnimSlaveComponent()
{
    // Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
    // off to improve performance if you don't need them.
    PrimaryComponentTick.bCanEverTick = false;

    // ...
}

bool UACFCombinedAnimSlaveComponent::CanStartCombinedAnimation(const FCombinedAnimConfig& combinedAnim, const class ACharacter* animMaster)
{
    return AnimTags.HasAll(combinedAnim.SlaveRequiredTags) && VerifyConditions(combinedAnim, animMaster);
}

// Called when the game starts
void UACFCombinedAnimSlaveComponent::BeginPlay()
{
    Super::BeginPlay();

    // ...
}

bool UACFCombinedAnimSlaveComponent::VerifyConditions(const FCombinedAnimConfig& animTag, const ACharacter* animMaster)
{
    for (const auto condition : AnimStartingConditions) {
        if (!condition->Internal_VerifyCondition(this, animTag, animMaster)) {
            return false;
        }
    }
    return true;
}