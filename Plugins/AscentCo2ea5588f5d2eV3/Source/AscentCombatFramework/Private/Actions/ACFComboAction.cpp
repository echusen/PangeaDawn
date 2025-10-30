// // Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.

#include "Actions/ACFComboAction.h"
#include "Actions/ACFActionAbility.h"
#include <Abilities/Tasks/AbilityTask_WaitGameplayEvent.h>
#include <GameplayTask.h>

void UACFComboAction::OnActionStarted_Implementation()
{
    bSuccesfulCombo = false;

    Super::OnActionStarted_Implementation();
}

FName UACFComboAction::GetMontageSectionName_Implementation()
{
    if (animMontage) {
        const int32 currentCount = GetActionsManager()->GetComboCount(ActionTag);
        const FName SectionName = animMontage->GetSectionName(currentCount);

        if (SectionName != NAME_None) {
            return SectionName;
        } else {
            return animMontage->GetSectionName(0);
        }
    }
    return NAME_None;
}

void UACFComboAction::OnActionEnded_Implementation()
{
    Super::OnActionEnded_Implementation();

    if (!bSuccesfulCombo) {
        ResetComboCounter();
    }
}

void UACFComboAction::ResetComboCounter()
{
    GetActionsManager()->ResetComboCount(ActionTag);
    bSuccesfulCombo = false;
}

void UACFComboAction::SendComboInput()
{
    if (bSuccesfulCombo) {
        // we can increment the counter only once per attack
        return;
    }
    if (!animMontage) {
        return;
    }

    int32 currentCount = GetComboCounter();
    if (currentCount + 1 >= animMontage->CompositeSections.Num()) {
        GetActionsManager()->ResetComboCount(ActionTag);
    } else {
        GetActionsManager()->SetComboCounter(ActionTag, ++currentCount);
    }
    bSuccesfulCombo = true;
}

void UACFComboAction::OnGameplayEventReceived_Implementation(const FGameplayTag eventTag)
{
    if (eventTag.MatchesTag(ActionTag)) {
        SendComboInput();
        bSuccesfulCombo = true;
    }
}

UACFComboAction::UACFComboAction()
{
}
