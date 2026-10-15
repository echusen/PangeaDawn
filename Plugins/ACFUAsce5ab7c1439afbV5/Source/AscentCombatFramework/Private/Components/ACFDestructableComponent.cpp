// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "Components/ACFDestructableComponent.h"
#include "ACFGameplayCueTags.h"
#include "ACFTeamManagerSubsystem.h"
#include "AbilitySystemComponent.h"
#include "Components/ACFDamageHandlerComponent.h"
#include "Components/ACFEquipmentComponent.h"
#include "Game/ACFDestructableDamageCalculator.h"
#include "GameFramework/DamageType.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "Logging.h"
#include <Engine/World.h>
#include <GameFramework/Actor.h>

UACFDestructableComponent::UACFDestructableComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
	DestructionCueTag = FACFCueTags::GameplayCue_ACF_Destruction;
}

void UACFDestructableComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	if (!Owner) {
		return;
	}

	GeometryCollectionComp = Owner->FindComponentByClass<UGeometryCollectionComponent>();
	if (!GeometryCollectionComp) {
		UE_LOG(ACFLog, Warning, TEXT("UACFDestructableComponent on [%s]: no GeometryCollectionComponent found, nothing will break."), *Owner->GetName());
	} else if (bEnforceDestructibleCollisionSetup) {
		GeometryCollectionComp->SetCollisionProfileName(TEXT("Destructible"));
		// Body locks would freeze the broken pieces in place
		GeometryCollectionComp->BodyInstance.bLockTranslation = false;
		GeometryCollectionComp->BodyInstance.bLockRotation = false;
	}

	if (UACFDamageHandlerComponent* DamageHandler = Owner->FindComponentByClass<UACFDamageHandlerComponent>()) {
		if (bAutoSetDamageCalculator) {
			DamageHandler->SetDamageCalculatorClass(UACFDestructableDamageCalculator::StaticClass());
		}
		if (!DamageHandler->OnOwnerDeath.IsAlreadyBound(this, &UACFDestructableComponent::HandleOwnerDeath)) {
			DamageHandler->OnOwnerDeath.AddDynamic(this, &UACFDestructableComponent::HandleOwnerDeath);
		}
	}
}

void UACFDestructableComponent::HandleOwnerDeath()
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority() || bWasDestroyed) {
		return;
	}
	bWasDestroyed = true;

	FACFDamageEvent Last;
	if (const UACFDamageHandlerComponent* DamageHandler = Owner->FindComponentByClass<UACFDamageHandlerComponent>()) {
		Last = DamageHandler->GetLastDamageInfo();
	}

	FVector StrainLoc = Last.hitResult.ImpactPoint;
	if (StrainLoc.IsNearlyZero()) {
		StrainLoc = Owner->GetActorLocation();
	}

	// Equipment drop / OnActorDestroyed are handled by the owner's own death flow here
	Multicast_PlayDestruction(StrainLoc, ComputeScatterVelocity(Last));
	PlayDestructionCue(Last, StrainLoc);
}

void UACFDestructableComponent::HandleHitDestruction(const FACFDamageEvent& DamageEvent)
{
	AActor* Owner = GetOwner();
	if (!bDestroyOnFirstHostileHit || !Owner || !Owner->HasAuthority() || bWasDestroyed) {
		return;
	}

	if (!DamageEvent.DamageDealer) {
		return;
	}

	// TakeDamage can be reached without going through the collisions manager team check
	if (UWorld* World = GetWorld()) {
		if (const UACFTeamManagerSubsystem* TeamSubsystem = World->GetSubsystem<UACFTeamManagerSubsystem>()) {
			if (!TeamSubsystem->CanActorDamageActor(DamageEvent.DamageDealer, Owner)) {
				return;
			}
		}
	}

	ForceDestruction(DamageEvent);
}

void UACFDestructableComponent::ForceDestruction(const FACFDamageEvent& DamageEvent)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority() || bWasDestroyed) {
		return;
	}
	bWasDestroyed = true;
	Owner->SetCanBeDamaged(false);

	FVector StrainLoc = DamageEvent.hitResult.ImpactPoint;
	if (StrainLoc.IsNearlyZero()) {
		StrainLoc = Owner->GetActorLocation();
	}

	Multicast_PlayDestruction(StrainLoc, ComputeScatterVelocity(DamageEvent));
	PlayDestructionCue(DamageEvent, StrainLoc);

	if (UACFEquipmentComponent* EquipmentComp = Owner->FindComponentByClass<UACFEquipmentComponent>()) {
		EquipmentComp->OnEntityOwnerDeath();
	}

	OnDestroyed.Broadcast();
}

FVector UACFDestructableComponent::ComputeScatterVelocity(const FACFDamageEvent& DamageEvent) const
{
	// hitDirection follows the ACF convention (points from the victim back toward the
	// damager, see UACMCollisionManagerComponent::ApplyPointDamage): scatter is its negation
	FVector ScatterDir = -DamageEvent.hitDirection.GetSafeNormal();
	if (ScatterDir.IsNearlyZero()) {
		ScatterDir = FallbackBreakImpulse.GetSafeNormal();
		if (ScatterDir.IsNearlyZero()) {
			ScatterDir = FVector::UpVector;
		}
	}

	// The punch comes from the hit (DamageImpulse of the damage type, kg*cm/s), the
	// resistance from the prop (collection mass): impulse over mass is the piece velocity
	float DamageImpulse = GetDefault<UDamageType>()->DamageImpulse;
	if (DamageEvent.DamageClass) {
		if (const UDamageType* DamageTypeCDO = DamageEvent.DamageClass->GetDefaultObject<UDamageType>()) {
			DamageImpulse = DamageTypeCDO->DamageImpulse;
		}
	}

	const float Mass = FMath::Max(GeometryCollectionComp ? GeometryCollectionComp->GetMass() : 1.f, 1.f);
	return ScatterDir * (DamageImpulse / Mass);
}

void UACFDestructableComponent::PlayDestructionCue(const FACFDamageEvent& DamageEvent, const FVector& ImpactPoint) const
{
	if (!DestructionCueTag.IsValid()) {
		return;
	}

	// Props have no AbilitySystem: route the cue through the damage dealer's one so
	// GAS replicates it to every client (same pattern as UACMEffectsDispatcherComponent)
	UAbilitySystemComponent* ASC = DamageEvent.DamageDealer ? DamageEvent.DamageDealer->FindComponentByClass<UAbilitySystemComponent>() : nullptr;
	if (!ASC) {
		UE_LOG(ACFLog, Log, TEXT("No AbilitySystem on the damage dealer, destruction cue skipped - UACFDestructableComponent"));
		return;
	}

	FGameplayEffectContextHandle ContextHandle = ASC->MakeEffectContext();
	ContextHandle.AddInstigator(DamageEvent.DamageDealer, DamageEvent.DamageDealer);
	ContextHandle.AddHitResult(DamageEvent.hitResult);

	FGameplayCueParameters Params;
	Params.EffectContext = ContextHandle;
	Params.Location = ImpactPoint;
	Params.SourceObject = this;
	Params.Instigator = DamageEvent.DamageDealer;
	Params.EffectCauser = DamageEvent.DamageDealer;

	ASC->ExecuteGameplayCue(DestructionCueTag, Params);
}

void UACFDestructableComponent::Multicast_PlayDestruction_Implementation(FVector StrainWorldLocation, FVector ScatterVelocity)
{
	ApplyChaosDestructionAt(StrainWorldLocation, ScatterVelocity);
}

void UACFDestructableComponent::ApplyChaosDestructionAt(FVector StrainWorldLocation, FVector ScatterVelocity)
{
	if (!GeometryCollectionComp || !GeometryCollectionComp->GetRestCollection()) {
		return;
	}

	const int32 RootIdx = GeometryCollectionComp->GetRootIndex();
	if (RootIdx == INDEX_NONE) {
		return;
	}

	GeometryCollectionComp->ApplyExternalStrain(RootIdx, StrainWorldLocation, ExternalStrainRadius, ExternalStrainPropagationDepth,
		ExternalStrainPropagationFactor, ExternalStrainMagnitude);

	if (!ScatterVelocity.IsNearlyZero()) {
		GeometryCollectionComp->ApplyBreakingLinearVelocity(RootIdx, ScatterVelocity);
	}
}
