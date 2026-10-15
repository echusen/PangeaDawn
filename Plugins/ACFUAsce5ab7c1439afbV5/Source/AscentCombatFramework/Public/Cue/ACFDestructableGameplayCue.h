// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Static.h"

#include "ACFDestructableGameplayCue.generated.h"

/**
 * Generic destruction cue: reads the particle & sound configured on the destroyed actor
 * (FGameplayCueParameters::SourceObject, its UACFDestructableComponent) and spawns them at
 * the cue Location (the impact point). The component executes it server-side through the
 * damage dealer's AbilitySystemComponent, so it replicates to every client without the prop
 * needing an AbilitySystem of its own.
 */
UCLASS()
class ASCENTCOMBATFRAMEWORK_API UACFDestructableGameplayCue : public UGameplayCueNotify_Static {
    GENERATED_BODY()

public:
    UACFDestructableGameplayCue();

protected:
    bool OnExecute_Implementation(AActor* Target, const FGameplayCueParameters& Parameters) const;
};
