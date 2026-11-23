// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "Components/ACFEffectsManagerComponent.h"
#include "ACMCollisionsFunctionLibrary.h"
#include "ACMEffectsDispatcherComponent.h"
#include "ACMTypes.h"
#include "Actors/ACFCharacter.h"
#include "Components/ACFAbilitySystemComponent.h"
#include "Components/ACFCharacterMovementComponent.h"
#include "Components/ACFDamageHandlerComponent.h"
#include "Config/ACFAudioBankDataAsset.h"
#include "Config/ACFEffectsConfigDataAsset.h"
#include "Engine/World.h"
#include "Game/ACFFunctionLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Logging.h"
#include <AbilitySystemComponent.h>
#include <CollisionQueryParams.h>
#include <Components/AudioComponent.h>
#include <Components/SceneComponent.h>
#include <Components/SkeletalMeshComponent.h>
#include <NiagaraComponent.h>
#include <Particles/ParticleSystemComponent.h>
#include <PhysicalMaterials/PhysicalMaterial.h>
#include <Sound/SoundCue.h>
#include "Net/UnrealNetwork.h"

// Sets default values for this component's properties
UACFEffectsManagerComponent::UACFEffectsManagerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	FootstepNoiseByLocomotionState.Add(ELocomotionState::EWalk, .2f);
	FootstepNoiseByLocomotionState.Add(ELocomotionState::EJog, .6f);
	FootstepNoiseByLocomotionState.Add(ELocomotionState::ESprint, 1.f);

	FootstepNoiseByLocomotionStateWhenCrouched.Add(ELocomotionState::EWalk, .1f);
	FootstepNoiseByLocomotionStateWhenCrouched.Add(ELocomotionState::EJog, .3f);
	FootstepNoiseByLocomotionStateWhenCrouched.Add(ELocomotionState::EJog, .5f);
}

void UACFEffectsManagerComponent::BeginPlay()
{
	Super::BeginPlay();

	CharacterOwner = Cast<ACharacter>(GetOwner());

	UACFDamageHandlerComponent* DamageHanlder = GetOwner()->FindComponentByClass<UACFDamageHandlerComponent>();

	if (DamageHanlder && !DamageHanlder->OnDamageReceived.IsAlreadyBound(this, &UACFEffectsManagerComponent::HandleDamageReceived)) {
		DamageHanlder->OnDamageReceived.AddDynamic(this, &UACFEffectsManagerComponent::HandleDamageReceived);
	}

	if (UACFAbilitySystemComponent* AbilityComp = GetOwner()->FindComponentByClass<UACFAbilitySystemComponent>()) {
		if (!AbilityComp->OnAbilityStartedEvent.IsAlreadyBound(this, &UACFEffectsManagerComponent::HandleAbilityStarted)) {
			AbilityComp->OnAbilityStartedEvent.AddDynamic(this, &UACFEffectsManagerComponent::HandleAbilityStarted);
		}
	}
}

void UACFEffectsManagerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GetOwner()) {
		if (UACFAbilitySystemComponent* AbilityComp = GetOwner()->FindComponentByClass<UACFAbilitySystemComponent>()) {
			if (AbilityComp->OnAbilityStartedEvent.IsAlreadyBound(this, &UACFEffectsManagerComponent::HandleAbilityStarted)) {
				AbilityComp->OnAbilityStartedEvent.RemoveDynamic(this, &UACFEffectsManagerComponent::HandleAbilityStarted);
			}
		}
	}

	Super::EndPlay(EndPlayReason);
}

void UACFEffectsManagerComponent::TriggerFootstepFX(FName footBone /*= NAME_None*/)
{
	if (!GetCharacterEffectsConfig()) {
		return;
	}

	FBaseFX baseFX;

	if (GetCharacterEffectsConfig()->ShouldCheckForSurface()) {
		GetCharacterEffectsConfig()->TryGetFootstepFXBySurfaceType(GetCurrentTerrain(), baseFX);
	}
	else {
		GetCharacterEffectsConfig()->GetDefaultFootstepFX(baseFX);
	}

	FVector ownerLocation;

	if (footBone != NAME_None) {
		ownerLocation = CharacterOwner->GetMesh()->GetSocketLocation(footBone);
	}
	else {
		ownerLocation = CharacterOwner->GetActorLocation();
	}

	FImpactFX fxToPlay = FImpactFX(baseFX, ownerLocation);

	const float noise = GetNoiseToEmitForCurrentLocomotionState();

	if (UKismetSystemLibrary::IsServer(this) && noise != 0.f) {
		CharacterOwner->MakeNoise(noise, CharacterOwner, CharacterOwner->GetActorLocation());
	}

	// Not needed for footstep to spawn a cue as it's always handled only clientside.
	FComponentFX outComps;
	UACMCollisionsFunctionLibrary::SpawnSoundAndParticleAtLocation(CharacterOwner, fxToPlay, outComps);
}


void UACFEffectsManagerComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UACFEffectsManagerComponent, CharacterEffectsConfig);
	DOREPLIFETIME(UACFEffectsManagerComponent, AudioBank);
}

EPhysicalSurface UACFEffectsManagerComponent::GetCurrentTerrain()
{
	if (!IsValid(CharacterOwner)) {
		UE_LOG(ACFLog, Warning, TEXT("Invalid CharacterOwner! - UACFEffectsManagerComponent::GetCurrentTerrain"));
		return EPhysicalSurface::SurfaceType_Max;
	}

	FCollisionQueryParams outTraceParams = FCollisionQueryParams(FName(TEXT("Trace")), true, CharacterOwner);

	outTraceParams.bTraceComplex = true;
	outTraceParams.bReturnPhysicalMaterial = true;

	const FVector Start = CharacterOwner->GetActorLocation();
	const FVector End = Start + (FVector(0, 0, 1) * TraceLengthByActorLocation * -1);

	// Re-initialize hit info
	FHitResult outTerrain;

	const bool bHit = GetWorld()->LineTraceSingleByObjectType(outTerrain, Start, End, ECC_WorldStatic, outTraceParams);

	if (bHit) {
		const UPhysicalMaterial* PhysicsMtl = outTerrain.PhysMaterial.Get();
		if (IsValid(PhysicsMtl)) {
			const EPhysicalSurface surface = PhysicsMtl->SurfaceType;
			return surface;
		}

	}

	return EPhysicalSurface::SurfaceType_Max;
}

void UACFEffectsManagerComponent::HandleDamageReceived(const FACFDamageEvent& damageEvent)
{
	if (GetOwner()->HasAuthority()) {
		PlayHitReactionEffect(damageEvent);
	}
	OnDamageImpactReceived(damageEvent);
}

void UACFEffectsManagerComponent::OnDamageImpactReceived_Implementation(const FACFDamageEvent& damageEvent)
{
}



void UACFEffectsManagerComponent::PlayEffectAttached_Implementation(const FActionEffect& attachedFX)
{
	ClientsPlayEffectAttached(attachedFX);
}

void UACFEffectsManagerComponent::StopEffectAttached_Implementation(const FActionEffect& attachedFX)
{
	ClientsStopEffectAttached(attachedFX);
}

void UACFEffectsManagerComponent::ClientsPlayEffectAttached_Implementation(const FActionEffect& attachedFX)
{
	UACMEffectsDispatcherComponent* effDisp = UACMCollisionsFunctionLibrary::GetEffectDispatcher(this);
	if (effDisp) {
		FAttachedComponents attComp = UACMCollisionsFunctionLibrary::SpawnSoundAndParticleAttached(attachedFX, CharacterOwner);
		ActiveFX.Add(attachedFX.GetGuid(), attComp);
	}
}

void UACFEffectsManagerComponent::OnRep_CharacterEffectsConfig()
{

}

void UACFEffectsManagerComponent::ClientsStopEffectAttached_Implementation(const FActionEffect& attachedFX)
{
	if (ActiveFX.Contains(attachedFX.GetGuid())) {
		FAttachedComponents attComp = ActiveFX.FindChecked(attachedFX.GetGuid());
		if (attComp.AudioComp) {
			attComp.AudioComp->Stop();
			attComp.AudioComp->DestroyComponent();
		}
		if (attComp.CascadeComp) {
			attComp.CascadeComp->DestroyComponent();
		}

		if (attComp.NiagaraComp) {
			attComp.NiagaraComp->DestroyComponent();
		}
		ActiveFX.Remove(attachedFX.GetGuid());
	}
}

void UACFEffectsManagerComponent::PlayHitReactionEffect(const FACFDamageEvent& damageEvent)
{
	if (!GetOwner()->HasAuthority()) {
		UE_LOG(ACFLog, Warning, TEXT("Trying to play a replicated GameplayCue from a client!"));
		return;
	}
	if (!DefaultHitCue.IsValid()) {
		UE_LOG(ACFLog, Warning, TEXT("DefaultHitCue is invalid - UACFEffectsManagerComponent::PlayHitReactionEffect"));
		return;
	}
	UAbilitySystemComponent* ASC = GetOwner()->FindComponentByClass<UAbilitySystemComponent>();
	if (!ASC) {
		UE_LOG(ACFLog, Error, TEXT("Missing Ability Comp in Instigator!! - UACFEffectsManagerComponent "));
		return;
	}

	FGameplayEffectContextHandle ContextHandle = ASC->MakeEffectContext();
	ContextHandle.AddInstigator(GetOwner(), GetOwner());
	ContextHandle.AddHitResult(damageEvent.hitResult);

	FGameplayCueParameters Params;
	Params.EffectContext = ContextHandle;
	Params.Instigator = GetOwner();
	Params.EffectCauser = GetOwner();
	Params.Location = damageEvent.hitResult.ImpactPoint;
	Params.SourceObject = damageEvent.DamageClass.Get();
	if (damageEvent.HitResponseAction.IsValid()) {
		Params.AggregatedSourceTags.AddTag(damageEvent.HitResponseAction);
	}

	ASC->ExecuteGameplayCue(DefaultHitCue, Params);
}

void UACFEffectsManagerComponent::PlayDamageAvoidedEffect(const FGameplayTag& reactiveEventTag)
{
	if (!GetOwner()->HasAuthority()) {
		UE_LOG(ACFLog, Warning, TEXT("Trying to play a replicated GameplayCue from a client!"));
		return;
	}
	if (!DefaultAvoidedCue.IsValid()) {
		UE_LOG(ACFLog, Warning, TEXT("DefaultAvoidedCue is invalid - UACFEffectsManagerComponent::PlayDamageAvoidedEffect"));
		return;
	}
	UAbilitySystemComponent* ASC = GetOwner()->FindComponentByClass<UAbilitySystemComponent>();
	if (!ASC) {
		UE_LOG(ACFLog, Error, TEXT("Missing Ability Comp in Instigator!! - UACFEffectsManagerComponent "));
		return;
	}

	FGameplayEffectContextHandle ContextHandle = ASC->MakeEffectContext();
	ContextHandle.AddInstigator(GetOwner(), GetOwner());

	FGameplayCueParameters Params;
	Params.EffectContext = ContextHandle;
	Params.Instigator = GetOwner();
	Params.EffectCauser = GetOwner();
	if (reactiveEventTag.IsValid()) {
		Params.AggregatedSourceTags.AddTag(reactiveEventTag);
	}

	ASC->ExecuteGameplayCue(DefaultAvoidedCue, Params);
}


bool UACFEffectsManagerComponent::TryGetDamageFX(const FGameplayTag& HitRection, const TSubclassOf<class UDamageType>& DamageType, FBaseFX& outFX)
{
	if (GetCharacterEffectsConfig() && GetCharacterEffectsConfig()->TryGetDamageEffectsByHitReactionAndDamageType(HitRection, DamageType, outFX)) {
		return true;
	}
	return false;
}

bool UACFEffectsManagerComponent::TryGetDamageAvoidedFX(const FGameplayTag& avoidedTag, FBaseFX& outFX)
{
	if (GetCharacterEffectsConfig() && GetCharacterEffectsConfig()->TryGetDamageAvoidedFX(avoidedTag, outFX)) {
		return true;
	}
	return false;
}


float UACFEffectsManagerComponent::GetNoiseToEmitForCurrentLocomotionState() const
{
	if (!IsValid(CharacterOwner)) {
		UE_LOG(ACFLog, Warning, TEXT("Invalid CharacterOwner! - UACFEffectsManagerComponent::GetNoiseToEmitForCurrentLocomotionState"));
		return 0.f;
	}

	const UACFCharacterMovementComponent* locComp = CharacterOwner->FindComponentByClass<UACFCharacterMovementComponent>();
	if (locComp) {
		const ELocomotionState currentLocState = locComp->GetCurrentLocomotionState();
		return GetNoiseToEmitByLocomotionState(currentLocState);
	}
	UE_LOG(ACFLog, Warning, TEXT("Missing Locomotion Component! - UACFEffectsManagerComponent::GetNoiseToEmitForCurrentLocomotionState "));
	return 0.f;
}

float UACFEffectsManagerComponent::GetNoiseToEmitByLocomotionState(ELocomotionState locState) const
{
	float outNoise = 0.f;
	if (!IsValid(CharacterOwner)) {
		UE_LOG(ACFLog, Warning, TEXT("Invalid CharacterOwner! - UACFEffectsManagerComponent::GetNoiseToEmitByLocomotionState"));
		return outNoise;
	}

	if (CharacterOwner->bIsCrouched && FootstepNoiseByLocomotionStateWhenCrouched.Contains(locState)) {
		outNoise = *FootstepNoiseByLocomotionStateWhenCrouched.Find(locState);
	}
	else if (FootstepNoiseByLocomotionState.Contains(locState)) {
		outNoise = *FootstepNoiseByLocomotionState.Find(locState);
	}
	return outNoise;
}

void UACFEffectsManagerComponent::SetCharacterEffectsConfig(UACFEffectsConfigDataAsset* val)
{
	CharacterEffectsConfig = val;
}

void UACFEffectsManagerComponent::SetAudioBank(UACFAudioBankDataAsset* val)
{
	AudioBank = val;
}

void UACFEffectsManagerComponent::HandleAbilityStarted(FGameplayTag AbilityTag)
{
	PlayAbilityAudio(AbilityTag);
}

bool UACFEffectsManagerComponent::ShouldPlayAbilityAudioLocally() const
{
	return GetNetMode() != NM_DedicatedServer;
}

void UACFEffectsManagerComponent::PlayAbilityAudio(FGameplayTag AbilityTag)
{
	if (!ShouldPlayAbilityAudioLocally() || !AbilityTag.IsValid() || !AudioBank) {
		return;
	}

	if (!IsValid(CharacterOwner) || !CharacterOwner->GetMesh()) {
		return;
	}

	USoundCue* SoundToPlay = nullptr;
	if (!AudioBank->TryGetAbilitySound(AbilityTag, SoundToPlay) || !SoundToPlay) {
		return;
	}

	UGameplayStatics::SpawnSoundAttached(SoundToPlay, CharacterOwner->GetMesh(), AudioBank->GetSoundBoneName());
}
