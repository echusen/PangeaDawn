// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.

#include "ACFComboAttackAction.h"
#include "ACFComboComponent.h"
#include "ARSStatisticsComponent.h"
#include "Graph/ACFComboGraph.h"
#include "Graph/ACFComboNode.h"
#include "Logging.h"

void UACFComboAttackAction::OnActionStarted_Implementation()
{
    if (Combo && GetCharacterOwner()) {
        comboComponent = GetCharacterOwner()->FindComponentByClass<UACFComboComponent>();
        if (comboComponent) {

            node = Combo->GetCurrentComboNode();

            if (node) {
               // DamageToActivate = node->GetDamageToActivate();
               // TraceChannels = node->GetTraceChannels();
                UARSStatisticsComponent* statComp = GetCharacterOwner()->FindComponentByClass<UARSStatisticsComponent>();
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
    if (node && comboComponent) {
        UARSStatisticsComponent* statComp = GetCharacterOwner()->FindComponentByClass<UARSStatisticsComponent>();
        if (statComp) {
            statComp->RemoveAttributeSetModifier(geHandle);
        }
        bSuccesfulCombo = comboComponent->HasPendingInput();
        if (bSuccesfulCombo && Combo->PerformTransition(comboComponent->GetLastTagInput(), GetCharacterOwner())) {
            ActionsManager->StoreAbilityInBuffer(GetActionTag());
        } else {
            comboComponent->StopCombo(Combo);
            ActionsManager->StoreAbilityInBuffer(FGameplayTag()); // we clean the input buffer
        }
    }

    Super::OnActionEnded_Implementation();
}

UACFComboAttackAction::UACFComboAttackAction()
{
    bAutoOpenBuffer = true;
}

void UACFComboAttackAction::PreActivate(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, FOnGameplayAbilityEnded::FDelegate* OnGameplayAbilityEndedDelegate, const FGameplayEventData* TriggerEventData)
{
    Super::PreActivate(Handle, ActorInfo, ActivationInfo, OnGameplayAbilityEndedDelegate, TriggerEventData);
    if (Combo && GetCharacterOwner()) {
        comboComponent = GetCharacterOwner()->FindComponentByClass<UACFComboComponent>();
        if (comboComponent) {
            if (!comboComponent->IsExecutingCombo(Combo)) {
                comboComponent->StartCombo(Combo, GetActionTag());
            }
            comboComponent->ClearInputTag();
            node = Combo->GetCurrentComboNode();

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
    node = Combo->GetCurrentComboNode();
    if (node && comboComponent) {
        if (bAutoOpenBuffer) {
            comboComponent->SetInputBufferOpened(false);
        }
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
