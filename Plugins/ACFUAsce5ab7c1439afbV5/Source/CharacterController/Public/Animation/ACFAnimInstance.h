// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "ACFActionTypes.h"
#include "ACFCCTypes.h"
#include "Animation/ACFAnimTypes.h"
#include "Animation/AnimInstance.h"
#include "CoreMinimal.h"
#include "Types/ACFFirstPersonTypes.h"
#include <GameplayTagContainer.h>

#include "ACFAnimInstance.generated.h"

class UACFRiderLayer;
class UACFBaseOverlayLayer;
class UACFClimbingLayer;
class UACFClimbingComponent;

// Delegate declarations - fired when movesets/overlays change
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMovesetChanged, FGameplayTag, NewMovesetTag);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnOverlayChanged, FGameplayTag, NewOverlayTag);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRiderLayerChanged, FGameplayTag, NewRiderLayerTag);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnClimbingLayerChanged, bool, bIsActive);


/**
 * Animation instance class for ACF characters
 * Handles locomotion states, movement data, overlays, and movesets.
 */
UCLASS()
class CHARACTERCONTROLLER_API UACFAnimInstance : public UAnimInstance {
    GENERATED_BODY()

public:
    UACFAnimInstance();

    /* Returns the movement direction in degrees */
    UFUNCTION(BlueprintPure, Category = ACF)
    FORCEINLINE float GetDirection()  const{ return Direction; }

    /* Returns the normalized movement speed (0..1) */
    UFUNCTION(BlueprintPure, Category = ACF)
    FORCEINLINE float GetNormalizedSpeed() const { return NormalizedSpeed; }

    /* Returns the normalized swim speed (0..1) */
    UFUNCTION(BlueprintPure, Category = ACF)
    FORCEINLINE float GetNormalizedSwimSpeed() const { return NormalizedSwimSpeed; }

    /* Returns the raw movement speed */
    UFUNCTION(BlueprintPure, Category = ACF)
    FORCEINLINE float GetSpeed() const { return Speed; }

    /* Returns the turn rate value */
    UFUNCTION(BlueprintPure, Category = ACF)
    FORCEINLINE float GetTurnRate() const { return TurnRate; }

    /* Checks if this anim instance belongs to the local player */
    UFUNCTION(BlueprintPure, Category = ACF)
    bool IsLocalPlayer() const;

    /* Returns the owning controller's rotation */
    UFUNCTION(BlueprintPure, Category = ACF)
    FRotator GetOwnerControllerRotation() const;

    /* Returns the current locomotion state */
    UFUNCTION(BlueprintPure, Category = ACF)
    ELocomotionState GetCurrentLocomotionState() const;

    /* Returns the target locomotion state (transition) */
    UFUNCTION(BlueprintPure, Category = ACF)
    ELocomotionState GetTargetLocomotionState() const;

    /* Returns whether the character is crouching */
    UFUNCTION(BlueprintPure, Category = ACF)
    bool GetIsCrouching() const;

    /* Returns true if the character is in air */
    UFUNCTION(BlueprintPure, Category = ACF)
    FORCEINLINE bool GetIsInAir() const { return bIsInAir; }

    /**
     * Returns the world location of the current target point, whatever set it: player lock-on,
     * AI combat target or script. The targeting component owns the look-at math; this only reads
     * a Game Thread copy, so it is safe from animation threads.
     * @param outLookAtLocation World location of the target point.
     * @return False when there is no target, so callers keep their own fallback.
     */
    UFUNCTION(BlueprintPure, Category = "ACF|AimOffset", meta = (BlueprintThreadSafe, DisplayName = "Get Target Look At Location", Keywords = "lock on target look at aim"))
    bool GetTargetLookAtLocation(FVector& outLookAtLocation) const;

    /**
     * The world rotation an aim offset should be built from, with the lock-on already handled:
     * eyes-to-target while a lock-on is engaged, the usual camera rotation otherwise (control
     * rotation where the controller lives, replicated base aim rotation on simulated proxies).
     * Feed it straight to your rotator-to-aim-offset conversion: it replaces the whole
     * Is Locally Controlled / Get Control Rotation / Get Base Aim Rotation select.
     * Safe from thread-safe animation graphs: it only reads a value cached on the Game Thread.
     * @return World rotation the character should be aiming along this frame.
     */
    UFUNCTION(BlueprintPure, Category = "ACF|AimOffset", meta = (BlueprintThreadSafe, DisplayName = "Get Aim Rotation", Keywords = "aim offset lock on target camera control rotation"))
    FORCEINLINE FRotator GetAimRotation() const { return AimRotation; }

    /**
     * Native equivalent of the MM template's Get_AOValue (non-riding branch).
     * Pass the SAME world-space RootTransform used by the animation graph, not the actor
     * transform: motion matching can rotate the animated root independently of the capsule.
     * Returns FVector2D(X = yaw, Y = pitch), with Disable_AO already applied.
     * Evaluates against the supplied root immediately, without the legacy AimOffset smoothing.
     */
    UFUNCTION(BlueprintPure, Category = "ACF|AimOffset", meta = (BlueprintThreadSafe, DisplayName = "Get Aim Offset"))
    FVector2D GetAimOffset(const FTransform& InRootTransform) const;

    /**
     * Native conversion for the MM FromRotatorToAimOffset wrapper, including Disable_AO.
     * Use this overload for riding/custom aim rotations; use GetAimOffset for normal aiming.
     * InRotation and InRootTransform must both be in world space. Only cached animation state
     * is read, so this is safe to call from BlueprintThreadSafeUpdateAnimation.
     */
    UFUNCTION(BlueprintPure, Category = "ACF|AimOffset", meta = (BlueprintThreadSafe, DisplayName = "Convert Rotation To Aim Offset"))
    FVector2D ConvertRotationToAimOffset(FRotator InRotation, const FTransform& InRootTransform) const;

    /**
     * True while the owner is anchoring its upper body to the camera through an ACFCameraAimComponent.
     * Wire it to the ACFCameraAim node's bEnabled pin.
     * Safe from thread-safe animation graphs: it only reads a value cached on the Game Thread.
     */
    UFUNCTION(BlueprintPure, Category = "ACF|CameraAim", meta = (BlueprintThreadSafe, DisplayName = "Is Camera Aim Active", Keywords = "camera aim first person shoulder view"))
    FORCEINLINE bool IsCameraAimActive() const { return bCameraAimActive; }

    /**
     * World space rotation the ACFCameraAim node aims the upper body at, sampled on the Game
     * Thread by the ACFCameraAimComponent before the mesh ticks, so the animation and the controller
     * are always solving against the very same value.
     * Safe from thread-safe animation graphs.
     */
    UFUNCTION(BlueprintPure, Category = "ACF|CameraAim", meta = (BlueprintThreadSafe, DisplayName = "Get Camera View Rotation", Keywords = "camera aim view control rotation"))
    FORCEINLINE FRotator GetCameraViewRotation() const { return CameraViewRotation; }

    /**
     * True while the owner's ACFFirstPersonComponent has first person on. False when it has none.
     * Safe from thread-safe animation graphs: it only reads a value cached on the Game Thread.
     */
    UFUNCTION(BlueprintPure, Category = "ACF|FirstPerson", meta = (BlueprintThreadSafe, Keywords = "first person perspective view"))
    FORCEINLINE bool IsInFirstPerson() const { return bIsInFirstPerson; }

    /**
     * 0 at hip, 1 fully aimed. Blended by the owner's ACFFirstPersonComponent; without one it is the raw aiming
     * state, 0 or 1. Safe from thread-safe animation graphs: it only reads a value cached on the Game Thread.
     */
    UFUNCTION(BlueprintPure, Category = "ACF|Aiming", meta = (BlueprintThreadSafe, Keywords = "aim ads alpha blend"))
    FORCEINLINE float GetAimingAlpha() const { return AimingAlpha; }

    /**
     * Bone the held item is held by, from the owner's ACFFirstPersonComponent: the item's own override, else the
     * component's ItemReferenceBone. None without one. Bind it to the Bone Rebase node's Pivot Source Bone Override
     * pin. Safe from thread-safe animation graphs: it only reads a value cached on the Game Thread.
     */
    UFUNCTION(BlueprintPure, Category = "ACF|FirstPerson", meta = (BlueprintThreadSafe, Keywords = "first person pivot hand bone rebase"))
    FORCEINLINE FName GetItemReferenceBone() const { return ItemReferenceBone; }

    /**
     * Hand offsets for the held item, from the active preset data's Hand Offset preset on the owner's
     * ACFFirstPersonComponent. Zero offsets without one. Bind it to the Procedural Hand Offset node's Hand Offset
     * Data pin. Safe from thread-safe animation graphs: it only reads a value cached on the Game Thread.
     */
    UFUNCTION(BlueprintPure, Category = "ACF|FirstPerson", meta = (BlueprintThreadSafe, Keywords = "first person hand offset clavicle lowerarm"))
    FORCEINLINE FACFProceduralHandOffsetData GetHandOffsetData() const { return HandOffsetData; }

    // MOVESETS
    /* Retrieves the animation overlay matching the given tag */
    UFUNCTION(BlueprintCallable, Category = ACF)
    bool GetAnimationOverlay(const FGameplayTag& tag, FOverlayLayer& outOverlay);

    /* Sets the current animation overlay */
    UFUNCTION(BlueprintCallable, Category = ACF)
    void SetAnimationOverlay(const FGameplayTag& overlayTag);

    /* Sets the riding layer overlay */
    UFUNCTION(BlueprintCallable, Category = ACF)
    void SetRidingLayer(const FGameplayTag& overlayTag);

    /* Sets the current moveset */
    UFUNCTION(BlueprintCallable, Category = ACF)
    void SetMoveset(const FGameplayTag& MovesetTag);

    /* Removes the current overlay and resets to none */
    UFUNCTION(BlueprintCallable, Category = ACF)
    void RemoveOverlay();

    /* Returns the current moveset instance */
    UFUNCTION(BlueprintPure, Category = ACF)
    FORCEINLINE UACFMovesetLayer* GetCurrentMoveset() const { return currentMovesetInstance; }

    /* Retrieves a moveset by tag */
    UFUNCTION(BlueprintCallable, Category = ACF)
    bool GetMovesetByTag(const FGameplayTag& movesetTag, FMoveset& outMoveset) const;

    /* Returns the current moveset tag */
    UFUNCTION(BlueprintPure, Category = ACF)
    FORCEINLINE FGameplayTag GetCurrentMovesetTag() const { return currentMoveset.TagName; }

    /* Returns the current overlay tag */
    UFUNCTION(BlueprintPure, Category = ACF)
    FORCEINLINE FGameplayTag GetCurrentOverlay() const { return currentOverlay.TagName; }

    /* Resets to the default moveset */
    UFUNCTION(BlueprintCallable, Category = ACF)
    void ResetToDefaultMoveset();

    /* Resets to the default overlay */
    UFUNCTION(BlueprintCallable, Category = ACF)
    void ResetToDefaultOverlay();

    /**
     * Populates the animation layer tables (MovesetLayers, OverlayLayers, RiderLayers, IKLayer,
     * ClimbingLayer) from an ACFAnimsetDataAsset. Only non-empty arrays / valid classes from the
     * asset overwrite the existing tables; unset entries are left untouched.
     *
     * This is a pure designer-data initialisation call — it does NOT change the currently active
     * moveset or overlay. Active layer selection remains driven by equipment/weapon logic through
     * UACFCharacterMovementComponent as usual.
     */
    UFUNCTION(BlueprintCallable, Category = "ACF|Movesets")
    void ApplyAnimset(class UACFAnimsetDataAsset* AnimsetData);

    // CLIMBING LAYER
    /* Activates the climbing animation layer */
    UFUNCTION(BlueprintCallable, Category = ACF)
    void ActivateClimbingLayer();

    /* Deactivates the climbing animation layer */
    UFUNCTION(BlueprintCallable, Category = ACF)
    void DeactivateClimbingLayer();

    /* Returns the current climbing layer instance */
    UFUNCTION(BlueprintPure, Category = ACF)
    FORCEINLINE UACFClimbingLayer* GetCurrentClimbingLayer() const { return currentClimbingInstance; }

    /* Returns whether the climbing layer is currently active */
    UFUNCTION(BlueprintPure, Category = ACF)
    FORCEINLINE bool IsClimbingLayerActive() const { return currentClimbingInstance != nullptr; }
    // END CLIMBING LAYER

    // END MOVESETS

     // DELEGATES
    /* Broadcast when the moveset changes */
    UPROPERTY(BlueprintAssignable, Category = "ACF|Events")
    FOnMovesetChanged OnMovesetChanged;

    /* Broadcast when the overlay changes */
    UPROPERTY(BlueprintAssignable, Category = "ACF|Events")
    FOnOverlayChanged OnOverlayChanged;

    /* Broadcast when the rider layer changes */
    UPROPERTY(BlueprintAssignable, Category = "ACF|Events")
    FOnRiderLayerChanged OnRiderLayerChanged;

    /* Broadcast when the climbing layer is activated or deactivated */
    UPROPERTY(BlueprintAssignable, Category = "ACF|Events")
    FOnClimbingLayerChanged OnClimbingLayerChanged;
    // END DELEGATES

    // IK
    /* Returns whether foot IK is enabled */
    UFUNCTION(BlueprintPure, Category = ACF, meta = (BlueprintThreadSafe))
    bool GetEnableFootIK() const { return EnableFootIK; }

    /* Enables or disables foot IK */
    UFUNCTION(BlueprintCallable, Category = ACF)
    void SetEnableFootIK(bool val) { EnableFootIK = val; }

    /* Returns whether hand IK is enabled */
    UFUNCTION(BlueprintPure, Category = ACF)
    bool GetEnableHandIK() const { return EnableHandIK; }

    /* Enables or disables hand IK */
    UFUNCTION(BlueprintCallable, Category = ACF)
    void SetEnableHandIK(bool val) { EnableHandIK = val; }

    /* Returns the desired start direction for locomotion */
    UFUNCTION(BlueprintCallable, Category = ACF)
    EACFDirection GetStartDirection() const { return DesiredStartDirection; }

    /* Sets the desired start direction for locomotion */
    UFUNCTION(BlueprintCallable, Category = ACF)
    void SetStartDirection(EACFDirection val) { DesiredStartDirection = val; }

    /* Returns the current root transform */
    UFUNCTION(BlueprintPure, Category = ACF)
    FTransform GetRootTrans() const { return RootTrans; }
    // END IK

    virtual void NativeInitializeAnimation() override;

    virtual void NativeBeginPlay() override;

    /* Runs on the Game Thread. Used to safely (re)resolve component references -
     * NativeThreadSafeUpdateAnimation runs on a parallel worker thread and must never
     * look up components there (see SetReferences). */
    virtual void NativeUpdateAnimation(float DeltaSeconds) override;

    virtual void NativeThreadSafeUpdateAnimation(float DeltaSeconds) override;

protected:
    // ----- CONFIG ---- //
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF|Config")
    float IsMovingSpeedThreshold = 0.02f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF|Config")
    float YawOffsetLimit = 60.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF|Config")
    float DirectionDeadZone = 10.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF|Config")
    float LeaningFactor = .5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF|Config")
    float TurnRateSmoothing = 2.f;

    UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "ACF|Config")
    float AimOffsetInterpSpeed = 10.f;

    /* Maximum aim yaw in either direction. Strafing clamps here and NEVER uses rear-facing aim.
     * In other rotation modes this is also the start of the rear-facing blend. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ACF|AimOffset", meta = (ClampMin = "0.0", ClampMax = "180.0", Units = "deg"))
    float AimOffsetYawLimit = 90.f;

    /* Angular width of the smooth transition to rear-facing aim, outside the yaw limit.
     * Ignored while strafing. Zero restores the Blueprint's hard threshold. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ACF|AimOffset", meta = (ClampMin = "0.0", ClampMax = "180.0", Units = "deg"))
    float AimOffsetRearBlendAngle = 15.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (TitleProperty = "TagName"), Category = "ACF|Movesets")
    TArray<FMoveset> MovesetLayers;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (TitleProperty = "TagName"), Category = "ACF|Overlays")
    TArray<FOverlayLayer> OverlayLayers;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (TitleProperty = "TagName"), Category = "ACF|Movesets")
    TArray<FRiderLayer> RiderLayers;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF|Movesets")
    TSubclassOf<class UACFIKLayer> IKLayer;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF|Movesets")
    TSubclassOf<class UACFClimbingLayer> ClimbingLayer;

    // ----- READ ONLY VARIABLES ---- //
    UPROPERTY(BlueprintReadOnly, Category = "ACF|Movesets")
    FMoveset currentMoveset;

    UPROPERTY(BlueprintReadOnly, Category = "ACF|Movesets")
    FOverlayLayer currentOverlay;

    UPROPERTY(BlueprintReadOnly, Category = "ACF|Movesets")
    FRiderLayer currentRiderLayer;

    // Instances
    UPROPERTY(BlueprintReadOnly, Category = ACF)
    UACFMovesetLayer* currentMovesetInstance;

    UPROPERTY(BlueprintReadOnly, Category = ACF)
    UACFBaseOverlayLayer* currentOverlayInstance;

    UPROPERTY(BlueprintReadOnly, Category = ACF)
    UACFRiderLayer* currentRiderInstance;

    UPROPERTY(BlueprintReadOnly, Category = ACF)
    UACFClimbingLayer* currentClimbingInstance;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF|Updates")
    bool bUpdateMovementData = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF|Updates")
    bool bUpdateLocationData = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF|Updates")
    bool bUpdateRotationData = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF|Updates")
    bool bUpdateSpeedData = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF|Updates")
    bool bUpdateAccelerationData = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF|Updates")
    bool bUpdateAimData = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF|Updates")
    bool bUpdateJumpData = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF|Updates")
    bool bUpdateLeaningData = true;

    UPROPERTY(BlueprintReadOnly, Category = ACF)
    bool bCanUseAdditive = false;

    UPROPERTY(BlueprintReadOnly, Category = "ACF|Movement")
    bool bIsCrouching;

    UPROPERTY(BlueprintReadOnly, Category = "ACF|Movement")
    bool bWasCrouching;

    UPROPERTY(BlueprintReadOnly, Category = "ACF|Movement")
    bool bCrouchChanged;

    UPROPERTY(BlueprintReadOnly, Category = "ACF|Movement")
    bool bIsOnGround;

    UPROPERTY(BlueprintReadOnly, Category = "ACF|Movement")
    bool bIsWalking;

    UPROPERTY(BlueprintReadOnly, Category = "ACF|Movement")
    bool bWasWalking;

    UPROPERTY(BlueprintReadOnly, Category = "ACF|Movement")
    bool bWalkStateChanged;

    UPROPERTY(BlueprintReadOnly, Category = "ACF|Movement")
    bool bIsStrafing;

    UPROPERTY(BlueprintReadOnly, Category = "ACF|Movement")
    bool bIsSwimming;

    UPROPERTY(BlueprintReadOnly, Category = "ACF|Movement")
    bool bIsClimbing;

    UPROPERTY(BlueprintReadOnly, Category = "ACF|Location")
    float Displacement = 0.f;

    UPROPERTY(BlueprintReadOnly, Category = "ACF|Location")
    float DisplacementDelta = 0.f;

    UPROPERTY(BlueprintReadOnly, Category = "ACF|Location")
    FVector OwnerLocation;

    UPROPERTY(BlueprintReadOnly, Category = "ACF|Rotation")
    FRotator OwnerRotation;

    UPROPERTY(BlueprintReadOnly, Category = "ACF|Rotation")
    FRotator ControlRotation;

    UPROPERTY(BlueprintReadOnly, Category = "ACF|Rotation")
    FRotator PreviousRotation;

    UPROPERTY(BlueprintReadOnly, Category = "ACF|Rotation")
    float YawSpeed = 0.f;

    UPROPERTY(BlueprintReadOnly, Category = "ACF|Rotation")
    float YawDelta = 0.f;

    UPROPERTY(BlueprintReadWrite, Category = "ACF|Rotation")
    float YawOffset = 0.f;

    UPROPERTY(BlueprintReadOnly, Category = "ACF|Velocity")
    bool bWasMoving;

    UPROPERTY(BlueprintReadOnly, Category = "ACF|Velocity")
    bool bIsMoving;

    UPROPERTY(BlueprintReadOnly, Category = "ACF|Velocity")
    float Direction = 0.f;

    UPROPERTY(BlueprintReadOnly, Category = "ACF|Velocity")
    float DirectionWithOffset = 0.f;

    UPROPERTY(BlueprintReadOnly, Category = "ACF|Velocity")
    float Speed = 0.f;

    UPROPERTY(BlueprintReadOnly, Category = "ACF|Velocity")
    float DeltaInputDirection = 0.f;

    UPROPERTY(BlueprintReadOnly, Category = "ACF|Velocity")
    float NormalizedSpeed = 0.f;

    UPROPERTY(BlueprintReadOnly, Category = "ACF|Velocity")
    float PreviousSpeed = 0.f;

    UPROPERTY(BlueprintReadOnly, Category = "ACF|Velocity")
    float NormalizedSwimSpeed = 0.f;

    UPROPERTY(BlueprintReadOnly, Category = "ACF|Velocity")
    FVector WorldSpeed;

    UPROPERTY(BlueprintReadOnly, Category = "ACF|Velocity")
    FVector WorldSpeed2D;

    UPROPERTY(BlueprintReadOnly, Category = "ACF|Velocity")
    FVector LocalSpeed2D;

    UPROPERTY(BlueprintReadOnly, Category = "ACF|Velocity")
    EACFDirection SpeedDirection;

    UPROPERTY(BlueprintReadOnly, Category = "ACF|Velocity")
    EACFDirection SpeedDirectionWithOffset;

    UPROPERTY(BlueprintReadOnly, Category = "ACF|Velocity")
    EACFDirection LastSpeedDirection;

    UPROPERTY(BlueprintReadOnly, Category = "ACF|Velocity")
    EACFDirection LastSpeedDirectionWithOffset;

    UPROPERTY(BlueprintReadOnly, Category = "ACF|AimOffset")
    FVector2D AimOffset;

    /* True while the aim offset is aimed at the locked target instead of the camera direction. */
    UPROPERTY(BlueprintReadOnly, Category = "ACF|AimOffset")
    bool bIsLockedOnTarget = false;

    UPROPERTY(BlueprintReadWrite, Category = "ACF|AimOffset")
    FTransform RootTrans;

    UPROPERTY(BlueprintReadOnly, Category = "ACF|AimOffset")
    float TurnRate;

    UPROPERTY(BlueprintReadOnly, Category = "ACF|Jump")
    bool bIsInAir;

    UPROPERTY(BlueprintReadOnly, Category = "ACF|Jump")
    bool bIsJumping;

    UPROPERTY(BlueprintReadOnly, Category = "ACF|Jump")
    bool bIsFalling;

    UPROPERTY(BlueprintReadOnly, Category = "ACF|Jump")
    float GroundDistance = -1.0f;

    UPROPERTY(BlueprintReadOnly, Category = "ACF|Jump")
    float TimeToApex = 0.f;

    UPROPERTY(BlueprintReadOnly, Category = "ACF|Acceleration")
    bool bIsAccelerating;

    UPROPERTY(BlueprintReadOnly, Category = "ACF|Acceleration")
    float Acceleration = 0.f;

    UPROPERTY(BlueprintReadOnly, Category = "ACF|Acceleration")
    FVector Acceleration2D;

    UPROPERTY(BlueprintReadOnly, Category = "ACF|Acceleration")
    FVector NormalizedAccel;

    UPROPERTY(BlueprintReadOnly, Category = "ACF|Acceleration")
    FVector LocalAccel2D;

    UPROPERTY(BlueprintReadOnly, Category = "ACF|Acceleration")
    FVector PivotDirection;

    UPROPERTY(BlueprintReadWrite, Category = "ACF|Acceleration")
    float TimeSinceLastPivot;

    UPROPERTY(BlueprintReadOnly, Category = "ACF|Acceleration")
    EACFDirection AccelerationDirection;

    UPROPERTY(BlueprintReadOnly, Category = "ACF|Leaning")
    float LeanAngle;

    UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "ACF|IK")
    bool EnableFootIK;

    UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "ACF|IK")
    bool EnableHandIK;

    bool bCachedFootIKEnabled = true;

    UPROPERTY(BlueprintReadOnly, Category = ACF)
    class ACharacter* CharacterOwner;

    UPROPERTY(BlueprintReadOnly, Category = ACF)
    class UACFCharacterMovementComponent* MovementComp;

    /* Cached on the Game Thread in SetReferences(). Never call FindComponentByClass from
     * NativeThreadSafeUpdateAnimation - it runs off the Game Thread and can race with
     * components being added/removed, corrupting the owner's component array mid-iteration. */
    UPROPERTY(BlueprintReadOnly, Category = ACF)
    class UACFClimbingComponent* ClimbingComponent;

    /* Targeting component that owns the lock-on target. Lives on the controller, so it is
     * re-resolved on the Game Thread whenever the possessing controller changes. */
    UPROPERTY(BlueprintReadOnly, Category = ACF)
    class UATSBaseTargetComponent* TargetComponent;

    /* Optional: only characters that anchor their pose to the camera have one. Cached on the
     * Game Thread in SetReferences() for the same reason as the components above. */
    UPROPERTY(BlueprintReadOnly, Category = ACF)
    class UACFCameraAimComponent* CameraAimComponent;

    /* Game Thread copies of the camera aim state, read through the two thread-safe getters. */
    UPROPERTY(Transient, BlueprintReadOnly, Category = "ACF|CameraAim")
    bool bCameraAimActive = false;

    UPROPERTY(Transient, BlueprintReadOnly, Category = "ACF|CameraAim")
    FRotator CameraViewRotation = FRotator::ZeroRotator;

    /* Optional: only characters that can switch to first person have one. Cached on the Game
     * Thread in SetReferences() for the same reason as the components above. */
    UPROPERTY(BlueprintReadOnly, Category = ACF)
    class UACFFirstPersonComponent* FirstPersonComponent;

    /* Game Thread copy of the first person state, read through IsInFirstPerson. */
    UPROPERTY(Transient, BlueprintReadOnly, Category = "ACF|FirstPerson")
    bool bIsInFirstPerson = false;

    /* Game Thread copy of the aiming alpha, read through GetAimingAlpha. */
    UPROPERTY(Transient, BlueprintReadOnly, Category = "ACF|Aiming")
    float AimingAlpha = 0.f;

    /* Game Thread copy of the held item's reference bone, read through GetItemReferenceBone. */
    UPROPERTY(Transient, BlueprintReadOnly, Category = "ACF|FirstPerson")
    FName ItemReferenceBone;

    /* Game Thread copy of the held item's hand offsets, read through GetHandOffsetData. */
    UPROPERTY(Transient, BlueprintReadOnly, Category = "ACF|FirstPerson")
    FACFProceduralHandOffsetData HandOffsetData;

    /* Game Thread copy of the target point location, refreshed every frame from the targeting
     * component. Animation threads must never resolve the target's transform themselves. */
    UPROPERTY(Transient, BlueprintReadOnly, Category = "ACF|AimOffset")
    FVector TargetLookAtLocation = FVector::ZeroVector;

    /* True while TargetLookAtLocation holds a valid target position. */
    UPROPERTY(Transient, BlueprintReadOnly, Category = "ACF|AimOffset")
    bool bHasTargetLookAt = false;

    /* Game Thread copy of the rotation the aim offset is built from: target based while locked,
     * camera based otherwise. Read it through Get Aim Rotation. */
    UPROPERTY(Transient, BlueprintReadOnly, Category = "ACF|AimOffset")
    FRotator AimRotation = FRotator::ZeroRotator;
    // ----- END READ ONLY VARIABLES ---- //

    // ----- END CONFIG ---- //

    UFUNCTION(BlueprintCallable, Category = ACF)
    void SetTurnInPlaceYawOffset(float inYawOffset);

    UFUNCTION(BlueprintNativeEvent, Category = ACF)
    EACFDirection GetDirectionFromAngle(float angle, EACFDirection currentDirection, bool bUseCurrentDirection);

    UPROPERTY(BlueprintReadOnly, Category = "ACF|Acceleration")
    EACFDirection DesiredStartDirection;

private:
    // Sampled on the Game Thread even when bUpdateMovementData is disabled by an MM template.
    bool bAimOffsetStrafing = false;

    void SetReferences();

    /* Re-resolves TargetComponent when the possessing controller changed. Game Thread only,
     * for the same reason as SetReferences(). */
    void UpdateTargetComponentRef();

    /* Refreshes everything the aim offset reads: the target point (from the targeting component)
     * and the aim rotation. Game Thread only, it reads another actor's transform and the controller. */
    void UpdateTargetData();

    /* Copies the camera aim state the ACFCameraAim node reads. Game Thread only. */
    void UpdateCameraAimData();

    /* Copies the first person state. Game Thread only. */
    void UpdateFirstPersonData();

    /* Controller TargetComponent was resolved from. */
    TWeakObjectPtr<class AController> cachedTargetController;

    /* True once SetReferences() has successfully resolved CharacterOwner/MovementComp/
     * ClimbingComponent at least once. Guards NativeUpdateAnimation so we don't redo a
     * FindComponentByClass lookup every Game Thread tick once resolved. */
    bool bReferencesResolved = false;

    void UpdateLeaning(float deltatime);
    void UpdateLocation(float deltatime);
    void UpdateRotation(float deltatime);
    void UpdateVelocity(float deltatime);

    void UpdateAimData(float deltatime);
    void UpdateAcceleration(float deltatime);
    void UpdateStateData(float deltatime);
    void UpdateJump(float deltatime);

    FVector CalculateRelativeAccelerationAmount() const;

    UFUNCTION()
    void HandleTargetLocomotionStateChanged(ELocomotionState newState);
};
