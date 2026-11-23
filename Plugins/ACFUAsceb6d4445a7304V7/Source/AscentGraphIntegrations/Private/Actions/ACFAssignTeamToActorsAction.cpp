// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "Actions/ACFAssignTeamToActorsAction.h"

#include "Components/ACFTeamComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Logging.h"

void UACFAssignTeamToActorsAction::GetResolvedTargetActors(TArray<AActor*>& outActors) const
{
    outActors.Reset();

    for (const TSoftObjectPtr<AActor>& softActor : TargetActors) {
        if (softActor.IsNull()) {
            continue;
        }

        AActor* resolvedActor = softActor.Get();
        if (!resolvedActor) {
            // The actor lives in the level: it can only be resolved while its streaming cell is loaded.
            UE_LOG(ACFLog, Warning,
                TEXT("AssignACFTeamToActors: target %s is not loaded, team not assigned"), *softActor.ToString());
            continue;
        }

        outActors.Add(resolvedActor);
    }
}

UACFTeamComponent* UACFAssignTeamToActorsAction::FindTeamComponent(AActor* targetActor) const
{
    if (!targetActor) {
        return nullptr;
    }

    if (UACFTeamComponent* teamComp = targetActor->FindComponentByClass<UACFTeamComponent>()) {
        return teamComp;
    }

    if (bFallbackToControllerComponent) {
        if (const APawn* targetPawn = Cast<APawn>(targetActor)) {
            if (AController* pawnController = targetPawn->GetController()) {
                return pawnController->FindComponentByClass<UACFTeamComponent>();
            }
        }
    }

    return nullptr;
}

int32 UACFAssignTeamToActorsAction::ApplyTeamToTargets()
{
    if (!TeamToAssign.IsValid()) {
        UE_LOG(ACFLog, Warning, TEXT("AssignACFTeamToActors: no team configured on %s"), *GetNameSafe(this));
        return 0;
    }

    TArray<AActor*> resolvedActors;
    GetResolvedTargetActors(resolvedActors);

    int32 assignedCount = 0;
    for (AActor* targetActor : resolvedActors) {
        UACFTeamComponent* teamComp = FindTeamComponent(targetActor);
        if (!teamComp) {
            UE_LOG(ACFLog, Warning,
                TEXT("AssignACFTeamToActors: %s has no ACF Team Component"), *targetActor->GetName());
            continue;
        }

        if (!teamComp->GetOwner()->HasAuthority()) {
            UE_LOG(ACFLog, Warning,
                TEXT("AssignACFTeamToActors: %s is not authority, team change ignored"), *targetActor->GetName());
            continue;
        }

        teamComp->SetTeam(TeamToAssign);
        assignedCount++;
    }

    return assignedCount;
}

void UACFAssignTeamToActorsAction::ExecuteAction_Implementation(APlayerController* playerController, UAGSGraphNode* nodeOwner)
{
    Super::ExecuteAction_Implementation(playerController, nodeOwner);

    ApplyTeamToTargets();
}
