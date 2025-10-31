// Fill out your copyright notice in the Description page of Project Settings.


#include "GameplayTasks/AbilityTask_WaitForTameResult.h"

UAbilityTask_WaitForTameResult* UAbilityTask_WaitForTameResult::WaitForTameResult(UGameplayAbility* OwningAbility)
{
	return NewAbilityTask<UAbilityTask_WaitForTameResult>(OwningAbility);
}

void UAbilityTask_WaitForTameResult::Activate()
{
	bHasResult = false;
}

void UAbilityTask_WaitForTameResult::NotifyTameResult(bool bWasSuccessful)
{
	if (bHasResult) return;
	bHasResult = true;

	OnTameResult.Broadcast(bWasSuccessful);
	EndTask();
}


