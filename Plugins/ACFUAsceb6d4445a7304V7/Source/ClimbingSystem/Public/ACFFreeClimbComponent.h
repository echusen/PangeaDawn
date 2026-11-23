// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ACFRPGTypes.h"
#include "ActiveGameplayEffectHandle.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "ACFFreeClimbTypes.h"
#include "ACFFreeClimbStateTypes.h"
#include "States/ACFFreeClimbState.h"
#include "ACFFreeClimbNetworkTypes.h"
#include "ACFFreeClimbComponent.generated.h"

class AActor;
class ACharacter;
class UAbilitySystemComponent;
class UCapsuleComponent;
class UPrimitiveComponent;
class UCharacterMovementComponent;
class USkeletalMeshComponent;
class USpringArmComponent;
class UPhysicalAnimationComponent;
class UAnimMontage;
class UACFFreeClimbableComponent;
class UACFFreeClimbAnimInstance;
class UACFFreeClimbRigProfile;
class FACFFreeClimbState_Idle;
class FACFFreeClimbState_Entering;
class FACFFreeClimbState_Climb;
class FACFFreeClimbState_Jump;
class FACFFreeClimbState_Cling;
class FACFFreeClimbState_ClimbUp;
class FACFFreeClimbState_ClimbExit;
class FLifetimeProperty;

/** Broadcast when the character starts or stops climbing: true when a climb session begins,
 * false when it ends. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnACFClimbingChanged, bool, bIsClimbing);

/** Broadcast when the character starts or stops clinging to the surface. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnACFClingingChanged, bool, bIsClinging);

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class CLIMBINGSYSTEM_API UACFFreeClimbComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UACFFreeClimbComponent();
    virtual ~UACFFreeClimbComponent() override;

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    UFUNCTION(BlueprintCallable, Category = "ACF")
    bool StartClimbing();

    UFUNCTION(BlueprintCallable, Category = "ACF")
    void StopClimbing(EACFFreeClimbStopReason Reason = EACFFreeClimbStopReason::Manual);

    UFUNCTION(BlueprintPure, Category = "ACF")
    EACFFreeClimbState GetClimbState() const { return ClimbState; }

    UFUNCTION(BlueprintPure, Category = "ACF")
    bool IsClimbing() const;

    UFUNCTION(BlueprintPure, Category = "ACF")
    FVector GetSurfaceNormal() const { return CurrentSurface.SmoothedNormal; }

    UFUNCTION(BlueprintCallable, Category = "ACF")
    void SetClimbInput(FVector2D Input) { PendingClimbInput = Input.GetClampedToMaxSize(1.0f); }

    UFUNCTION(BlueprintCallable, Category = "ACF")
    void Jump() { bJumpPressed = true; }

    UFUNCTION(BlueprintCallable, Category = "ACF")
    void StopJumping() { bJumpPressed = false; }

    UFUNCTION(BlueprintCallable, Category = "ACF",
        meta = (DisplayName = "Drop From Surface",
        ToolTip = "Lets go of the climbing surface and falls, pushing slightly away from the wall. This is what a jump pressed with downward input does instead of hopping to a lower grip."))
    void DropFromSurface();

    UFUNCTION(BlueprintCallable, Category = "ACF",
        meta = (DisplayName = "Start Climb Jump Movement",
        ToolTip = "Starts moving the character along the current climb jump path. Called by the ACF Free Climb Start Jump Movement notify when Jump Movement Waits For Notify is enabled. Does nothing outside a climb jump."))
    void StartJumpMovement();

    UFUNCTION(BlueprintCallable, Category = "ACF")
    void StartCling();

    UFUNCTION(BlueprintCallable, Category = "ACF")
    void StopCling();

    UFUNCTION(BlueprintCallable, Category = "ACF", meta = (DisplayName = "Add Climb Collision Channel"))
    void AddCollisionChannel(TEnumAsByte<ECollisionChannel> Channel);

    UFUNCTION(BlueprintCallable, Category = "ACF", meta = (DisplayName = "Remove Climb Collision Channel"))
    void RemoveCollisionChannel(TEnumAsByte<ECollisionChannel> Channel);

    UFUNCTION(BlueprintPure, Category = "ACF", meta = (DisplayName = "Can Climb Actor"))
    bool CanClimbActor(AActor* Actor) const;

    UFUNCTION(BlueprintPure, Category = "ACF",
        meta = (DisplayName = "Can Afford Climb Jump",
        ToolTip = "True when the owner can currently pay Jump Gameplay Effect. Also true when no jump effect is set, when the owner has no ability system component, or when the effect cost cannot be evaluated up front: in all those cases there is nothing to check against."))
    bool CanAffordClimbJump() const;

    UFUNCTION(BlueprintPure, Category = "ACF",
        meta = (DisplayName = "Is Climb Movement Effect Active",
        ToolTip = "True while Climb Movement Gameplay Effect is applied on the owner, which only happens on the authority and only while the character is actually moving on the surface."))
    bool IsClimbMovementEffectActive() const { return bClimbMovementEffectActive; }

    UFUNCTION(BlueprintPure, Category = "ACF",
        meta = (DisplayName = "Is Climb Idle Effect Active",
        ToolTip = "True while Climb Idle Gameplay Effect is applied on the owner, which only happens on the authority and only while the character is on the surface without moving on it."))
    bool IsClimbIdleEffectActive() const { return bClimbIdleEffectActive; }

    UFUNCTION(BlueprintPure, Category = "ACF",
        meta = (DisplayName = "Is Climb Attribute Depleted",
        ToolTip = "True when Climb Depletion Attribute has reached Climb Depletion Threshold on the owner. False when no attribute is set, when the owner has no ability system component or when it has no attribute set for it: in all those cases there is nothing to run out of."))
    bool IsClimbAttributeDepleted() const;

    FACFFreeClimbAnimData GetAnimData() const;

    UACFFreeClimbRigProfile* GetRigProfile() const { return bRigProfileValid ? RigProfile : nullptr; }

    bool GetLimbContact(EACFFreeClimbLimb Limb, FACFFreeClimbContact& OutContact) const;

    UACFFreeClimbableComponent* GetCurrentClimbable() const { return AnchorClimbable; }

    /** Fires with true when a climb session starts and false when it ends. */
    UPROPERTY(BlueprintAssignable, Category = "ACF")
    FOnACFClimbingChanged OnClimbingChanged;

    /** Fires with true when the character starts clinging and false when it stops. */
    UPROPERTY(BlueprintAssignable, Category = "ACF")
    FOnACFClingingChanged OnClingingChanged;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
    friend class FACFFreeClimbState_Idle;
    friend class FACFFreeClimbState_Entering;
    friend class FACFFreeClimbState_Climb;
    friend class FACFFreeClimbState_Jump;
    friend class FACFFreeClimbState_Cling;
    friend class FACFFreeClimbState_ClimbUp;
    friend class FACFFreeClimbState_ClimbExit;
    friend class UACFFreeClimbAnimInstance;

    void EnterClimbState();
    void ExitClimbState();

    void InitializeStates();
    void TransitionTo(EACFFreeClimbState NextState, const FACFSurfaceHitResult* TransitionHit = nullptr);
    EACFFreeClimbState GetTransitionTarget() const { return TransitionTarget; }
    bool ConsumeTransitionHit(FACFSurfaceHitResult& OutHit);
    void ResetClimbRuntime();
    void RefreshSurfaceTickPrerequisite();
    void ClearSurfaceTickPrerequisite();
    
    //Network Routing
    bool IsNetworkedGame() const;
    bool UsesRemoteState() const;
    bool ShouldSendState() const;
    bool ConsumeMovementSendTick(float DeltaTime);
    float GetRemoteInterpolationDelay() const;
    
    // Local ->Server
    void BeginLocalSession();
    FACFFreeClimbRepState BuildRepState();
    void SendRepState(bool bReliable);
    void SendJumpRepState(FACFJumpRepState State);
    
    UFUNCTION(Server, Reliable)
    void ServerSendClimbStateReliable(FACFFreeClimbRepState State);
    
    UFUNCTION(Server, Unreliable)
    void ServerSendClimbStateUnreliable(FACFFreeClimbRepState State);
    
    UFUNCTION(Server, Reliable)
    void ServerSendJumpStateReliable(FACFJumpRepState State);
    
    // Server Relay
    bool AcceptServerState(FACFFreeClimbRepState& State);
    void ApplyServerState(const FACFFreeClimbRepState& State);
    bool AcceptServerJumpState(FACFJumpRepState& State);
    void ApplyServerJumpState(const FACFJumpRepState& State);
    
    // Remote Presentation
    UFUNCTION()
    void OnRep_ClimbState();
    
    UFUNCTION()
    void OnRep_JumpState();
    
    void ApplyRemoteState(const FACFFreeClimbRepState& State);
    void ApplyRemoteJumpState(const FACFJumpRepState& State);
    UACFFreeClimbableComponent* FindReplicatedClimbable(const FACFFreeClimbRepState& State) const;
    void PreventCharacterMoveCombining() const;
    void ApplyMovementReplicationPolicy(EACFFreeClimbState State);
    void EnsureAutonomousProxyMovementReplication() const;
    bool CanStartClimbUp(FACFSurfaceHitResult& OutHit) const;

    // Gameplay Effects
    UAbilitySystemComponent* GetOwnerAbilitySystem() const;
    bool CanAffordEffect(const FGameplayEffectConfig& EffectConfig) const;
    /** Requirement check plus consumption of the jump cost. Returns false when the jump must be refused. */
    bool TryConsumeClimbJumpCost();
    void ApplyClimbJumpCost();

    /** The owning client starts the jump locally, so the authority is asked to charge the cost. */
    UFUNCTION(Server, Reliable)
    void ServerApplyClimbJumpCost();

    /** True while the player is actually steering the character on the surface. */
    bool IsMovingOnSurface() const;
    bool ShouldApplyClimbMovementEffect() const;
    bool ShouldApplyClimbIdleEffect() const;

    /** Drives both continuous climb effects. Called once per tick and nowhere else. */
    void UpdateClimbGameplayEffects();
    void RemoveClimbGameplayEffects();

    /** Applies or removes one continuous effect, keeping its handle and its active flag in sync. */
    void UpdateContinuousEffect(const FGameplayEffectConfig& EffectConfig, bool bShouldBeActive, FActiveGameplayEffectHandle& Handle, bool& bEffectActive);
    void RemoveContinuousEffect(FActiveGameplayEffectHandle& Handle, bool& bEffectActive);

    /** Lets go of the surface when the watched attribute has run out. Called once per tick and nowhere else. */
    void UpdateAttributeDepletionDrop();

    bool CacheOwnerComponents();

    UFUNCTION()
    void OnClimbUpMontageNotify(FName NotifyName, const FBranchingPointNotifyPayload& BranchingPointPayload);
    void OnClimbUpMontageEnded(UAnimMontage* Montage, bool bInterrupted);
    void OnClimbExitMontageEnded(UAnimMontage* Montage, bool bInterrupted);

    bool QuerySurface(FACFSurfaceHitResult& OutHit) const;
    bool QuerySurfaceAt(const FVector& Point, const FVector& Normal, FACFSurfaceHitResult& OutHit) const;
    bool QuerySurfaceSegment(const FVector& Start, const FVector& End, FACFSurfaceHitResult& OutHit) const;
    bool QuerySkeletalSegment(const FVector& Start, const FVector& End, FACFSurfaceHitResult& OutHit) const;
    bool QueryStaticSegment(const FVector& Start, const FVector& End, FACFSurfaceHitResult& OutHit) const;
    bool QueryCornerCandidates(const FVector& Start, const FVector& End, TArray<FACFSurfaceHitResult>& OutHits) const;
    bool IsClimbable(const FACFSurfaceHitResult& Hit) const;
    bool PassesActorTagFilter(const AActor* Actor) const;
    bool AcceptsClimbablePrimitive(const UPrimitiveComponent* Primitive) const;
    bool HasClimbableCollisionChannel(ECollisionChannel Channel) const;
    bool ShouldAcceptClimbableActor(const AActor* Actor) const;

    void UpdateSurfaceFromHit(const FACFSurfaceHitResult& Hit);
    void BuildSurfaceBasis(FACFFreeClimbSurface& Surface) const;
    FVector ProjectSurfaceUp(const FVector& Normal) const;
    FVector GetAnchorAdjustedActorLocation(const FVector& AnchorPoint, const FVector& AnchorNormal,
        const FQuat& ActorRotation) const;
    FTransform ComputeClimbTransform() const;
    
    void ApplyClimbTransform();
    void ApplyClimbTransform(const FQuat& Rotation);
    
    bool ResolveAnchorRotation(FQuat& OutRotation) const;
    
    bool CaptureAnchorRotationOffset();
    bool CalculateClimbRotation(float DeltaTime, const FVector2D& MoveInput, FQuat& OutRotation) const;
    FQuat CalculateMovementSurfaceRotation(const FVector& ReferenceUp) const;

    void SetAnchorFromHit(const FACFSurfaceHitResult& Hit);
    bool ResolveAnchorWorld(FVector& OutPoint, FVector& OutNormal) const;

    bool IsSurfacePersistentForPose(const FVector& SurfacePoint, const FVector& SurfaceNormal,
        const FVector& SurfaceMoveDir, const UACFFreeClimbableComponent* ExpectedClimbable) const;
    void UpdatePoseTargetNormal(const FVector& NewPoint, const FVector& NewNormal,
        const FVector& NewMoveDir, const UACFFreeClimbableComponent* NewClimbable);

    void UpdateProbeContacts();
    bool MakeProfileProbe(EACFFreeClimbLimb Limb, FACFFreeClimbProbe& OutProbe) const;
    void CollectProbeHits(const FACFFreeClimbProbe& Probe, FACFFreeClimbProbeResult& OutResult) const;
    bool SweepSphereStaticMulti(const FACFSurfaceSphereSweepQuery& Query, TArray<FACFSurfaceHitResult>& OutHits) const;
    
    void ApplyLimbProbe(const FACFFreeClimbProbe& Probe, const FACFFreeClimbProbeResult& Result);

    bool FindBestContact(const FACFFreeClimbProbe& Probe, const TArray<FACFFreeClimbContactCandidate>& Candidates, FACFFreeClimbContact& OutContact) const;

    static int32 GetLimbIndex(EACFFreeClimbLimb Limb) { return static_cast<int32>(Limb); }

    void DrawRaycastQuery(const FVector& Start, const FVector& End, const FColor Color = FColor::Cyan) const;

private:
    // ===== Cached Components & Saved State =====
    UPROPERTY(Transient)
    TObjectPtr<ACharacter> OwnerCharacter = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<UCapsuleComponent> CapsuleComponent = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<UCharacterMovementComponent> MovementComponent = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<USkeletalMeshComponent> MeshComponent = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<UACFFreeClimbAnimInstance> ACFFreeClimbAnimInstance = nullptr;

    float JumpProgress = 0.0f;

    bool bPrevOrientRotationToMovement = true;
    bool bPrevUseControllerRotationYaw = false;
    
    bool bPrevIgnoreClientMovementErrorChecksAndCorrection = false;
    bool bPrevClientIgnoreMovementCorrections = false;
    bool bPrevReplicateMovement = true;

    TWeakObjectPtr<USkeletalMeshComponent> SurfaceTickPrerequisiteMesh;

    // ===== Surface Detection =====
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF",
        meta = (AllowPrivateAccess = "true", DisplayName = "Climb Rig Profile"))
    TObjectPtr<UACFFreeClimbRigProfile> RigProfile = nullptr;

    bool bRigProfileValid = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF",
        meta = (
        AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "90.0", Units = "Degrees",
        DisplayName = "Max Climb Surface Angle From Vertical",
        ToolTip = "Maximum climbable surface angle measured from vertical. 0 degrees is vertical and 90 degrees is horizontal."))
    float MaxClimbAngle = 50.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF",
        meta = (
        AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "90.0", Units = "Degrees",
        DisplayName = "Walk Transition Surface Angle From Vertical",
        ToolTip = "Surface angle at which the character will transition to walking mode."))
    float WalkEnterAngle = 55.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF",
        meta = (
        AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "90.0", Units = "Degrees",
        DisplayName = "Force Walk Transition Surface Angle",
        ToolTip = "Surface angle from vertical at which the character is forced to transition to walking mode."))
    float ForceWalkAngle = 80.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF",
        meta = (
        AllowPrivateAccess = "true", ClampMin = "0.0", Units = "cm/s",
        DisplayName = "Max Anchor Speed For Walk Transition"))
    float WalkEnterMaxAnchorSpeed = 50.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF",
        meta = (
        AllowPrivateAccess = "true", ClampMin = "0.0", Units = "cm",
        DisplayName = "Walk Transition Floor Contact Distance",
        ToolTip = "Maximum distance from the capsule base to the floor at which a downward climb transitions to walking. Keeps the transition near actual contact instead of snapping from far above."))
    float WalkEnterFloorContactDistance = 5.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF",
        meta = (
        AllowPrivateAccess = "true", ClampMin = "0.0", Units = "s",
        DisplayName = "Walk Transition Input Lock Duration",
        ToolTip = "Duration that movement input is ignored after a climb-to-walk transition. Intended to span the transition animation."))
    float WalkEnterInputLockDuration = 0.3f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF",
        meta = (
        AllowPrivateAccess = "true", ClampMin = "0.0", Units = "s",
        DisplayName = "Climb Exit Rotation Blend Duration",
        ToolTip = "Duration over which the capsule pitch and roll linearly interpolate back to upright during a climb exit, instead of snapping instantly."))
    float ClimbExitRotationBlendDuration = 0.25f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF",
        meta = (
        AllowPrivateAccess = "true", ClampMin = "-1.0", ClampMax = "1.0",
        DisplayName = "Minimum Facing Surface Dot",
        ToolTip = "Minimum dot product between the character forward vector and the direction toward the surface."))
    float MinFacingDot = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF",
        meta = (
        AllowPrivateAccess = "true", ClampMin = "0.0", Units = "cm",
        ToolTip = "Forward grab probe distance measured beyond the character capsule radius."))
    float GrabProbeDistance = 50.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF",
        meta = (
        AllowPrivateAccess = "true", Units = "cm",
        DisplayName = "Anchor Probe Offset",
        ToolTip = "Offset of the initial climb anchor probe from the capsule center, in capsule local space. Use this to align probing with the visual body center when the mesh root is offset from the character origin."))
    FVector AnchorProbeOffset = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF",
        meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "cm"))
    float SurfaceProbeStartOffset = 10.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF",
        meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "cm"))
    float SurfaceProbeDistance = 12.0f;

    /** Object types that can be climbed. Queries use object-type traces, not Visibility or other trace channels. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF",
        meta = (
        AllowPrivateAccess = "true",
        DisplayName = "Climbable Collision Channels",
        ToolTip = "Collision object types that can be climbed. Queries use object-type traces, not Visibility. Defaults to the Climbable object channel. Leave empty to disable static-world climbing."))
    TArray<TEnumAsByte<ECollisionChannel>> CollisionChannels;

    /** Positive actor-tag filter. If not empty, the hit actor must have every listed tag. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF",
        meta = (
        AllowPrivateAccess = "true",
        DisplayName = "Required Actor Tags",
        ToolTip = "Positive filter. If not empty, the hit actor must have all of these actor tags."))
    TArray<FName> RequiredActorTags;

    /** Negative actor-tag filter. An actor with any of these tags cannot be climbed. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF",
        meta = (
        AllowPrivateAccess = "true",
        DisplayName = "Blocking Actor Tags",
        ToolTip = "Negative filter. Actors that have any of these actor tags cannot be climbed."))
    TArray<FName> BlockingActorTags;

    TArray<FACFFreeClimbContact> LimbContacts;

    FVector LastDesiredClimbDirection = FVector::ZeroVector;

    // ===== Entering =====
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF|Entering",
        meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "Seconds"))
    float EnteringDuration = 0.2f;


    // ===== Climbing Movement =====
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF|Movement", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
    float ClimbSpeedScale = 1.0f;
    static constexpr float BaseHorizontalClimbSpeed = 50.f * (2.0f / 3.0f);
    static constexpr float BaseVerticalClimbSpeed = 50.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, AdvancedDisplay, Category = "ACF|Movement",
        meta = (AllowPrivateAccess = "true", ClampMin = "1.0", Units = "cm"))
    float MaxSubstepDistance = 15.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, AdvancedDisplay, Category = "ACF|Movement", meta = (AllowPrivateAccess = "true", ClampMin = "1"))
    int32 MaxSubstepsPerFrame = 8;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, AdvancedDisplay, Category = "ACF|Movement",
        meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "1.0"))
    float MinMovementUpProjectionLength = 0.1f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF|Movement|Corner",
        meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "cm"))
    float CornerSearchRadius = 12.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF|Movement|Corner",
        meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "180.0", Units = "Degrees"))
    float MaxCornerTurnDegrees = 120.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF|Movement|Corner",
        meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "cm"))
    float CornerBacktrackTolerance = 2.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF|Movement|Corner",
        meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "cm"))
    float CornerProbeReach = 4.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF|Movement|Corner", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "1.0"))
    float MinDirectProgressRatio = 0.2f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF|Movement|Corner",
        meta = (AllowPrivateAccess = "true", ClampMin = "1.0", ClampMax = "90.0", Units = "Degrees"))
    float StaticCornerWrapAngleStep = 15.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF|Movement|Corner",
        meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "1.0"))
    float StaticConcaveWallFacingDot = 0.2f;

    // ===== Rotation =====
    UPROPERTY(EditAnywhere, Category = "ACF|Rotation",
        meta = (ClampMin = "0.0"))
    float ClimbRotationSpeed = 30.0f;

    // ===== Smoothing =====
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF|Smoothing", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
    float NormalInterpSpeed = 10.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF|Smoothing|Pose Surface",
        meta = (
        AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "180.0", Units = "Degrees",
        DisplayName = "Max Immediate Pose Normal Change"))
    float SharpPoseNormalAngleDegrees = 40.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF|Smoothing|Pose Surface",
        meta = (
        AllowPrivateAccess = "true", ClampMin = "0.0", Units = "cm",
        DisplayName = "Pose Surface Validation Distance"))
    float MinPoseSurfaceTravelDistance = 20.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF|Smoothing|Pose Surface", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "180.0", Units = "Degrees"))
    float PoseSurfaceNormalToleranceDegrees = 20.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF|Smoothing|Pose Surface", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "1.0"))
    float MinPoseSurfaceProgressRatio = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF|Smoothing|Pose Surface", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "Seconds"))
    float PoseNormalSmoothTime = 0.12f;

    // ===== Jump =====
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF|Jump",
        meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "cm"))
    float JumpDistance = 200.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF|Jump",
        meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "Seconds"))
    float JumpDuration = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF|Jump",
        meta = (AllowPrivateAccess = "true",
        ToolTip = "When enabled, the character holds its grip at the start of a climb jump until the ACF Free Climb Start Jump Movement notify fires in the jump animation, then travels the path in the remaining jump time."))
    bool bJumpMovementWaitsForNotify = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, AdvancedDisplay, Category = "ACF|Jump",
        meta = (
        AllowPrivateAccess = "true", ClampMin = "1",
        DisplayName = "Jump Path Segment Count"))
    int32 JumpSegment = 16;

    // ===== GAS =====
    /** Continuous effect kept applied while the character moves on the surface, the way a glider
     * drains while it is open: it is applied when the climb starts moving and removed as soon as the
     * character hangs still, the climb session ends or the component is destroyed. Use an Infinite
     * duration effect with a Period to drain stamina over time - a durational one would expire and
     * not come back until the next movement. Leave the Effect empty to make climbing free. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF|GAS",
        meta = (AllowPrivateAccess = "true", DisplayName = "Climb Movement Gameplay Effect"))
    FGameplayEffectConfig ClimbMovementGameplayEffect;

    /** Continuous effect kept applied while the character is on the surface without moving on it:
     * entering, hanging still, clinging, jumping along the surface and mantling. It is swapped with
     * Climb Movement Gameplay Effect as the character starts and stops moving, and removed when the
     * climb session ends or the component is destroyed. Use an Infinite duration effect - a durational
     * one would expire and not come back until the next pause. Leave the Effect empty to leave the
     * attributes alone while the character hangs. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF|GAS",
        meta = (AllowPrivateAccess = "true", DisplayName = "Climb Idle Gameplay Effect"))
    FGameplayEffectConfig ClimbIdleGameplayEffect;

    /** Cost of a climb jump, the way an ability pays for itself: it is checked as a requirement
     * before the jump and consumed when the jump starts, so a character that cannot pay keeps
     * climbing instead of jumping. Use an Instant effect with a negative modifier on the stamina
     * attribute. Leave the Effect empty to make climb jumps free. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF|GAS",
        meta = (AllowPrivateAccess = "true", DisplayName = "Jump Gameplay Effect"))
    FGameplayEffectConfig JumpGameplayEffect;

    /** When enabled, the character lets go of the surface as soon as Climb Depletion Attribute runs
     * out, falling exactly like a manual drop. Off by default: without an attribute to watch nothing
     * can run out, so climbing never ends on its own. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF|GAS",
        meta = (AllowPrivateAccess = "true", DisplayName = "Drop When Attribute Depleted"))
    bool bDropWhenAttributeDepleted = false;

    /** Attribute watched while climbing, usually the stamina the climb effects drain. The character
     * drops when it reaches Climb Depletion Threshold. Leaving it unset, or picking an attribute the
     * owner does not have, leaves the climb free: nothing is watched and nothing drops. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF|GAS",
        meta = (AllowPrivateAccess = "true", DisplayName = "Climb Depletion Attribute",
        EditCondition = "bDropWhenAttributeDepleted"))
    FGameplayAttribute ClimbDepletionAttribute;

    /** Value at or below which the watched attribute counts as run out. Raise it above zero to make
     * the character let go while it still has a reserve left. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF|GAS",
        meta = (AllowPrivateAccess = "true", ClampMin = "0.0", DisplayName = "Climb Depletion Threshold",
        EditCondition = "bDropWhenAttributeDepleted"))
    float ClimbDepletionThreshold = 0.0f;

    // ===== Drop =====
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF|Drop",
        meta = (
        AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "1.0",
        DisplayName = "Drop Input Threshold",
        ToolTip = "How far down the climb input must be pushed for a jump to release the surface instead of jumping along it. A slight downward drift while jumping sideways stays a jump."))
    float DropInputThreshold = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF|Drop",
        meta = (
        AllowPrivateAccess = "true", ClampMin = "0.0", Units = "cm/s",
        DisplayName = "Drop Push Back Speed",
        ToolTip = "Speed given away from the surface when dropping, so the release reads as letting go instead of sliding down the wall."))
    float DropPushBackSpeed = 150.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF|Drop",
        meta = (
        AllowPrivateAccess = "true", ClampMin = "0.0", Units = "s",
        DisplayName = "Drop Regrab Delay",
        ToolTip = "Time after a drop during which the surface cannot be grabbed again, so the character actually falls away instead of re-attaching on the next frame."))
    float DropRegrabDelay = 0.3f;

    // ===== Climb Up =====
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF|Climb Up", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "cm"))
    float ClimbUpWalkableProbeDistance = 20.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF|Climb Up", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "180.0", Units = "Degrees"))
    float MaxClimbUpCharacterTiltAngle = 60.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF|Climb Up", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "2.0"))
    float ClimbUpGripHeightRatio = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF|Climb Up", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
    float ClimbUpClearanceLengthScale = 2.0f;

    // ===== Cling =====
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF|Cling",
        meta = (
        AllowPrivateAccess = "true",
        ClampMin = "0.0", Units = "cm/s^2"))
    float ClingGravity = 980.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF|Cling",
        meta = (
        AllowPrivateAccess = "true",
        ClampMin = "0.0", ClampMax = "1.0"))
    float ClingDamping = 0.02f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF|Cling",
        meta = (
        AllowPrivateAccess = "true",
        ClampMin = "0.0"))
    float AnchorDragCoef = 0.4f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF|Cling",
        meta = (
        AllowPrivateAccess = "true",
        ClampMin = "0.0"))
    float ArmLengthScale = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF|Cling",
        meta = (
        AllowPrivateAccess = "true",
        ClampMin = "0.0"))
    float ClingHandOffset = 20.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF|Cling",
        meta = (AllowPrivateAccess = "true"))
    bool bClingCollision = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF|Cling",
        meta = (
        AllowPrivateAccess = "true", EditCondition = "bClingCollision",
        ClampMin = "0.0", Units = "cm"))
    float ClingSurfacePadding = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF|Cling",
        meta = (
        AllowPrivateAccess = "true", EditCondition = "bClingCollision",
        ClampMin = "0.0", Units = "cm",
        DisplayName = "Cling Body Sphere Radius",
        ToolTip = "Torso collision sphere radius used while clinging. A value of 0 uses the character capsule radius."))
    float ClingProbeRadius = 0.0f;

    // ===== Leg Physics =====
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF|LegPhysics",
        meta = (AllowPrivateAccess = "true"))
    TArray<FName> LegRootBones = { "thigh_l", "thigh_r" };

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF|LegPhysics",
        meta = (
        AllowPrivateAccess = "true",
        ClampMin = "0.0", ClampMax = "1.0"))
    float ClingLegBlend = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF|LegPhysics",
        meta = (
        AllowPrivateAccess = "true",
        ClampMin = "0.0"))
    float LegOrientationStrength = 1000.0f;

    UPROPERTY(Transient)
    TObjectPtr<UPhysicalAnimationComponent> PhysicalAnim = nullptr;

    // ===== Debug =====
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF|Debug", meta = (AllowPrivateAccess = "true"))
    bool bDrawDebug = false;

    // ===== Runtime State =====
    UPROPERTY(VisibleInstanceOnly, Category = "ACF|Runtime", meta = (AllowPrivateAccess = "true"))
    FACFFreeClimbSurface CurrentSurface;

    UPROPERTY(Transient)
    TObjectPtr<UACFFreeClimbableComponent> AnchorClimbable = nullptr;

    EACFFreeClimbState ClimbState = EACFFreeClimbState::Idle;
    TMap<EACFFreeClimbState, TUniquePtr<IACFFreeClimbState>> States;
    IACFFreeClimbState* CurrentState = nullptr;
    EACFFreeClimbState TransitionTarget = EACFFreeClimbState::Idle;
    bool bHasTransitionHit = false;
    FACFSurfaceHitResult TransitionHit;
    bool bClimbModeActive = false;

    /** Movement input pushed from Blueprint while climbing. X = lateral (right), Y = vertical (up).
     * Enhanced Input "Triggered" keeps firing every frame while the axis is held, so this value is
     * latched into ClimbInput and cleared at the start of every tick: releasing the stick zeroes the
     * climb input on its own, the same contract as AddMovementInput / ConsumeInputVector.
     * Set it directly from the movement input event, or through SetClimbInput() which also clamps it. */
    UPROPERTY(BlueprintReadWrite, Category = "ACF|Movement", meta = (AllowPrivateAccess = "true"))
    FVector2D PendingClimbInput = FVector2D::ZeroVector;

    /** Movement input the climb states consume this frame, latched from PendingClimbInput. */
    FVector2D ClimbInput = FVector2D::ZeroVector;

    bool bPendingExitToWalking = false;

    // SkeletalMesh Anchor Data
    int32   AnchorTriangleIndex = INDEX_NONE;
    FVector AnchorBarycentric   = FVector::ZeroVector;
    bool    bAnchorValid        = false;

    // StaticMesh Anchor Data
    EACFSurfaceSource AnchorSource = EACFSurfaceSource::Skeletal;
    TWeakObjectPtr<UPrimitiveComponent> AnchorPrimitive;
    FVector AnchorLocalPos = FVector::ZeroVector;
    FVector AnchorLocalNormal = FVector::ZeroVector;

    /** Pending climb jump request. Consumed (and cleared) when the climb state starts the jump, so a
     * held button gives one jump instead of hopping up the surface. Set it directly, or through Jump(). */
    UPROPERTY(BlueprintReadWrite, Category = "ACF|Jump", meta = (AllowPrivateAccess = "true"))
    bool bJumpPressed = false;

    /** Handle of the climb movement effect while it is applied, used to remove exactly that effect. */
    FActiveGameplayEffectHandle ClimbMovementEffectHandle;

    /** True between the application and the removal of the climb movement effect. */
    bool bClimbMovementEffectActive = false;

    /** Handle of the climb idle effect while it is applied, used to remove exactly that effect. */
    FActiveGameplayEffectHandle ClimbIdleEffectHandle;

    /** True between the application and the removal of the climb idle effect. */
    bool bClimbIdleEffectActive = false;

    /** World time of the last drop, used to keep the surface ungrabbable for DropRegrabDelay. */
    float LastDropTimeSeconds = -BIG_NUMBER;

    /** Release velocity of the drop in flight, read by the Idle rep state so remotes match it. */
    FVector PendingDropVelocity = FVector::ZeroVector;

    // Rotation Anchor Data
    FQuat AnchorRotationOffset = FQuat::Identity;
    bool bHasAnchorRotationOffset = false;
    
    // ===== Client-Authority Network =====
    UPROPERTY(ReplicatedUsing = OnRep_ClimbState)
    FACFFreeClimbRepState ReplicatedClimbState;
    
    UPROPERTY(ReplicatedUsing = OnRep_JumpState)
    FACFJumpRepState ReplicatedJumpState;
    
    uint16 LocalSessionId = 0;
    uint16 LocalSequence = 0;
    
    float LocalStateStartServerTimeSeconds = 0.0f;
    
    uint16 RemoteSessionId = 0;
    
    FVector2D RemoteVisualMoveInput = FVector2D::ZeroVector;
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF|Network", meta = (AllowPrivateAccess = "true", ClampMin = "1.0", Units = "Hz"))
    float MovementStateSendRate = 20.0f;
    
    float MovementStateSendAccumulator = 0.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF|Network",
        meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "Seconds"))
    float RemoteInterpolationDelay = 0.0f;
};
