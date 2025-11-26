// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.

#pragma once

#include "Components/ACFAbilitySystemComponent.h"
#include "Components/ACFCharacterMovementComponent.h"
#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "Game/ACFDamageType.h"
#include <Engine/EngineTypes.h>

#include "ACFEffectsManagerComponent.generated.h"

/**
 * Component responsible for managing and triggering FX (particles, sounds, decals)
 * based on gameplay events such as footsteps, damage reactions, and terrain interaction.
 * Typically attached to characters or actors that require contextual visual/audio feedback.
 */
UCLASS(Blueprintable, ClassGroup = (ACF), meta = (BlueprintSpawnableComponent))
class ASCENTCOMBATFRAMEWORK_API UACFEffectsManagerComponent : public UActorComponent {
    GENERATED_BODY()

public:
    // Sets default values for this component's properties
    UACFEffectsManagerComponent();

public:
    /**
     * Triggers footstep visual and audio effects for the given bone, if valid.
     *
     * @param footBone The name of the foot bone to emit the FX from. If None, defaults to auto-selection.
     */
    UFUNCTION(BlueprintCallable, Category = ACF)
    void TriggerFootstepFX(FName footBone = NAME_None);

    /**
     * Returns the physical surface type the character is currently standing on.
     *
     * @return The current terrain surface (e.g., grass, rock, water).
     */
    UFUNCTION(BlueprintPure, Category = ACF)
    EPhysicalSurface GetCurrentTerrain();

    /**
     * Called when the character receives a damage event with impact data.
     *
     * @param damageEvent Struct containing the full impact information (e.g., hit direction, instigator, damage type).
     */
    UFUNCTION(BlueprintNativeEvent, Category = ACF)
    void OnDamageImpactReceived(const FACFDamageEvent& damageEvent);
    virtual void OnDamageImpactReceived_Implementation(const FACFDamageEvent& damageEvent);

    /**
     * Plays the visual and audio feedback for receiving damage, such as particles, sounds, and decals.
     *
     * @param damageEvent The data describing the hit and damage information.
     */
    UFUNCTION(BlueprintCallable, Category = ACF)
    void PlayHitReactionEffect(const FACFDamageEvent& damageEvent);

    /**
     * Attempts to retrieve the configured FX (particle, sound, etc.) for a given hit reaction and damage type.
     *
     * @param HitRection The gameplay tag identifying the type of reaction (e.g., light_hit, heavy_hit).
     * @param DamageType The specific subclass of UDamageType that caused the hit.
     * @param outFX Output parameter receiving the FX if a match is found.
     * @return True if a matching FX configuration was found, false otherwise.
     */
    UFUNCTION(BlueprintCallable, Category = ACF)
    bool TryGetDamageFX(const FGameplayTag& HitRection, const TSubclassOf<class UDamageType>& DamageType, FBaseFX& outFX);

    /**
     * Returns the noise value that should be emitted based on the character's current locomotion state.
     *
     * @return The scalar value of noise emitted (used for AI perception).
     */
    UFUNCTION(BlueprintCallable, Category = ACF)
    float GetNoiseToEmitForCurrentLocomotionState() const;

    /**
     * Returns the noise value associated with a specific locomotion state.
     *
     * @param locState The locomotion state to evaluate (e.g., walking, sprinting, crouching).
     * @return The scalar noise value to emit for that state.
     */
    UFUNCTION(BlueprintCallable, Category = ACF)
    float GetNoiseToEmitByLocomotionState(ELocomotionState locState) const;

    UFUNCTION(BlueprintCallable, Server, Reliable, Category = ACF)
    void PlayEffectAttached(const FActionEffect& attachedFX);

    UFUNCTION(BlueprintCallable, Server, Reliable, Category = ACF)
    void StopEffectAttached(const FActionEffect& attachedFX);

protected:
    // Called when the game starts
    virtual void BeginPlay() override;

    UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = ACF)
    FName DefaultHitBoneName = "pelvis";
       
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ACF)
    FGameplayTag DefaultHitCue;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ACF)
    class UACFEffectsConfigDataAsset* CharacterEffectsConfig;

    UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "ACF| Footstep")
    float TraceLengthByActorLocation = 200.f;

    /*The noise emitted while moving by this character. This noise is used as a check
    for AI Perceptions*/
    UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "ACF| Footstep")
    TMap<ELocomotionState, float> FootstepNoiseByLocomotionState;

    /*The noise emitted while moving by this character. This noise is used as a check
    for AI Perceptions*/
    UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "ACF| Footstep")
    TMap<ELocomotionState, float> FootstepNoiseByLocomotionStateWhenCrouched;

    UPROPERTY(BlueprintReadOnly, Category = ACF)
    class ACharacter* CharacterOwner;

private:
    UFUNCTION()
    void HandleDamageReceived(const FACFDamageEvent& damageEvent);

    /*
    void PlayCurrentActionEffect(const FActionEffect& effect);*/

    UFUNCTION(NetMulticast, Reliable)
    void ClientsPlayEffectAttached(const FActionEffect& attachedFX);

    UFUNCTION(NetMulticast, Reliable)
    void ClientsStopEffectAttached(const FActionEffect& attachedFX);

    TMap<FGuid, FAttachedComponents> ActiveFX;
};
