// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "Cue/ACFDestructableGameplayCue.h"
#include "ACMCollisionsFunctionLibrary.h"
#include "ACMTypes.h"
#include "Components/ACFDestructableComponent.h"
#include "Logging.h"

UACFDestructableGameplayCue::UACFDestructableGameplayCue()
{
}

bool UACFDestructableGameplayCue::OnExecute_Implementation(AActor* Target, const FGameplayCueParameters& Parameters) const
{
    const UACFDestructableComponent* Destructable = Cast<UACFDestructableComponent>(Parameters.GetSourceObject());
    if (!Destructable || !Destructable->GetOwner()) {
        UE_LOG(ACFLog, Warning, TEXT("Missing destructable SourceObject! - UACFDestructableGameplayCue::OnExecute_Implementation"));
        return false;
    }

    FVector SpawnLocation = Parameters.Location;
    if (SpawnLocation.IsNearlyZero()) {
        SpawnLocation = Destructable->GetOwner()->GetActorLocation();
    }

    FComponentFX outComps;
    UACMCollisionsFunctionLibrary::SpawnSoundAndParticleAtLocation(Target, FImpactFX(Destructable->GetDestructionFX(), SpawnLocation), outComps);
    return true;
}
