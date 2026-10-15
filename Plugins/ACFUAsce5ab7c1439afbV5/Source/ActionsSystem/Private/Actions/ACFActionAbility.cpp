// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "Actions/ACFActionAbility.h"
#include "ACFActionsFunctionLibrary.h"
#include "ACFRPGFunctionLibrary.h"
#include "ACMEffectsDispatcherComponent.h"
#include "AIController.h"
#include "ARSFunctionLibrary.h"
#include "ACFGASStatisticsComponent.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/GameState.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/GameStateBase.h"
#include "GameplayEffectTypes.h"
#include "GameplayEffects/ACFDefaultCooldownGE.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include <Abilities/GameplayAbility.h>
#include <Abilities/GameplayAbilityTypes.h>
#include <BehaviorTree/BehaviorTreeComponent.h>
#include <GameFramework/CharacterMovementComponent.h>
#include <GameplayTagsManager.h>
#include <Kismet/GameplayStatics.h>
#include <Logging.h>
#include <Net/UnrealNetwork.h>
#include <TimerManager.h>

UACFActionAbility::UACFActionAbility()
{
	CooldownGameplayEffectClass = UACFDefaultCooldownGE::StaticClass();
	bBindActionToAnimation = true;

}

void UACFActionAbility::PreActivate(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	FOnGameplayAbilityEnded::FDelegate* OnGameplayAbilityEndedDelegate, const FGameplayEventData* TriggerEventData)
{
	Super::PreActivate(Handle, ActorInfo, ActivationInfo, OnGameplayAbilityEndedDelegate, TriggerEventData);

	// Runs on BOTH client and server at every activation (Internal_OnActivated is authority-only).
	// Abilities are InstancedPerActor, so without this the flag raised by the exit notify would
	// survive into the next predicted activation on the client and cut it short immediately.
	bExitRequested = false;

	if (CharacterOwner && ActionConfig.bStopBehavioralThree) {
		const AAIController* contr = Cast<AAIController>(CharacterOwner->GetController());
		if (contr) {
			UBehaviorTreeComponent* behavComp = contr->FindComponentByClass<UBehaviorTreeComponent>();
			if (behavComp) {
				behavComp->PauseLogic("Blocking Action");
			}
		}
	}
}





void UACFActionAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (GetCharacterOwner())
	{
		if (UCharacterMovementComponent* charMov = GetCharacterOwner()->GetCharacterMovement()) {
			if (bAllowPhysicalRotation != charMov->bAllowPhysicsRotationDuringAnimRootMotion) {
				bOldAllowPhysicsRotation = charMov->bAllowPhysicsRotationDuringAnimRootMotion;
				charMov->bAllowPhysicsRotationDuringAnimRootMotion = bAllowPhysicalRotation;
			}
		}
	}

	// Character avatars use CharacterOwner; other pawn avatars (vehicles etc.) fall back to the avatar pawn
	APawn* pawnOwner = CharacterOwner ? static_cast<APawn*>(CharacterOwner.Get()) : Cast<APawn>(GetAvatarActorFromActorInfo());
	if (ACFAbilityComponent && pawnOwner) {
		if (pawnOwner->HasAuthority()) {
			// Always execute server-side logic
			Internal_OnActivated(ACFAbilityComponent, animMontage);

			// In standalone / listen server, this pawn is also locally controlled
			if (pawnOwner->IsLocallyControlled()) {
				// Execute local feedback logic too
				ClientsOnActionStarted();
			}
		}
		else {
			// Owning client in multiplayer
			ClientsOnActionStarted();
		}
	}

	if (ActionConfig.bPlayEffectOnActionStart) {
		PlayEffects();
	}
}



void UACFActionAbility::SetMontageReproductionType(EMontageReproductionType reproType)
{
	ActionConfig.MontageReproductionType = reproType;
}

void UACFActionAbility::Internal_OnActivated(class UACFAbilitySystemComponent* actionManger, class UAnimMontage* inAnimMontage)
{
	bExitRequested = false;
	OnActionStarted();
}

void UACFActionAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	// Same fallback as ActivateAbility: support pawn avatars without a CharacterOwner
	const APawn* pawnOwner = CharacterOwner ? static_cast<const APawn*>(CharacterOwner.Get()) : Cast<APawn>(GetAvatarActorFromActorInfo());
	if (pawnOwner && HasAuthority(&ActivationInfo)) {
		Internal_OnDeactivated();
		// In standalone / listen server, this pawn is also locally controlled
		if (pawnOwner->IsLocallyControlled()) {
			// Execute local feedback logic too
			ClientsOnActionEnded();
		}
	}
	else {
		ClientsOnActionEnded();
	}

	RestorePhysicalRotationDuringAnimRootMotion();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

bool UACFActionAbility::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags, OUT FGameplayTagContainer* OptionalRelevantTags) const
{
	if (ActorInfo->AvatarActor->IsValidLowLevelFast()) {
		const UCharacterMovementComponent* moveComp = ActorInfo->AvatarActor->FindComponentByClass<UCharacterMovementComponent>();
		if (moveComp) {
			if (!ActionConfig.PerformableInMovementModes.Contains(moveComp->MovementMode)) {
				UE_LOG(ACFLog, Warning, TEXT("Actions Can't be exectuted while in air!"));
				return false;
			}
		}
	}
	return Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);// && CanExecuteAction(Cast<ACharacter>(ActorInfo->OwnerActor));
}



void UACFActionAbility::HandleGameplayEventReceived(FGameplayEventData Payload)
{
	Super::HandleGameplayEventReceived(Payload);
	if (!CharacterOwner) {
		return;
	}
	if (Payload.EventTag.MatchesTagExact(UGameplayTagsManager::Get().RequestGameplayTag(ACF::ExitTag))) {
		// Re-enable physics rotation before the montage finishes so the character can turn during the tail.
		RestorePhysicalRotationDuringAnimRootMotion();

		// Mark that the exit notify has fired on the server.
		// Late-arriving combo input RPCs will check this flag to know they can exit immediately.
		bExitRequested = true;

		if (ACFAbilityComponent->HasStoredActions()) {
			// if there are other actions in the queue, we just exit this one
			ExitAction(false);
		}
		else {
			// otherwise just put this ability on minimum priority so that can be interrupted by any other
			ACFAbilityComponent->SetCurrentPriority(-1);
		}
	}
	else if (Payload.EventTag.MatchesTagExact(UGameplayTagsManager::Get().RequestGameplayTag(ACF::NotableTag))) {
		if (HasAuthority(&activationInfo)) {
			// In standalone / listen server, this pawn may also be locally controlled
			if (CharacterOwner->IsLocallyControlled()) {
				// Execute local feedback logic too
				ClientsOnNotablePointReached();
			}
		}
		else {
			ClientsOnNotablePointReached();
		}
	}


}



bool UACFActionAbility::CommitAbilityCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const bool ForceCooldown, OUT FGameplayTagContainer* OptionalRelevantTags /*= nullptr*/)
{
	if (UsingDefaultCooldown()) {
		if (!ActivationBlockedTags.HasTagExact(GetTriggeringTag())) {
			ActivationBlockedTags.AddTag(GetTriggeringTag());
		}
		if (!cooldownTags.HasTagExact(GetTriggeringTag())) {
			cooldownTags.AddTag(GetTriggeringTag());
		}
	}
	return Super::CommitAbilityCooldown(Handle, ActorInfo, ActivationInfo, ForceCooldown, OptionalRelevantTags);
}

void UACFActionAbility::ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	// SetByCallerFromConfig: duration comes from ActionConfig.CoolDownTime.
	// Custom GE: apply CooldownGameplayEffectClass as-is via Super (CoolDownTime ignored).
	if (UsingDefaultCooldown()) {
		// CoolDownTime <= 0 means no cooldown — do not call Super. UACFDefaultCooldownGE is
		// HasDuration with unset magnitude (duration 0, no expiry timer, Stacking=None),
		// so each EndAbility would leave a permanent ActiveGE that never expires.
		if (ActionConfig.CoolDownTime <= 0.f) {
			return;
		}
		FGameplayEffectSpecHandle CooldownSpec = ACFAbilityComponent->MakeOutgoingSpec(CooldownGameplayEffectClass, GetAbilityLevel(Handle, ActorInfo), ACFAbilityComponent->MakeEffectContext());
		CooldownSpec.Data->DynamicGrantedTags.AddTag(GetTriggeringTag());
		CooldownSpec.Data->SetDuration(ActionConfig.CoolDownTime, true);
		ACFAbilityComponent->ApplyGameplayEffectSpecToSelf(*CooldownSpec.Data.Get());
	}
	else {
		Super::ApplyCooldown(Handle, ActorInfo, ActivationInfo);
	}
}


bool UACFActionAbility::UsingDefaultCooldown() const
{
	// Default cooldown = SetByCallerFromConfig (CoolDownTime) AND still on UACFDefaultCooldownGE.
	// Custom GE type, or a non-default CooldownGameplayEffectClass, uses Super as-is.
	return ActionConfig.CoolDownGEType == EGEType::ESetByCallerFromConfig
		&& CooldownGameplayEffectClass == UACFDefaultCooldownGE::StaticClass();
}

const FGameplayTagContainer* UACFActionAbility::GetCooldownTags() const
{
	if (UsingDefaultCooldown()) {
		if (ActionConfig.CoolDownTime <= 0.f) {
			// Static empty container - no allocation, always valid
			static const FGameplayTagContainer EmptyContainer;
			return &EmptyContainer;
		}
		return &cooldownTags;
	}
	return Super::GetCooldownTags();
}

bool UACFActionAbility::CheckCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, OUT FGameplayTagContainer* OptionalRelevantTags /*= nullptr*/) const
{
	if (UsingDefaultCooldown() && ActionConfig.CoolDownTime <= 0) {
		return true;
	}
	return Super::CheckCooldown(Handle, ActorInfo, OptionalRelevantTags);
}

void UACFActionAbility::RestorePhysicalRotationDuringAnimRootMotion()
{
	if (GetCharacterOwner()) {
		if (UCharacterMovementComponent* charMov = GetCharacterOwner()->GetCharacterMovement()) {
			if (charMov->bAllowPhysicsRotationDuringAnimRootMotion != bOldAllowPhysicsRotation) {
				charMov->bAllowPhysicsRotationDuringAnimRootMotion = bOldAllowPhysicsRotation;
			}
		}
	}
}

void UACFActionAbility::ExitAction(bool bCancelled)
{
	EndAbility(selfHandle, &actorInfo, activationInfo, true, bCancelled);
}

void UACFActionAbility::HandleMontageFinished()
{
	Super::HandleMontageFinished();
	//ExitAction(false);
}

void UACFActionAbility::HandleMontageInterrupted()
{
	Super::HandleMontageInterrupted();
	//ExitAction(true);
}

void UACFActionAbility::Internal_OnDeactivated()
{

	// reset warp info
	if (ActionConfig.bStopBehavioralThree && CharacterOwner) {
		const AAIController* aiContr = Cast<AAIController>(CharacterOwner->GetController());
		if (aiContr) {
			UBehaviorTreeComponent* behavComp = aiContr->FindComponentByClass<UBehaviorTreeComponent>();
			if (behavComp) {
				behavComp->ResumeLogic("Blocking Action");
			}
		}
	}

	OnActionEnded();
}

void UACFActionAbility::SetActionConfig(const FActionConfig& newConfig)
{
	ActionConfig = newConfig;
}



/* TO BE IMPLEMENTED IN CHILD CLASSES!*/
void UACFActionAbility::OnActionStarted_Implementation()
{
}

void UACFActionAbility::ClientsOnActionStarted_Implementation()
{
}

void UACFActionAbility::OnActionEnded_Implementation()
{
}

void UACFActionAbility::ClientsOnActionEnded_Implementation()
{
}



bool UACFActionAbility::CanExecuteAction_Implementation(class ACharacter* owner) const
{
	return true;
}


void UACFActionAbility::ClientsOnNotablePointReached_Implementation()
{
}
