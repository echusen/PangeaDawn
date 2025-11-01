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

void UPangeaTamingAbility::OnActionStarted_Implementation()
{
	Super::OnActionStarted_Implementation();

	FGameplayTagContainer TagsToAdd;
	TagsToAdd.AddTag(FGameplayTag::RequestGameplayTag("Tame.Attempt.InProgress"));
	
	UAbilitySystemBlueprintLibrary::AddLooseGameplayTags(GetAvatarActorFromActorInfo(), TagsToAdd);
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

	TamingComp->StartMinigame(GetAvatarActorFromActorInfo(), TargetActor,
	TamingComp->TameSpeciesConfig ? TamingComp->TameSpeciesConfig->TameDuration : 5.f);
}
