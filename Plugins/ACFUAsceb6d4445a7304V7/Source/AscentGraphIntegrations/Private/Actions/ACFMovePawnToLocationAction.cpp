// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "Actions/ACFMovePawnToLocationAction.h"

#include "ACFAIController.h"
#include "Engine/TargetPoint.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PawnMovementComponent.h"

void UACFMovePawnToLocationAction::ExecuteAction_Implementation(APlayerController* playerController, UAGSGraphNode* nodeOwner)
{
    Super::ExecuteAction_Implementation(playerController, nodeOwner);

    UWorld* World = GetWorld();
    if (!World || World->GetNetMode() == NM_Client) {
        return;
    }

    // Like the original Blueprint Delay, repeated activation does not queue duplicate moves.
    if (World->GetTimerManager().IsTimerActive(MoveTimerHandle)) {
        return;
    }

    if (Delay > 0.0f) {
        World->GetTimerManager().SetTimer(MoveTimerHandle, this,
            &UACFMovePawnToLocationAction::ExecuteDelayedMove, Delay, false);
    } else {
        MovePawnToLocation();
    }
}

void UACFMovePawnToLocationAction::ExecuteDelayedMove()
{
    MovePawnToLocation();
}

bool UACFMovePawnToLocationAction::MovePawnToLocation()
{
    APawn* Pawn = TargetPawn.Get();
    ATargetPoint* Destination = TargetPoint.Get();
    if (!IsValid(Pawn) || !IsValid(Destination)) {
        UE_LOG(LogTemp, Warning, TEXT("ACF MovePawnToLocation: pawn or destination is not loaded. Pawn=%s Destination=%s"),
            *TargetPawn.ToSoftObjectPath().ToString(), *TargetPoint.ToSoftObjectPath().ToString());
        return false;
    }

    if (!Pawn->HasAuthority() || Pawn->GetWorld() != Destination->GetWorld()) {
        UE_LOG(LogTemp, Warning, TEXT("ACF MovePawnToLocation: requires authority and actors in the same world."));
        return false;
    }

    AACFAIController* AIController = Cast<AACFAIController>(Pawn->GetController());
    if (!IsValid(AIController)) {
        UE_LOG(LogTemp, Warning, TEXT("ACF MovePawnToLocation: %s has no ACFAIController; relocation skipped."), *GetNameSafe(Pawn));
        return false;
    }

    const FVector Location = Destination->GetActorLocation();
    const FRotator Rotation = Destination->GetActorRotation();

    // Preserve the original action's unswept relocation semantics. Author a collision-safe target point.
    if (!Pawn->SetActorLocationAndRotation(Location, Rotation, false, nullptr, ETeleportType::TeleportPhysics)) {
        UE_LOG(LogTemp, Warning, TEXT("ACF MovePawnToLocation: failed to relocate %s; home unchanged."), *GetNameSafe(Pawn));
        return false;
    }

    // Stop the old request before notifying the active state of its new home.
    AIController->StopMovement();
    if (bChangeHome) {
        AIController->SetHomeLocation(Pawn->GetActorLocation());
    }
    if (UPawnMovementComponent* Movement = Pawn->GetMovementComponent()) {
        Movement->StopMovementImmediately();
    }

    return true;
}
