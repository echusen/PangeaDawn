// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "AbilityTask_WaitForTameResult.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTameResultDelegate, bool, bSuccess);

/**
 * Waits for the taming minigame to complete (success or fail).
 * Exposes a Blueprint function to feed the result back into GAS.
 */
UCLASS()
class PANGEATAMINGSYSTEM_API UAbilityTask_WaitForTameResult : public UAbilityTask
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable)
	FTameResultDelegate OnTameResult;

	UFUNCTION(BlueprintCallable, Category="Ability|Tasks", meta=(HidePin="OwningAbility", DefaultToSelf="OwningAbility"))
	static UAbilityTask_WaitForTameResult* WaitForTameResult(UGameplayAbility* OwningAbility);

	UFUNCTION(BlueprintCallable, Category="Ability|Tasks")
	void NotifyTameResult(bool bWasSuccessful);

protected:
	virtual void Activate() override;

private:
	bool bHasResult = false;
	
};
