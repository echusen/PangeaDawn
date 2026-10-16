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

	TargetActor = nullptr;
	if (ActorInfo && ActorInfo->AvatarActor.IsValid())
	{
		if (UACFInteractionComponent* Interaction = ActorInfo->AvatarActor->FindComponentByClass<UACFInteractionComponent>())
		{
			// The best nearby candidate may change while the interaction starts.
			TargetActor = Interaction->GetCurrentInteractingActor();
			if (!IsValid(TargetActor))
				TargetActor = Interaction->GetCurrentBestInteractableActor();
		}
	}
}

void UPangeaTamingAbility::OnActionStarted_Implementation()
{
	Super::OnActionStarted_Implementation();

	FGameplayTagContainer TagsToAdd;
	const UPangeaTamingComponent* Taming = IsValid(TargetActor) ? TargetActor->FindComponentByClass<UPangeaTamingComponent>() : nullptr;
	const FGameplayTag ProgressTag = Taming && Taming->TameSpeciesConfig
		? Taming->TameSpeciesConfig->InProgressTag : FGameplayTag();
	if (ProgressTag.IsValid())
		TagsToAdd.AddTag(ProgressTag);
	
	UAbilitySystemBlueprintLibrary::AddLooseGameplayTags(GetAvatarActorFromActorInfo(), TagsToAdd);
}

void UPangeaTamingAbility::OnActionEnded_Implementation()
{
	Super::OnActionEnded_Implementation();

	if (!IsValid(TargetActor))
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

	TamingComp->StartMinigame(GetAvatarActorFromActorInfo(), TargetActor,
	TamingComp->TameSpeciesConfig ? TamingComp->TameSpeciesConfig->TameDuration : 5.f);
}

void UPangeaTamingAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (bWasCancelled)
	{
		if (UPangeaTamingComponent* Taming = IsValid(TargetActor) ? TargetActor->FindComponentByClass<UPangeaTamingComponent>() : nullptr)
		{
			FGameplayTagContainer Tags;
			if (Taming->TameSpeciesConfig && Taming->TameSpeciesConfig->InProgressTag.IsValid())
				Tags.AddTag(Taming->TameSpeciesConfig->InProgressTag);
			UAbilitySystemBlueprintLibrary::RemoveLooseGameplayTags(GetAvatarActorFromActorInfo(), Tags);
		}
		// Super invokes OnActionEnded even on cancellation. Suppress the minigame.
		TargetActor = nullptr;
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
