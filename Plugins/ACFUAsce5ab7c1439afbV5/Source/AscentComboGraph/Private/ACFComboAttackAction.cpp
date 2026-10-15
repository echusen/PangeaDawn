// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "ACFComboAttackAction.h"
#include "ACFComboComponent.h"
#include "ACFGASStatisticsComponent.h"
#include "Graph/ACFComboGraph.h"
#include "Graph/ACFComboNode.h"
#include "Logging.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"
#include "TimerManager.h"

bool UACFComboAttackAction::CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags,
    const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
    // [Multi-input combo fix] While a combo is running, reject any combo-ability (re)activation that
    // is NOT the combo's own authorized continuation. This stops a second combo ability (e.g. Heavy)
    // pressed mid-string from interrupting the in-flight combo. The input still reaches the combo
    // component's buffer and drives the graph transition; only the redundant ability activation is
    // blocked. The opener (no combo running yet) and each authorized continuation pass through.
    const ACharacter* Character = ActorInfo ? Cast<ACharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
    const UACFComboComponent* ActiveComboComponent =
        Character ? Character->FindComponentByClass<UACFComboComponent>() : nullptr;
    const bool bInternalContinuation = ActiveComboComponent
        && ActiveComboComponent->IsNextNodeActivationAuthorized(GetActionTag());
    if (ActiveComboComponent && ActiveComboComponent->IsExecutingAnyCombo() && !bInternalContinuation) {
        return false;
    }
    return Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);
}

bool UACFComboAttackAction::CheckCooldown(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags) const
{
    // [Multi-input combo fix] The authorized continuation must bypass the opener's cooldown, or the
    // graph's own re-activation would be blocked by CheckCooldown.
    const ACharacter* Character = ActorInfo ? Cast<ACharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
    const UACFComboComponent* ActiveComboComponent =
        Character ? Character->FindComponentByClass<UACFComboComponent>() : nullptr;
    if (ActiveComboComponent && ActiveComboComponent->IsNextNodeActivationAuthorized(GetActionTag())) {
        return true;
    }
    return Super::CheckCooldown(Handle, ActorInfo, OptionalRelevantTags);
}

void UACFComboAttackAction::OnActionStarted_Implementation()
{
    if (Combo && GetCharacterOwner()) {
        comboComponent = GetCharacterOwner()->FindComponentByClass<UACFComboComponent>();
        if (comboComponent) {
            // Use the runtime instance (DuplicateObject), not the source asset
            if (UACFComboGraph* activeCombo = comboComponent->GetCurrentCombo()) {
                node = activeCombo->GetCurrentComboNode();
            }

            if (node) {
                UACFGASStatisticsComponent* statComp = GetCharacterOwner()->FindComponentByClass<UACFGASStatisticsComponent>();
                if (statComp) {
                    geHandle = statComp->AddAttributeSetModifier(node->GetComboNodeModifier());
                }
            }
        }
    }
    Super::OnActionStarted_Implementation();
}

void UACFComboAttackAction::OnActionEnded_Implementation()
{
    ProcessComboStepEnd();

    Super::OnActionEnded_Implementation();
}

UACFComboAttackAction::UACFComboAttackAction()
{
    bAutoOpenBuffer = true;
}

void UACFComboAttackAction::PreActivate(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, FOnGameplayAbilityEnded::FDelegate* OnGameplayAbilityEndedDelegate, const FGameplayEventData* TriggerEventData)
{
    Super::PreActivate(Handle, ActorInfo, ActivationInfo, OnGameplayAbilityEndedDelegate, TriggerEventData);
    bComboStepEndProcessed = false;
    if (Combo && GetCharacterOwner()) {
        comboComponent = GetCharacterOwner()->FindComponentByClass<UACFComboComponent>();
        if (comboComponent) {
            const bool bFreshCombo = !comboComponent->IsExecutingCombo(Combo);
            if (bFreshCombo) {
                comboComponent->StartCombo(Combo, GetActionTag());
            }
            // [Multi-input combo fix] reset the per-node transition window and spend the single-use
            // continuation token (if this activation IS the combo's authorized continuation).
            comboComponent->BeginComboNode();
            comboComponent->ConsumeNextNodeActivationAuthorization(GetActionTag());
            // Use the runtime instance (DuplicateObject), not the source asset
            if (UACFComboGraph* activeCombo = comboComponent->GetCurrentCombo()) {
                node = activeCombo->GetCurrentComboNode();
            }

            if (!node || !IsValid(node) || !node->IsValidLowLevel()) {
                UE_LOG(ACFLog, Error, TEXT("Invalid Combo Transition in %s! Node: %s, Combo: %s"),
                    *GetNameSafe(this),
                    node ? *GetNameSafe(node) : TEXT("nullptr"),
                    *GetNameSafe(Combo));

                // First stop the combo to prevent recursion
                //This is a fallback, combo should stop at the end of the ability
                if (comboComponent && comboComponent->IsExecutingCombo(Combo)) {
                    comboComponent->StopCombo(Combo);
                }

                ExitAction();
                return;
            }
            if (bAutoOpenBuffer) {
                comboComponent->SetInputBufferOpened(true);
            }

            // [Multi-input combo fix] On a FRESH combo start, defer one tick then clear the buffer, so
            // the opener's own press (which can buffer on the same frame it starts the combo) cannot
            // seed the first transition. Continuation presses (made later) buffer normally; this does
            // not fire on continuations.
            if (bFreshCombo) {
                if (UWorld* World = GetWorld()) {
                    TWeakObjectPtr<UACFComboComponent> WeakCombo = comboComponent;
                    World->GetTimerManager().SetTimerForNextTick([WeakCombo]() {
                        if (WeakCombo.IsValid()) {
                            WeakCombo->ClearBufferedInputs();
                        }
                    });
                }
            }

            if (node) {
                // FString MontageName = node ? node->GetMontage()->GetName() : "None";
                SetAnimMontage(node->GetMontage());
            }
        }
    } else {
        ExitAction(true);
    }
}

void UACFComboAttackAction::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    // Use the runtime instance (DuplicateObject), not the source asset
    if (comboComponent) {
        if (UACFComboGraph* activeCombo = comboComponent->GetCurrentCombo()) {
            node = activeCombo->GetCurrentComboNode();
        }
    }
    if (node && comboComponent) {
        if (bAutoOpenBuffer) {
            comboComponent->SetInputBufferOpened(false);
        }
    }

    if (!HasAuthority(&ActivationInfo)) {
        ProcessComboStepEnd();
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UACFComboAttackAction::ProcessComboStepEnd()
{
    if (bComboStepEndProcessed) {
        return;
    }
    bComboStepEndProcessed = true;

    if (!comboComponent || !GetCharacterOwner()) {
        return;
    }

    if (!node) {
        if (UACFComboGraph* activeCombo = comboComponent->GetCurrentCombo()) {
            node = activeCombo->GetCurrentComboNode();
        }
    }

    if (!node) {
        return;
    }

    if (geHandle.IsValid()) {
        UACFGASStatisticsComponent* statComp = GetCharacterOwner()->FindComponentByClass<UACFGASStatisticsComponent>();
        if (statComp) {
            statComp->RemoveAttributeSetModifier(geHandle);
        }
        geHandle.Invalidate();
    }

    // [Multi-input combo fix] Consume the best buffered input for THIS node from the node-scoped
    // buffer (replaces the single-slot HasPendingInput()/GetLastTagInput()). On success, authorize the
    // combo's OWN re-activation so CanActivateAbility lets the continuation through, then re-fire it via
    // the existing store-then-launch path.
    FGameplayTag ConsumedInput;
    bSuccesfulCombo = comboComponent->TryConsumeBufferedTransition(GetCharacterOwner(), ConsumedInput);
    if (bSuccesfulCombo) {
        comboComponent->AuthorizeNextNodeActivation(GetActionTag());
        ACFAbilityComponent->StoreAbilityInBuffer(GetActionTag());
    } else {
        comboComponent->StopCombo(Combo);
        ACFAbilityComponent->StoreAbilityInBuffer(FGameplayTag());
    }
}
