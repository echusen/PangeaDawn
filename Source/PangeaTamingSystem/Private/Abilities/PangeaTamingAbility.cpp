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

	if (UPangeaTamingComponent* TamingComp = TargetActor ? TargetActor->FindComponentByClass<UPangeaTamingComponent>() : nullptr)
	{
		TamingComp->OnTameResolved(true, ETamedRole::None);
	}
}

void UPangeaTamingAbility::OnGameplayEventReceived_Implementation(const FGameplayTag EventTag)
{
	Super::OnGameplayEventReceived_Implementation(EventTag);

	if (EventTag.MatchesTagExact(FGameplayTag::RequestGameplayTag("Tame.Event.Target")))
	{
		UE_LOG(LogTemp, Warning, TEXT("DATA GOT"));
	}
}
