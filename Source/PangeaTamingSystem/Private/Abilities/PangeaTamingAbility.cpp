// Fill out your copyright notice in the Description page of Project Settings.


#include "Abilities/PangeaTamingAbility.h"

#include "Components/ACFInteractionComponent.h"
#include "Components/PangeaTamingComponent.h"
#include "DataAssets/TameSpeciesConfig.h"


void UPangeaTamingAbility::PreActivate(const FGameplayAbilitySpecHandle Handle,
                                       const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
                                       FOnGameplayAbilityEnded::FDelegate* OnGameplayAbilityEndedDelegate, const FGameplayEventData* TriggerEventData)
{
	Super::PreActivate(Handle, ActorInfo, ActivationInfo, OnGameplayAbilityEndedDelegate, TriggerEventData);

	TargetActor = ActorInfo->AvatarActor->GetComponentByClass<UACFInteractionComponent>()->GetCurrentBestInteractableActor();
}

void UPangeaTamingAbility::OnActionEnded_Implementation()
{
	Super::OnActionEnded_Implementation();

	if (!TargetActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[TamingAbility] OnActionEnded: No valid target to tame."));
		return;
	}

	UPangeaTamingComponent* TamingComp = TargetActor->FindComponentByClass<UPangeaTamingComponent>();
	if (!TamingComp)
	{
		UE_LOG(LogTemp, Warning, TEXT("[TamingAbility] OnActionEnded: No TamingComponent on %s"),
			*GetNameSafe(TargetActor));
		return;
	}

	// Wait for minigame result instead of instantly resolving
	ActiveTameTask = UAbilityTask_WaitForTameResult::WaitForTameResult(this);
	ActiveTameTask->OnTameResult.AddDynamic(this, &UPangeaTamingAbility::HandleTameResult);
	ActiveTameTask->ReadyForActivation();

	TamingComp->BeginTameMinigame(this, ActiveTameTask);
}

void UPangeaTamingAbility::HandleTameResult(bool bSuccess)
{
	if (!TargetActor)
		return;

	UPangeaTamingComponent* TamingComp = TargetActor->FindComponentByClass<UPangeaTamingComponent>();
	if (!TamingComp)
		return;

	if (bSuccess)
	{
		TamingComp->OnTameResolved(true, TamingComp->GetTamedRole());
		UE_LOG(LogTemp, Log, TEXT("[TamingAbility] Taming succeeded for %s"), *GetNameSafe(TargetActor));
	}
	else
	{
		TamingComp->HandleTameFailed(TEXT("Minigame failed"));
		UE_LOG(LogTemp, Log, TEXT("[TamingAbility] Taming failed for %s"), *GetNameSafe(TargetActor));
	}

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, !bSuccess);
	ActiveTameTask = nullptr;
}
