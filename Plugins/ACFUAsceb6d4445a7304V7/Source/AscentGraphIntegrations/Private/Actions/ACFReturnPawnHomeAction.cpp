// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "Actions/ACFReturnPawnHomeAction.h"

#include "ACFAIController.h"
#include "ACFAITypes.h"
#include "Engine/TargetPoint.h"
#include "GameFramework/Pawn.h"
#include "GameplayTagsManager.h"

void UACFReturnPawnHomeAction::ExecuteAction_Implementation(APlayerController* playerController, UAGSGraphNode* nodeOwner)
{
    Super::ExecuteAction_Implementation(playerController, nodeOwner);
    ReturnPawnHome();
}

bool UACFReturnPawnHomeAction::ReturnPawnHome()
{
    APawn* Pawn = TargetPawn.Get();
    ATargetPoint* Destination = TargetPoint.Get();
    if (!IsValid(Pawn) || !IsValid(Destination)) {
        UE_LOG(LogTemp, Warning, TEXT("ACF ReturnPawnHome: pawn or destination is not loaded. Pawn=%s Destination=%s"),
            *TargetPawn.ToSoftObjectPath().ToString(), *TargetPoint.ToSoftObjectPath().ToString());
        return false;
    }

    if (!Pawn->HasAuthority() || Pawn->GetWorld() != Destination->GetWorld()) {
        UE_LOG(LogTemp, Warning, TEXT("ACF ReturnPawnHome: requires authority and actors in the same world."));
        return false;
    }

    AACFAIController* AIController = Cast<AACFAIController>(Pawn->GetController());
    if (!IsValid(AIController)) {
        UE_LOG(LogTemp, Warning, TEXT("ACF ReturnPawnHome: %s has no ACFAIController."), *GetNameSafe(Pawn));
        return false;
    }

    const FGameplayTag ReturnHomeState = UGameplayTagsManager::Get().RequestGameplayTag(ACF::AIReturnHome, false);
    if (!ReturnHomeState.IsValid()) {
        UE_LOG(LogTemp, Error, TEXT("ACF ReturnPawnHome: AIState.ReturnHome is not registered."));
        return false;
    }

    // SetHomeLocation also refreshes the blackboard destination when already returning home.
    AIController->SetHomeLocation(Destination->GetActorLocation());
    AIController->SetCurrentAIState(ReturnHomeState);
    return true;
}
