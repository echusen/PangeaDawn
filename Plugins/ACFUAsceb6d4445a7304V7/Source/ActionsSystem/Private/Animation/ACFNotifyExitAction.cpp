// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "Animation/ACFNotifyExitAction.h"
#include <Components/SkeletalMeshComponent.h>
#include "Components/ACFAbilitySystemComponent.h"
#include <GameFramework/Pawn.h>
#include <GameplayTagsManager.h>

void UACFNotifyExitAction::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
    if (!MeshComp || !MeshComp->GetOwner()) {
        return;
    }

    AActor* owner = MeshComp->GetOwner();
    APawn* pawnOwner = Cast<APawn>(owner);

    // The exit point must be evaluated on every machine that owns a (predicted) instance of
    // the running ability: the server, AND the owning client for its locally controlled pawn.
    // Actions are LocalPredicted and the input buffer lives on the client, so if only the
    // server fired this event the client would never exit early and every buffered action
    // (combo chains above all) would start at montage blend out instead of at the exit point.
    // Simulated proxies are skipped: they replay the montage but hold no ability instance.
    const bool bIsPredictingOwner = pawnOwner && pawnOwner->IsLocallyControlled();
    if (!owner->HasAuthority() && !bIsPredictingOwner) {
        return;
    }

    UACFAbilitySystemComponent* actionsManager = owner->FindComponentByClass<UACFAbilitySystemComponent>();
    if (actionsManager) {
        actionsManager->TriggerGameplayEvent(UGameplayTagsManager::Get().RequestGameplayTag(ACF::ExitTag, true));
    }
}
