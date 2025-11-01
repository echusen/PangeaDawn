// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Actions/ACFActionAbility.h"
#include "PangeaTamingAbility.generated.h"

class APDDinosaurBase;
/**
 * 
 */
UCLASS()
class PANGEATAMINGSYSTEM_API UPangeaTamingAbility : public UACFActionAbility
{
	GENERATED_BODY()
	
protected:
	UPROPERTY()
	TObjectPtr<AActor> TargetActor;

	virtual void PreActivate(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, FOnGameplayAbilityEnded::FDelegate* OnGameplayAbilityEndedDelegate, const FGameplayEventData* TriggerEventData = nullptr) override;
	virtual void OnActionEnded_Implementation() override;
	virtual void OnActionStarted_Implementation() override;
};
