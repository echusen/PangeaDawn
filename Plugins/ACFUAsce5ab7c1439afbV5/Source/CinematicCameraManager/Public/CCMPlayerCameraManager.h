// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CCMTypes.h"
#include "Camera/PlayerCameraManager.h"
#include "CoreMinimal.h"
#include <Engine/DataTable.h>
#include <GameFramework/Actor.h>

#include "ACFCameraPointComponent.h"
#include "CCMPlayerCameraManager.generated.h"

class APawn;
class AActor;
class ALevelSequenceActor;
class USceneComponent;
class UACFCameraPointComponent;

/**
 * Delegate that is called when a camera sequence starts.
 * @param cameraSequence - The name of the camera sequence that started.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCameraSequenceStarted, const FName&, cameraSequence);

/**
 * Delegate that is called when a camera sequence ends.
 * @param cameraSequence - The name of the camera sequence that ended.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCameraSequenceEnded, const FName&, cameraSequence);

/**
 * Player Camera Manager class that handles the camera sequence, movements, and events.
 * Inherits from APlayerCameraManager to manage the player's camera during gameplay.
 */
UCLASS()
class CINEMATICCAMERAMANAGER_API ACCMPlayerCameraManager : public APlayerCameraManager {
	GENERATED_BODY()

public:
	// Default constructor for the camera manager class
	ACCMPlayerCameraManager();

protected:
	/** Called when the game starts or when the actor is spawned */
	virtual void BeginPlay() override;

	/** Function to update the camera state every frame */
	void UpdateCamera(float deltaTime);

	/** Data table to store various camera movements (should be populated via Editor) */
	UPROPERTY(EditAnywhere, Category = "CCM Camera")
	UDataTable* CameraMovements;

	/** Absolute camera setups keyed by gameplay tag. Camera events remain deltas on top of the
	 *  active preset: their offset is added to Socket Offset and their FOV is added to FOV. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CCM Camera", meta = (DisplayName = "Camera Presets"))
	TMap<FGameplayTag, FCCMCameraPreset> CameraPresets;

	/** Preset applied when the pawn camera is resolved, until Apply Camera Preset selects another. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CCM Camera")
	FGameplayTag DefaultCameraPreset;

	/** Upper pitch limit for camera lock (in degrees) */
	UPROPERTY(EditAnywhere, Category = "CCM Camera")
	float CameraLockPitchUpperLimit = 75.f;

	/** Lower pitch limit for camera lock (in degrees) */
	UPROPERTY(EditAnywhere, Category = "CCM Camera")
	float CameraLockPitchLowerLimit = 35.f;

	/** Offset for global camera lock */
	UPROPERTY(EditAnywhere, Category = "CCM Camera")
	FVector2D GlobalLockOffset = FVector2D();

	/**
	 * Optional cap on the lock-on re-center speed, in degrees per second.
	 * 0 (the default) means uncapped: the camera re-centers as fast as the lock strength asks.
	 * Set it to something like 240 only if a hit-react yaw or a big target jump is whipping
	 * the camera too hard for comfort. Lower values feel calmer but make the lock lag behind
	 * fast-moving targets.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CCM Camera", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "720.0", ForceUnits = "deg/s"))
	float MaxLockRotationRate = 0.f;

	/**
	 * If the lock target is closer than this (cm) to the camera, skip look-at this frame
	 * and keep the current control rotation. Prevents FindLookAtRotation degenerating when
	 * the player rolls through the target (camera and target occupy the same point).
	 * Default 30cm. Set to 0 to disable.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CCM Camera", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "200.0", ForceUnits = "cm"))
	float MinLockTargetDistance = 30.f;

	/** Class type for the camera actor to use */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CCM Camera")
	TSubclassOf<ACameraActor> CameraActorClass;

	/** Holds the final camera movement settings (adjustable in Blueprint) */
	UPROPERTY(BlueprintReadOnly, Category = "CCM Camera")
	FCCMCameraMovementSettings FinalMov;

	/** Reference to the player's camera component */
	UPROPERTY(BlueprintReadOnly, Category = "CCM Camera")
	class UCameraComponent* playerCamera;

	/** Reference to the spring arm component for the camera */
	UPROPERTY(BlueprintReadOnly, Category = "CCM Camera")
	class USpringArmComponent* cameraBoom;

	/** If true, the camera will follow the player character */
	UPROPERTY(EditDefaultsOnly, Category = "CCM Camera")
	bool bFollowPlayer = false;

	/** Make the player's spring arm probe on the Camera channel as soon as the manager takes it
	 *  over. ACF characters clear their response to that channel, so this pair is what keeps an
	 *  enemy walking in front of the camera from shortening the arm and making the camera jump.
	 *  Turn it off to keep whatever probe channel the pawn's spring arm was authored with. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "CCM Camera")
	bool bEnforceCameraProbeChannel = true;

	/** Speed at which the camera will follow the player when the player moves */
	UPROPERTY(EditDefaultsOnly, meta = (EditCondition = "bFollowPlayerWhenMoving"), Category = "CCM Camera")
	float FollowSpeed = 1.f;

	/** Maximum angle difference at which the camera will follow the player */
	UPROPERTY(EditDefaultsOnly, meta = (EditCondition = "bFollowPlayerWhenMoving"), Category = "CCM Camera")
	float MaxFollowDeltaAngle = 180.f;

	/** Debugging setting to print the delta angle when following the player */
	UPROPERTY(EditDefaultsOnly, meta = (EditCondition = "bFollowPlayerWhenMoving"), Category = "CCM Camera")
	bool bPrintDebugDeltaAngle = false;

	/** Flag indicating if a camera sequence is currently playing */
	UPROPERTY()
	bool bIsPlayingSequence = false;

	/** The current camera actor playing a sequence */
	UPROPERTY()
	TObjectPtr<class ACameraActor> sequenceCameraActor;

	/** Current sequence settings */
	UPROPERTY()
	FCurrentSequence currentSequence;

	/** Resolves the current lock target every frame and feeds its position to LockCameraOnPosition. */
	virtual void UpdateLockOnTarget(float deltaTime);

	/**
	 * Re-centers the control rotation toward the lock target position.
	 * Called every frame while a camera lock is active. Override it in Blueprint to write your own
	 * lock-on feel (custom easing, deadzone, per-target speed) instead of the default interpolation.
	 * @param localPos World position the camera should look at.
	 * @param deltaTime Seconds since the last camera update.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "CCM Camera", meta = (DisplayName = "Lock Camera On Position", Keywords = "lock on camera target look at rotation"))
	void LockCameraOnPosition(FVector localPos, float deltaTime);
	virtual void LockCameraOnPosition_Implementation(FVector localPos, float deltaTime);

	/** Aims between the player and framedActor and widens the arm as much as the composition asks
	 * for. Does nothing when the framing mode was never started. */
	void UpdateActorFraming(float deltaTime);

	/**
	 * Moves the camera toward keeping the player and framedActor inside the requested fraction of
	 * the screen. Override it in Blueprint to compose the shot your own way, reading Get Framing
	 * Composition for the rotation and arm length this frame asks for.
	 * @param deltaTime Seconds since the last camera update.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "CCM Camera", meta = (DisplayName = "Frame Actor Pair", Keywords = "framing composition two actors on screen thirds"))
	void FrameActorPair(float deltaTime);
	virtual void FrameActorPair_Implementation(float deltaTime);

	/** Writes the spring arm's socket offset as the sum of the camera events' share and the
	 * framing's, so that neither of the two can overwrite the other's. */
	void ApplyFramingSocketOffset();

	UPROPERTY(Transient)
	TObjectPtr<AActor> framedActor;

	/** Composition asked for by whoever started the framing. */
	UPROPERTY(Transient)
	FCCMActorFramingSettings framingSettings;

	/** Arm length the camera had when framing started, restored when it stops. */
	UPROPERTY(Transient)
	float framingBaseArmLength = 0.f;

	/** Sideways shift the framing is asking of the spring arm, on top of the camera events' own
	 * offset. Kept apart from it so the two never overwrite each other. */
	UPROPERTY(Transient)
	FVector framingSocketOffset = FVector::ZeroVector;

	/** The camera events' share of the spring arm socket offset, interpolated by MoveCamera. */
	UPROPERTY(Transient)
	FVector eventSocketOffset = FVector::ZeroVector;

	/** True between StartFramingActors and StopFramingActors. */
	UPROPERTY(Transient)
	bool bIsFramingActors = false;

	/** True while the arm is interpolating back to framingBaseArmLength after StopFramingActors. */
	UPROPERTY(Transient)
	bool bIsRestoringFraming = false;

	UPROPERTY()
	float _lockStrength;

	UPROPERTY()
	ELockType currentLockType;

	UPROPERTY()
	ETargetLockType targetLockType = ETargetLockType::ENone;

	UPROPERTY()
	TObjectPtr<AActor> localtarget;

	UPROPERTY()
	TObjectPtr<USceneComponent> localtargetComponent;

	UPROPERTY()
	TObjectPtr<APawn> playerCharacter;

public:


	/** Event that gets triggered when a camera sequence starts */
	UPROPERTY(BlueprintAssignable, Category = "CCM")
	FOnCameraSequenceStarted OnCameraSequenceStarted;

	/** Event that gets triggered when a camera sequence ends */
	UPROPERTY(BlueprintAssignable, Category = "CCM")
	FOnCameraSequenceEnded OnCameraSequenceEnded;

	/** Trigger a camera event by name (e.g., cinematic sequences, cutscenes) */
	UFUNCTION(BlueprintCallable, Category = "CCM")
	void TriggerCameraEvent(FName CameraEventName);

	/** Trigger a timed camera event that lasts for a specified duration */
	UFUNCTION(BlueprintCallable, Category = "CCM")
	void TriggerTimedCameraEvent(FName CameraEventName, float duration);

	/** Stop a camera event by its name */
	UFUNCTION(BlueprintCallable, Category = "CCM")
	void StopCameraEvent(FName CameraEventName);

	/** Reset the camera position (instant or gradual) */
	UFUNCTION(BlueprintCallable, Category = "CCM")
	void ResetCameraPosition(bool bInstantReset = false);

	/**
	 * Replaces the runtime base camera state without removing active camera events or actor framing.
	 * Socket offsets and target arm lengths use centimeters; field of view uses degrees.
	 */
	UFUNCTION(BlueprintCallable, Category = "CCM", meta = (DisplayName = "Apply Camera Base Preset"))
	void ApplyCameraBasePreset(FVector NewBaseSocketOffset, float NewBaseFOV, float NewBaseTargetArmLength);

	/**
	 * Applies the camera preset registered under PresetTag. Active camera events stay in place as
	 * deltas on the new preset, and actor framing keeps the arm length it already added.
	 * @return False when the tag is not in Camera Presets, or the pawn camera is not resolved yet.
	 */
	UFUNCTION(BlueprintCallable, Category = "CCM", meta = (DisplayName = "Apply Camera Preset", Keywords = "camera preset gameplay tag spring arm fov"))
	bool ApplyCameraPreset(FGameplayTag PresetTag);

	/**
	 * Applies a preset directly. Same layering as Apply Camera Preset: camera events stay additive
	 * and framing keeps its arm contribution. Clears the active preset tag, since this value may not
	 * match any entry in Camera Presets.
	 */
	UFUNCTION(BlueprintCallable, Category = "CCM", meta = (DisplayName = "Apply Camera Preset Settings", Keywords = "camera preset spring arm fov lag"))
	void ApplyCameraPresetSettings(const FCCMCameraPreset& Preset);

	/** Tag of the preset last applied through Apply Camera Preset. Empty after a direct settings apply. */
	UFUNCTION(BlueprintPure, Category = "CCM", meta = (DisplayName = "Get Active Camera Preset"))
	FGameplayTag GetActiveCameraPreset() const { return ActiveCameraPreset; }

	/** Lock the camera on an actor with a specified lock type and strength. Stops the framing mode:
	 *  only one of the two camera behaviours is ever running. */
	UFUNCTION(BlueprintCallable, Category = "CCM")
	void LockCameraOnActor(class AActor* ActorLookAt, ELockType lockType, float lockStrength = 5.f);

	/** Lock the camera on a specific scene component with a specified lock type and strength. Stops
	 *  the framing mode: only one of the two camera behaviours is ever running. */
	UFUNCTION(BlueprintCallable, Category = "CCM")
	void LockCameraOnComponent(class USceneComponent* ComponentToLookAt, ELockType lockType, float lockStrength = 5.f);

	/** Stop locking the camera on the current actor */
	UFUNCTION(BlueprintCallable, Category = "CCM")
	void StopLookingActor();

	/**
	 * Frames the player together with ActorToFrame. This is not a lock: the camera keeps them both
	 * inside the requested fraction of the screen, from whichever side each is on, and slides the arm
	 * sideways to give them a side of the frame each - so neither sits dead center. Rotation, arm
	 * length and the framing's own share of the arm offset are all it touches, camera events keep
	 * owning their offset and the FOV. Starting it stops any camera lock, and calling it again
	 * reframes on the new actor with the new settings.
	 * @param ActorToFrame The other actor to keep in frame, usually the enemy being fought.
	 * @param Settings Screen fraction, arm cap, sideways room and interpolation speed.
	 * @param FramingStrength How hard the camera works at it, same scale as the lock strength.
	 */
	UFUNCTION(BlueprintCallable, Category = "CCM", meta = (DisplayName = "Start Framing Actors", AutoCreateRefTerm = "Settings", Keywords = "framing composition two actors on screen thirds zoom arm"))
	void StartFramingActors(class AActor* ActorToFrame, const FCCMActorFramingSettings& Settings, float FramingStrength = 15.f);

	/** Stops the framing mode and brings the arm length and the sideways shift back to what they were
	 *  when it started. */
	UFUNCTION(BlueprintCallable, Category = "CCM", meta = (DisplayName = "Stop Framing Actors", Keywords = "framing composition two actors on screen thirds zoom arm"))
	void StopFramingActors();

	/** @return The actor framed together with the player, or null when the framing mode is off. */
	UFUNCTION(BlueprintPure, Category = "CCM", meta = (DisplayName = "Get Framed Actor", Keywords = "framing composition two actors on screen thirds"))
	AActor* GetFramedActor() const { return framedActor; }

	/**
	 * The shot that would put the player and the framed actor inside the requested fraction of the
	 * screen right now. The rotation is the current one plus whatever angle is missing, so it comes
	 * back unchanged while both are already in frame. A target behind the player along the boom
	 * lengthens the arm instead of spinning the camera, so both stay in front of the lens.
	 * @param OutRotation Control rotation the composition asks for.
	 * @param OutArmLength Spring arm length the composition asks for.
	 * @param OutLateralOffset Sideways shift of the spring arm that gives them a side each.
	 * @return False when there is nothing to frame.
	 */
	UFUNCTION(BlueprintPure, Category = "CCM", meta = (DisplayName = "Get Framing Composition", Keywords = "framing composition two actors on screen thirds arm rotation"))
	bool GetFramingComposition(FRotator& OutRotation, float& OutArmLength, FVector& OutLateralOffset) const;

	UFUNCTION(BlueprintCallable, Category = "CCM")
	void PlayCameraBlend(AActor* TargetActor, const FGameplayTag& ComponentTag, float BlendTime = 1.f,
		TEnumAsByte<EViewTargetBlendFunction> BlendFunc = VTBlend_Linear,
		float BlendExp = 2.f,
		bool bLockOutgoing = false);

	/** Enable or disable camera following the player with an optional follow speed */
	UFUNCTION(BlueprintCallable, Category = "CCM")
	void SetFollowPlayer(bool bFollow, float inFollowSpeed = 1.f);

	/** Update references to the player's camera and boom */
	void UpdateCameraReferences();

	/** Trigger a camera sequence by event name and specify the camera actor owner */
	UFUNCTION(BlueprintCallable, Category = "CCM")
	void TriggerCameraSequence(AActor* cameraOwner, FName CameraEventName);

	/**
	 * Play a level sequence with actor bindings.
	 * Only one sequence can play at a time; starting a new one stops the previous.
	 * The reference to the sequence in progress is stored in CurrentLevelSequenceActor.
	 * @param Sequence - The level sequence to play
	 * @param TransformOrigin - Transform used as the origin point for the sequence (starting point)
	 * @param Bindings - Array of binding structs containing tag names and optional actors to bind
	 * @return The created LevelSequenceActor, or nullptr if creation failed
	 */
	UFUNCTION(BlueprintCallable, Category = "CCM", meta = (AutoCreateRefTerm = "TransformOrigin,Bindings"))
	ALevelSequenceActor* PlayGameplaySequenceWithBinding(
		ULevelSequence* Sequence,
		const FTransform& TransformOrigin,
		const TArray<FCCMSequenceBinding>& Bindings);

	/** Reference to the level sequence currently playing (max one at a time). Cleared when stopped or when the sequence finishes. */
	UPROPERTY(BlueprintReadOnly, Category = "CCM")
	TObjectPtr<ALevelSequenceActor> CurrentLevelSequenceActor = nullptr;

	/** Stops the sequence started with PlayGameplaySequenceWithBinding and clears CurrentLevelSequenceActor. No-op if none is playing. */
	UFUNCTION(BlueprintCallable, Category = "CCM")
	void StopSequence();

	/** Stop the current camera sequence */
	UFUNCTION(BlueprintCallable, Category = "CCM")
	void StopCurrentCameraSequence();

	/**
	 * Force-stops and deactivates the camera point currently activated via BlendToPoint/PlayCameraBlend
	 * (halting any orbit/crane/pan movement preset) and clears the internal reference.
	 * No-op if no camera point is active.
	 */
	UFUNCTION(BlueprintCallable, Category = "CCM")
	void DeactivateCurrentCameraPoint();

	/** Add a sequence event modifier to modify the current sequence behavior */
	UFUNCTION(BlueprintCallable, Category = "CCM")
	void AddSequenceEventModifier(const FCCMSequenceEvent& modifier);

	/** Remove a sequence event modifier */
	UFUNCTION(BlueprintCallable, Category = "CCM")
	void RemoveSequenceEventModifier(const FCCMSequenceEvent& modifier);

	/** Override the camera references (e.g., to set a new camera or boom) */
	UFUNCTION(BlueprintCallable, Category = "CCM")
	void OverrideCameraReferences(class UCameraComponent* inPlayerCamera, class USpringArmComponent* inCameraBoom);

	/** Binds the camera manager to the pawn change event */
	UFUNCTION(BlueprintCallable, Category = "CCM")
	void BindEvents();

	/*
	UFUNCTION(BlueprintCallable, Category = "CCM")
	void PlayCinematicCameraAnim(UCameraAnimationSequence* CameraAnimAsset, USceneComponent* ReferencePoint);*/


	void BlendToPoint(AActor* OwnerWithPoints,
		FGameplayTag CameraTag,
		float BlendTime = 1.f,
		TEnumAsByte<EViewTargetBlendFunction> BlendFunc = VTBlend_Linear,
		float BlendExp = 2.f,
		bool bLockOutgoing = false);

private:

	/**
	* Finds a camera point component by tag on the actor or its child actors.
	* @param OwnerWithPoints - Actor to search
	* @param CameraTag - Tag to match
	* @param OutViewTarget - Output actor to use as view target for SetViewTargetWithBlend
	* @return Camera point component if found, nullptr otherwise
	*/
	UACFCameraPointComponent* ResolveCameraPoint(AActor* OwnerWithPoints, const FGameplayTag& CameraTag, AActor*& OutViewTarget) const;

	void MoveCamera(float deltaTime);

	void SetCameraSequenceData();

	void UpdateCameraSequence(float deltaTime);

	void StoreData();
	void RecoverStoredData();

	UPROPERTY()
	TMap<FName, FCCMCameraMovementSettings> currentlyActiveCameraEvents;

	/** Preset currently underneath the camera events. Empty until Apply Camera Preset succeeds. */
	UPROPERTY(Transient)
	FGameplayTag ActiveCameraPreset;

	UPROPERTY()
	FVector originalPos;

	UPROPERTY()
	float originalFov;

	UPROPERTY()
	FRotator _originalRelativeRot;

	UPROPERTY()
	FVector _localPos;

	UPROPERTY()
	float _localFov;

	UPROPERTY()
	float _localTimeDilatation;

	UPROPERTY()
	ETargetLockType _localIsLockingAt;

	UPROPERTY()
	bool _localStrafing;

	UPROPERTY()
	bool _localOrientToMov;

	UPROPERTY()
	float _localArmLength;

	UPROPERTY()
	TObjectPtr<UACFCameraPointComponent> currentCameraComp;

	UFUNCTION()
	void HandlePawnChanged(APawn* newPawn);

	UFUNCTION()
	void OnLevelSequenceFinished();

	void FollowPlayer(float deltaTime);

	UPROPERTY()
	FCCMCameraSequenceSettings currentSequenceSettings;

	UPROPERTY()
	bool bFollowPlayerWhenMoving;

	// Battle camera managment

	UPROPERTY(Transient)
	ACameraActor* CameraStubA = nullptr;

	UPROPERTY(Transient)
	ACameraActor* CameraStubB = nullptr;

	UPROPERTY(Transient)
	ACameraActor* LastViewTarget = nullptr;

	UPROPERTY(Transient)
	bool bIsUsingA = false;



};
