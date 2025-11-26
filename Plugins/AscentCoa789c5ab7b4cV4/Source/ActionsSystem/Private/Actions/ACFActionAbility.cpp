// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2025. All Rights Reserved.

#include "Actions/ACFActionAbility.h"
#include "ACFActionsFunctionLibrary.h"
#include "ACFRPGFunctionLibrary.h"
#include "ACMEffectsDispatcherComponent.h"
#include "AIController.h"
#include "ARSFunctionLibrary.h"
#include "ARSStatisticsComponent.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/GameState.h"
#include "GameFramework/GameStateBase.h"
#include "GameplayEffectTypes.h"
#include "GameplayEffects/ACFDefaultCooldownGE.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "MotionWarpingComponent.h"
#include "RootMotionModifier.h"
#include "RootMotionModifier_SkewWarp.h"
#include <Abilities/GameplayAbility.h>
#include <Abilities/GameplayAbilityTypes.h>
#include <Abilities/Tasks/AbilityTask_PlayMontageAndWait.h>
#include <Abilities/Tasks/AbilityTask_WaitGameplayEvent.h>
#include <BehaviorTree/BehaviorTreeComponent.h>
#include <GameFramework/CharacterMovementComponent.h>
#include <GameplayTagsManager.h>
#include <Kismet/GameplayStatics.h>
#include <Logging.h>
#include <Net/UnrealNetwork.h>
#include <TimerManager.h>

UACFActionAbility::UACFActionAbility()
{
	bBindActionToAnimation = true;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	ReplicationPolicy = EGameplayAbilityReplicationPolicy::ReplicateYes;
	CooldownGameplayEffectClass = UACFDefaultCooldownGE::StaticClass();
	bFullyInit = false;
}

void UACFActionAbility::PreActivate(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	FOnGameplayAbilityEnded::FDelegate* OnGameplayAbilityEndedDelegate, const FGameplayEventData* TriggerEventData)
{
	Super::PreActivate(Handle, ActorInfo, ActivationInfo, OnGameplayAbilityEndedDelegate, TriggerEventData);

	selfHandle = Handle;
	actorInfo = *ActorInfo;
	activationInfo = ActivationInfo;
	CharacterOwner = Cast<ACharacter>(ActorInfo->AvatarActor.Get());
	ExtractPayloadFromEvent(TriggerEventData);



	UAbilityTask_WaitGameplayEvent* WaitExitEvent = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, FGameplayTag(), nullptr, false, false);
	WaitExitEvent->EventReceived.AddDynamic(this, &UACFActionAbility::HandleGameplayEventReceived);
	WaitExitEvent->ReadyForActivation();

	if (!IsFullyInit()) {
		InitAbility();
	}
	if (ActionConfig.bStopBehavioralThree) {
		const AAIController* contr = Cast<AAIController>(CharacterOwner->GetController());
		if (contr) {
			UBehaviorTreeComponent* behavComp = contr->FindComponentByClass<UBehaviorTreeComponent>();
			if (behavComp) {
				behavComp->PauseLogic("Blocking Action");
			}
		}
	}
}

void UACFActionAbility::ExtractPayloadFromEvent(const FGameplayEventData* TriggerEventData)
{
	StoredPayload = FACFAbilityPayload();

	if (!TriggerEventData) {
		return;
	}

	// INT
	StoredPayload.FloatPayload = TriggerEventData->EventMagnitude;
	StoredPayload.PayloadTag = TriggerEventData->EventTag;
	// VECTOR + HITRESULT
	const FGameplayEffectContextHandle& context = TriggerEventData->ContextHandle;
	if (context.IsValid()) {
		StoredPayload.VectorPayload = context.GetOrigin();
		const FHitResult* HR = context.GetHitResult();
		if (HR) {
			StoredPayload.HitResult = *HR;
		}
	}

	// TARGET
	if (TriggerEventData->Target) {
		StoredPayload.TargetActor = TriggerEventData->Target;
	}

	StoredPayload.bIsValid = true;
}



void UACFActionAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	GetActionsManager()->OnAbilityStarted(this);
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (CommitAbilityCost(Handle, ActorInfo, ActivationInfo)) {
		if (ActionsManager && CharacterOwner) {
			if (CharacterOwner->HasAuthority()) {
				// Always execute server-side logic
				Internal_OnActivated(ActionsManager, animMontage);
				// In standalone / listen server, this pawn is also locally controlled
				if (CharacterOwner->IsLocallyControlled()) {
					// Execute local feedback logic too
					ClientsOnActionStarted();
				}
			}
			else {
				// Owning client in multiplayer 
				ClientsOnActionStarted();
			}
		}
	}
	else {
		EndAbility(Handle, ActorInfo, activationInfo, true, true);
		return;
	}

	// select the sections, updates the speed
	PrepareMontageInfo();

	// initializes root motion and warping
	InitWarp();

	if (ActionConfig.bAutoExecute) {
		ExecuteAction();
	}
	if (ActionConfig.bPlayEffectOnActionStart) {
		PlayEffects();
	}
}

void UACFActionAbility::GetWarpInfo_Implementation(FACFWarpReproductionInfo& outWarpInfo)
{
	outWarpInfo.WarpConfig = ActionConfig.WarpInfo;
	if (!animMontage) {
		return;
	}
	const FName sectionName = GetMontageSectionName();
	int32 currentIndex = animMontage->GetSectionIndex(sectionName);
	if (currentIndex < 0) {
		currentIndex = 0;
	}
	/* float endTime;
	 animMontage->GetSectionStartAndEndTime(currentIndex, outWarpInfo.WarpConfig.WarpStartTime, endTime);
	 outWarpInfo.WarpConfig.WarpEndTime = outWarpInfo.WarpConfig.WarpStartTime + ActionConfig.WarpInfo.WarpEndTime;*/
	if (ActionConfig.WarpInfo.TargetType == EWarpTargetType::ETargetTransform) {
		const FTransform endTransform = GetWarpTransform();
		FVector localScale = FVector(1.f);
		UKismetMathLibrary::BreakTransform(endTransform, outWarpInfo.WarpLocation, outWarpInfo.WarpRotation, localScale);
	}
	else if (ActionConfig.WarpInfo.TargetType == EWarpTargetType::ETargetComponent) {
		outWarpInfo.TargetComponent = GetWarpTargetComponent();
	}
}

void UACFActionAbility::SetMontageReproductionType(EMontageReproductionType reproType)
{
	ActionConfig.MontageReproductionType = reproType;
}

void UACFActionAbility::PrepareMontageInfo()
{
	MontageInfo.MontageAction = GetMontage();
	MontageInfo.ReproductionSpeed = GetPlayRate();
	if (ActionConfig.bPlayRandomMontageSection) {
		const int32 numSections = MontageInfo.MontageAction->CompositeSections.Num();

		const int32 sectionToPlay = FMath::RandHelper(numSections);

		MontageInfo.StartSectionName = animMontage->GetSectionName(sectionToPlay);
	}
	else {
		MontageInfo.StartSectionName = GetMontageSectionName();
	}
	MontageInfo.ReproductionType = ActionConfig.MontageReproductionType;
	MontageInfo.RootMotionScale = 1.f;
}

void UACFActionAbility::ExecuteAction()
{
	if (animMontage && ActionsManager) {
		PlayCurrentMontage();
		bIsExecutingAction = true;
	}
	else {
		ExitAction(true);
	}
}

void UACFActionAbility::InitWarp()
{
	const UMotionWarpingComponent* motionComp = CharacterOwner->FindComponentByClass<UMotionWarpingComponent>();
	switch (MontageInfo.ReproductionType) {
	case EMontageReproductionType::ERootMotionScaled:
		MontageInfo.RootMotionScale = ActionConfig.RootMotionScale;
		break;
	case EMontageReproductionType::ERootMotion:
		break;
	case EMontageReproductionType::EMotionWarped:
		if (motionComp) {
			FACFWarpReproductionInfo WarpInfo;
			GetWarpInfo(WarpInfo);
			MontageInfo.WarpInfo = WarpInfo;
			UpdateWarp();
		}
		break;
	}
}

void UACFActionAbility::UpdateWarp()
{
	UMotionWarpingComponent* motionComp = CharacterOwner->FindComponentByClass<UMotionWarpingComponent>();
	const FTransform targetTransform = FTransform(MontageInfo.WarpInfo.WarpRotation, MontageInfo.WarpInfo.WarpLocation);

	if (motionComp) {

		FMotionWarpingTarget targetPoint;
		if (MontageInfo.WarpInfo.WarpConfig.TargetType == EWarpTargetType::ETargetComponent && MontageInfo.WarpInfo.TargetComponent) {
			targetPoint.Name = MontageInfo.WarpInfo.WarpConfig.SyncPoint;
			targetPoint.Component = MontageInfo.WarpInfo.TargetComponent;
			targetPoint.bFollowComponent = true;
		}
		else {
			targetPoint = FMotionWarpingTarget(MontageInfo.WarpInfo.WarpConfig.SyncPoint, targetTransform);
		}
		motionComp->AddOrUpdateWarpTarget(targetPoint);

		/* USE THE NOTIFY INSTEAD
		if (MontageInfo.WarpInfo.WarpConfig.bAutoWarp) {
			URootMotionModifier_SkewWarp::AddRootMotionModifierSkewWarp(motionComp, MontageInfo.MontageAction, MontageInfo.WarpInfo.WarpConfig.WarpStartTime,
				MontageInfo.WarpInfo.WarpConfig.WarpEndTime, MontageInfo.WarpInfo.WarpConfig.SyncPoint, EWarpPointAnimProvider::None, targetTransform, NAME_None, true, true, true,
				MontageInfo.WarpInfo.WarpConfig.RotationType, EMotionWarpRotationMethod::Slerp, MontageInfo.WarpInfo.WarpConfig.WarpRotationTime);
		}*/

		if (ActionsManager->bPrintDebugInfo) {
			UKismetSystemLibrary::DrawDebugSphere(this, MontageInfo.WarpInfo.WarpLocation, 100.f, 12, FLinearColor::Red, 5.f);
			const FVector Start = MontageInfo.WarpInfo.WarpLocation;
			const FVector Direction = MontageInfo.WarpInfo.WarpRotation.Vector(); // Convert ROTATION TO DIRECTION
			const FVector End = Start + Direction * 200.f;

			UKismetSystemLibrary::DrawDebugArrow(
				this,
				Start,
				End,
				200, // Arrow size
				FLinearColor::Red,
				10.f // Duration in seconds
			);
		}
	}
	else {
		motionComp->RemoveWarpTarget(MontageInfo.WarpInfo.WarpConfig.SyncPoint);
	}
}

void UACFActionAbility::PlayCurrentMontage()
{
	const float rootMotionScale = ActionConfig.MontageReproductionType == EMontageReproductionType::ERootMotionScaled ? ActionConfig.RootMotionScale : 1.f;

	// Using a gameplay Task to play the montage

	UAbilityTask_PlayMontageAndWait* Task = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, MontageInfo.StartSectionName,
		MontageInfo.MontageAction, GetPlayRate(), MontageInfo.StartSectionName, true, rootMotionScale);
	if (Task) {
		Task->OnBlendOut.AddDynamic(this, &UACFActionAbility::HandleMontageFinished);
		Task->OnInterrupted.AddDynamic(this, &UACFActionAbility::HandleMontageInterrupted);
		Task->OnCancelled.AddDynamic(this, &UACFActionAbility::HandleMontageInterrupted);
		Task->OnCompleted.AddDynamic(this, &UACFActionAbility::HandleMontageFinished);
		Task->ReadyForActivation();
	}
}

void UACFActionAbility::Internal_OnActivated(class UACFAbilitySystemComponent* actionManger, class UAnimMontage* inAnimMontage)
{
	if (StatisticComp) {
		executionEffect = StatisticComp->AddAttributeSetModifier(ActionConfig.AttributeModifier);
	}
	else {
		UE_LOG(ACFLog, Warning, TEXT("Invalid Attributes Setup!"));
		/*        ExitAction();*/
		return;
	}

	OnActionStarted();
}

void UACFActionAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	if (HasAuthority(&ActivationInfo)) {
		Internal_OnDeactivated();
		// In standalone / listen server, this pawn is also locally controlled
		if (CharacterOwner->IsLocallyControlled()) {
			// Execute local feedback logic too
			ClientsOnActionEnded();
		}
	}
	else {
		ClientsOnActionEnded();
	}

	if (ActionConfig.bAutoStartCooldown) {
		CommitAbilityCooldown(selfHandle, ActorInfo, ActivationInfo, true);
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);

	if (ActionsManager) {
		ActionsManager->OnAbilityEnded(this);
	}
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
	return Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags) && CanExecuteAction(Cast<ACharacter>(ActorInfo->OwnerActor));
}

bool UACFActionAbility::CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, OUT FGameplayTagContainer* OptionalRelevantTags /*= nullptr*/) const
{
	if (!ActorInfo->AvatarActor->IsValidLowLevelFast()) {
		return false;
	}

	if (!StatisticComp) {
		return false;
	}

	const TObjectPtr<UARSLevelingComponent> levelingComp = ActorInfo->AvatarActor->FindComponentByClass<UARSLevelingComponent>();
	if (!levelingComp || levelingComp->GetCurrentLevel() < ActionConfig.RequiredLevel) {
		return false;
	}

	if (!StatisticComp->CheckCosts(ActionConfig.ActionCost)) {
		return false;
	}

	return Super::CheckCost(Handle, ActorInfo, OptionalRelevantTags);
}

void UACFActionAbility::HandleGameplayEventReceived(FGameplayEventData Payload)
{
	if (!CharacterOwner) {
		return;
	}
	if (Payload.EventTag.MatchesTagExact(UGameplayTagsManager::Get().RequestGameplayTag(ACF::ExitTag))) {
		if (ActionsManager->HasStoredActions()) {
			// if there are other actions in the queue, we just exit this one
			ExitAction(false);
		}
		else {
			// otherwise just put this ability on minimum priority so that can be interrupted by any other
			ActionsManager->SetCurrentPriority(-1);
		}
	}
	else if (Payload.EventTag.MatchesTagExact(UGameplayTagsManager::Get().RequestGameplayTag(ACF::NotableTag))) {
		if (HasAuthority(&activationInfo)) {
			// In standalone / listen server, this pawn may also be locally controlled
			if (CharacterOwner->IsLocallyControlled()) {
				// Execute local feedback logic too
				ClientsOnNotablePointReached();
			}
			OnNotablePointReached();
		}
		else {
			ClientsOnNotablePointReached();
		}
	}
	else if (Payload.EventTag.MatchesTagExact(UGameplayTagsManager::Get().RequestGameplayTag(ACF::FXTag))) {
		PlayEffects();
	}
	else {
		OnGameplayEventReceived(Payload.EventTag);
	}
}

void UACFActionAbility::OnAvatarSet(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	Super::OnAvatarSet(ActorInfo, Spec);
	CharacterOwner = Cast<ACharacter>(ActorInfo->AvatarActor);
	ActionsManager = Cast<UACFAbilitySystemComponent>(ActorInfo->AbilitySystemComponent);
	if (ActorInfo->AvatarActor->IsValidLowLevelFast()) {
		StatisticComp = ActorInfo->AvatarActor->FindComponentByClass<UARSStatisticsComponent>();
	}
	K2_OnPawnAvatarSet();
}

bool UACFActionAbility::CommitAbilityCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, OUT FGameplayTagContainer* OptionalRelevantTags /*= nullptr*/)
{

	return Super::CommitAbilityCost(Handle, ActorInfo, ActivationInfo, OptionalRelevantTags);
}

bool UACFActionAbility::CommitAbilityCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const bool ForceCooldown, OUT FGameplayTagContainer* OptionalRelevantTags /*= nullptr*/)
{
	if (UsingDefaultCooldown()) {
		if (!ActivationBlockedTags.HasTagExact(ActionTag)) {
			ActivationBlockedTags.AddTag(ActionTag);
		}
		if (!cooldownTags.HasTagExact(ActionTag)) {
			cooldownTags.AddTag(ActionTag);
		}
	}
	return Super::CommitAbilityCooldown(Handle, ActorInfo, ActivationInfo, ForceCooldown, OptionalRelevantTags);
}

void UACFActionAbility::ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	if (UsingDefaultCooldown()) {
		FGameplayEffectSpecHandle CooldownSpec = ActionsManager->MakeOutgoingSpec(CooldownGameplayEffectClass, GetAbilityLevel(Handle, ActorInfo), ActionsManager->MakeEffectContext());
		CooldownSpec.Data->DynamicGrantedTags.AddTag(ActionTag);
		CooldownSpec.Data->SetDuration(ActionConfig.CoolDownTime, true);
		ActionsManager->ApplyGameplayEffectSpecToSelf(*CooldownSpec.Data.Get());
	}
	else {
		Super::ApplyCooldown(Handle, ActorInfo, ActivationInfo);
	}
}

void UACFActionAbility::ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	if (StatisticComp && ActionConfig.CostGEType == EGEType::ESetByCallerFromConfig) {
		StatisticComp->ConsumeStatistics(ActionConfig.ActionCost);
	}
	Super::ApplyCost(Handle, ActorInfo, ActivationInfo);
}

bool UACFActionAbility::UsingDefaultCooldown() const
{
	return CooldownGameplayEffectClass == UACFDefaultCooldownGE::StaticClass() && ActionConfig.CoolDownTime > 0.f;
}

float UACFActionAbility::GetCooldownTimeRemaining(const FGameplayAbilityActorInfo* ActorInfo) const
{
	return Super::GetCooldownTimeRemaining(ActorInfo);
}

const FGameplayTagContainer* UACFActionAbility::GetCooldownTags() const
{
	if (UsingDefaultCooldown()) {
		return &cooldownTags;
	}
	return Super::GetCooldownTags();
}

void UACFActionAbility::ExitAction(bool bCancelled)
{
	EndAbility(selfHandle, &actorInfo, activationInfo, true, bCancelled);
}

void UACFActionAbility::OnGameplayEventReceived_Implementation(const FGameplayTag eventTag)
{
	// Implement in child classes!
}

float UACFActionAbility::GetPlayRate_Implementation() const
{
	return 1.f;
}

UAnimMontage* UACFActionAbility::GetMontage_Implementation() const
{
	return animMontage;
}

void UACFActionAbility::InitAbility()
{
	ActionTag = GetCurrentAbilitySpec()->GetDynamicSpecSourceTags().First();

	UACFActionAbility* Template = Cast<UACFActionAbility>(GetCurrentSourceObject());

	if (!Template || Template->GetClass() != GetClass()) {
		return;
	}

	for (TFieldIterator<FProperty> PropIt(Template->GetClass()); PropIt; ++PropIt) {
		FProperty* Property = *PropIt;

		if (Property && Property->HasAnyPropertyFlags(CPF_Edit)) {
			void* SourceValue = Property->ContainerPtrToValuePtr<void>(Template);
			void* DestValue = Property->ContainerPtrToValuePtr<void>(this);

			Property->CopyCompleteValue(DestValue, SourceValue);
		}
	}
	/*
  TO DO: BETTER TO KEEP ALL THIS DATA ALREADY WITHIN THE ABILITY AND NOT OUTSIDE!
   if (animMontage != Source.MontageAction) {
	   animMontage = Source.MontageAction;
   }*/

	bFullyInit = true;
}

void UACFActionAbility::HandleMontageFinished()
{
	ExitAction(false);
}

void UACFActionAbility::HandleMontageInterrupted()
{
	ExitAction(true);
}

void UACFActionAbility::Internal_OnDeactivated()
{
	if (bIsExecutingAction) {
		bIsExecutingAction = false;
	}

	if (CharacterOwner) {
		UMotionWarpingComponent* motionComp = CharacterOwner->FindComponentByClass<UMotionWarpingComponent>();

		if (motionComp) {
			motionComp->RemoveWarpTarget(MontageInfo.WarpInfo.WarpConfig.SyncPoint);
		}
	}

	if (StatisticComp) {
		StatisticComp->RemoveAttributeSetModifier(executionEffect);
	}

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

void UACFActionAbility::SetAnimMontage(UAnimMontage* newMontage)
{
	animMontage = newMontage;
}

bool UACFActionAbility::IsFullyInit() const
{
	return bFullyInit;
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

void UACFActionAbility::PlayEffects_Implementation()
{

	if (ActionConfig.ActionEffect.ActionParticle || ActionConfig.ActionEffect.NiagaraParticle || ActionConfig.ActionEffect.ActionSound) {
		UACFAbilitySystemComponent* ASC = GetActionsManager();
		if (!ASC) {
			UE_LOG(ACFLog, Error, TEXT("Missing Ability Comp in Instigator!! - UACFActionAbility "));
			return;
		}
		FGameplayEffectContextHandle ContextHandle = ASC->MakeEffectContext();

		FGameplayCueParameters Params;
		Params.EffectContext = ContextHandle;
		Params.AggregatedSourceTags.AddTag(ActionTag);
		const FGameplayTag moveset = ASC->GetCurrentMovesetActionsTag();
		if (moveset != FGameplayTag()) {
			Params.AggregatedSourceTags.AddTag(moveset);
		}
		const FGameplayTag actionCue = UGameplayTagsManager::Get().RequestGameplayTag(ACF::ActionCue);
		ASC->ExecuteGameplayCue(actionCue, Params);
	}
	if (HasAuthority(&activationInfo)) {
		if (CharacterOwner && CharacterOwner->HasAuthority() && ActionConfig.ActionEffect.NoiseEmitted > 0) {
			CharacterOwner->MakeNoise(ActionConfig.ActionEffect.NoiseEmitted, CharacterOwner, CharacterOwner->GetActorLocation());
		}
	}
}

bool UACFActionAbility::CanExecuteAction_Implementation(class ACharacter* owner) const
{
	return true;
}

FName UACFActionAbility::GetMontageSectionName_Implementation()
{
	return NAME_None;
}

FTransform UACFActionAbility::GetWarpTransform_Implementation()
{
	ensure(false);
	return FTransform();
}

class USceneComponent* UACFActionAbility::GetWarpTargetComponent_Implementation()
{
	ensure(false);
	return nullptr;
}
void UACFActionAbility::OnNotablePointReached_Implementation()
{
}

void UACFActionAbility::ClientsOnNotablePointReached_Implementation()
{
}
