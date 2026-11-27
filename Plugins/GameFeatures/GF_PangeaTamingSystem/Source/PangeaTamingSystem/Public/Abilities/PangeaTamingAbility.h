// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Actions/ACFActionAbility.h"
#include "PangeaTamingAbility.generated.h"

class APDDinosaurBase;

/**
 * ACF action ability that starts a taming attempt on the actor the avatar is interacting with.
 * It tags the avatar while the action runs and hands over to the target's taming minigame when the action ends.
 */
UCLASS()
class PANGEATAMINGSYSTEM_API UPangeaTamingAbility : public UACFActionAbility
{
	GENERATED_BODY()

protected:
	/** Actor being tamed, captured from the avatar's interaction component in PreActivate. Cleared on cancel. */
	UPROPERTY()
	TObjectPtr<AActor> TargetActor;

	virtual void PreActivate(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		FOnGameplayAbilityEnded::FDelegate* OnGameplayAbilityEndedDelegate,
		const FGameplayEventData* TriggerEventData = nullptr) override;
	virtual void OnActionEnded_Implementation() override;
	virtual void OnActionStarted_Implementation() override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;
};
