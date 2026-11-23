// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "Abilities/ACFBoatBroadsideFireAbility.h"
#include "AI/ACFBoatAIController.h"
#include "Components/ACFShootingComponent.h"
#include "Components/MeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Perception/AISense_Hearing.h"
#include "TimerManager.h"

UACFBoatBroadsideFireAbility::UACFBoatBroadsideFireAbility()
{
    // The volley is timer-driven: no montage to auto-play, and the boat BT must keep steering
    ActionConfig.bAutoExecute = false;
    ActionConfig.bStopBehavioralThree = false;
}

void UACFBoatBroadsideFireAbility::OnActionStarted_Implementation()
{
    Super::OnActionStarted_Implementation();

    APawn* pawn = Cast<APawn>(GetAvatarActorFromActorInfo());
    if (!pawn) {
        ExitAction(true);
        return;
    }

    AActor* target = nullptr;
    if (const AACFBoatAIController* boatController = Cast<AACFBoatAIController>(pawn->GetController())) {
        target = boatController->GetTargetActorBK();
    }
    VolleyTarget = target;

    FVector toTarget = FVector::ZeroVector;
    if (target) {
        toTarget = target->GetActorLocation() - pawn->GetActorLocation();
        toTarget.Z = 0.f;
        toTarget.Normalize();
    }

    PendingComps.Reset();
    NextCompIndex = 0;
    TInlineComponentArray<UACFShootingComponent*> shootingComps(pawn);
    for (UACFShootingComponent* comp : shootingComps) {
        if (!comp->CanShoot()) {
            continue;
        }
        if (target && comp->GetShootingMesh()) {
            // Only the cannons whose socket faces the target take part in the volley
            const FVector muzzleDir = comp->GetShootingMesh()->GetSocketRotation(comp->GetProjectileStartSocketName()).Vector();
            if (FVector::DotProduct(muzzleDir, toTarget) <= 0.f) {
                continue;
            }
        }
        PendingComps.Add(comp);
    }

    if (PendingComps.Num() == 0) {
        ExitAction(true);
        return;
    }

    UAISense_Hearing::ReportNoiseEvent(pawn->GetWorld(), pawn->GetActorLocation(), CannonNoiseLoudness, pawn, 0.f, FName("CannonFire"));

    FireNextShot();
    if (NextCompIndex < PendingComps.Num() && GetWorld()) {
        GetWorld()->GetTimerManager().SetTimer(VolleyTimerHandle, FTimerDelegate::CreateUObject(this, &UACFBoatBroadsideFireAbility::FireNextShot), VolleyInterval, true);
    }
}

void UACFBoatBroadsideFireAbility::OnActionEnded_Implementation()
{
    if (GetWorld()) {
        GetWorld()->GetTimerManager().ClearTimer(VolleyTimerHandle);
    }
    PendingComps.Reset();
    VolleyTarget = nullptr;

    Super::OnActionEnded_Implementation();
}

void UACFBoatBroadsideFireAbility::FireNextShot()
{
    if (!PendingComps.IsValidIndex(NextCompIndex)) {
        if (GetWorld()) {
            GetWorld()->GetTimerManager().ClearTimer(VolleyTimerHandle);
        }
        ExitAction(false);
        return;
    }

    UACFShootingComponent* comp = PendingComps[NextCompIndex].Get();
    NextCompIndex++;

    if (comp) {
        if (AActor* target = VolleyTarget.Get()) {
            comp->ShootAtActor(target, RandomDeviation, 1.f);
        } else if (comp->GetShootingMesh()) {
            comp->ShootAtDirection(comp->GetShootingMesh()->GetSocketRotation(comp->GetProjectileStartSocketName()), 1.f);
        }
    }

    if (NextCompIndex >= PendingComps.Num()) {
        if (GetWorld()) {
            GetWorld()->GetTimerManager().ClearTimer(VolleyTimerHandle);
        }
        ExitAction(false);
    }
}
