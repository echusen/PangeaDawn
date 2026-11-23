// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "ACMTypes.h"
#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "Game/ACFDamageType.h"

#include "ACFDestructableComponent.generated.h"

class UGeometryCollectionComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDestructableDestroyed);

/**
 * Makes the owning actor destructible: breaks its GeometryCollectionComponent with Chaos
 * when a valid hostile hit reaches the damage pipeline, bypassing statistics entirely
 * (props usually have no AbilitySystem / Health attribute).
 *
 * Fully self-contained: resolves the Geometry Collection, damage handler and equipment
 * generically on the owner, so it can be added to any actor. The owner (and this
 * component) must replicate for the destruction multicast to reach clients.
 */
UCLASS(ClassGroup = (ACF), Blueprintable, meta = (BlueprintSpawnableComponent))
class ASCENTCOMBATFRAMEWORK_API UACFDestructableComponent : public UActorComponent {
    GENERATED_BODY()

public:
    UACFDestructableComponent();

    /** Called by UACFDestructableDamageCalculator for every hit that reaches the damage
     * pipeline: validates dealer/team and applies the first-hostile-hit destruction rule. */
    void HandleHitDestruction(const FACFDamageEvent& DamageEvent);

    /** Server-side destruction triggered from a damage event, without going through statistics. */
    UFUNCTION(BlueprintCallable, Category = ACF)
    void ForceDestruction(const FACFDamageEvent& DamageEvent);

    /** Particle & sound spawned at the impact point by the destruction GameplayCue. */
    UFUNCTION(BlueprintPure, Category = ACF)
    FORCEINLINE FBaseFX GetDestructionFX() const { return DestructionFX; }

    UFUNCTION(BlueprintPure, Category = ACF)
    FORCEINLINE bool GetWasDestroyed() const { return bWasDestroyed; }

    /** Broadcast on the server when this component triggers the destruction
     * (not fired for stats-driven deaths: the owner's own death flow handles those). */
    UPROPERTY(BlueprintAssignable, Category = ACF)
    FOnDestructableDestroyed OnDestroyed;

protected:
    virtual void BeginPlay() override;

    /** If true, any first hostile hit (after team check) destroys the prop, regardless of health. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF|Destructable")
    bool bDestroyOnFirstHostileHit = true;

    /** Forces UACFDestructableDamageCalculator on the owner's damage handler at BeginPlay. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF|Destructable")
    bool bAutoSetDamageCalculator = true;

    /** Forces the Destructible collision profile and unlocks translation/rotation on the
     * Geometry Collection body at BeginPlay (body locks would freeze the broken pieces). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF|Destructable")
    bool bEnforceDestructibleCollisionSetup = true;

    /** Radius for ApplyExternalStrain (world units). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF|Destructable|Chaos", meta = (ClampMin = "1"))
    float ExternalStrainRadius = 400.f;

    /** Propagation depth for strain (cluster hierarchy levels). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF|Destructable|Chaos", meta = (ClampMin = "0"))
    int32 ExternalStrainPropagationDepth = 1000;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF|Destructable|Chaos", meta = (ClampMin = "0"))
    float ExternalStrainPropagationFactor = 1.f;

    /** Higher values break cluster bonds more aggressively. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF|Destructable|Chaos", meta = (ClampMin = "0"))
    float ExternalStrainMagnitude = 2400000.f;

    /** Fallback scatter direction when the damage event has no hit direction. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF|Destructable|Chaos")
    FVector FallbackBreakImpulse = FVector(0.f, 0.f, 600.f);

    /** Particle & sound played at the impact point when the prop breaks. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF|Destructable|FX")
    FBaseFX DestructionFX;

    /** Cue executed through the damage dealer's AbilitySystem when the prop breaks
     * (props carry no AbilitySystem of their own, so the dealer's one replicates it). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF|Destructable|FX", meta = (Categories = "GameplayCue"))
    FGameplayTag DestructionCueTag;

    /** Handles stats-driven deaths (owners configured with a working statistics component). */
    UFUNCTION()
    void HandleOwnerDeath();

    UFUNCTION(NetMulticast, Reliable)
    void Multicast_PlayDestruction(FVector StrainWorldLocation, FVector ScatterVelocity);

    void ApplyChaosDestructionAt(FVector StrainWorldLocation, FVector ScatterVelocity);

    /** Scatter velocity of the broken pieces: driven by the hit, not by the prop.
     * Direction is the negated ACF hitDirection; magnitude is the DamageType's
     * DamageImpulse divided by the collection mass (impulse -> delta velocity). */
    FVector ComputeScatterVelocity(const FACFDamageEvent& DamageEvent) const;

    /** Executes DestructionCueTag through the damage dealer's AbilitySystem (server only). */
    void PlayDestructionCue(const FACFDamageEvent& DamageEvent, const FVector& ImpactPoint) const;

private:
    UPROPERTY()
    TObjectPtr<UGeometryCollectionComponent> GeometryCollectionComp;

    bool bWasDestroyed = false;
};
