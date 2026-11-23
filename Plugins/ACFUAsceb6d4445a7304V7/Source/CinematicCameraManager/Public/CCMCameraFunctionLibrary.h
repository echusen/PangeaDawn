// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved. 
/*
2018 PakyMan Prod. 
*/
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include <Engine/DataTable.h>
#include "CCMTypes.h"
#include "CCMCameraFunctionLibrary.generated.h"

class ULevelSequence;
class ALevelSequenceActor;
/**
 * 
 */
UCLASS()
class CINEMATICCAMERAMANAGER_API UCCMCameraFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public: 

	UFUNCTION(BlueprintCallable, BlueprintPure, meta = (WorldContext = "WorldContextObject"), Category = CCM)
	static class ACCMPlayerCameraManager* GetLocalCinematicCameraManager(const UObject* WorldContextObject);
	
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"), Category = CCM)
	static	void TriggerCameraEvent(const UObject* WorldContextObject, FName CameraEventName);

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"), Category = CCM)
	static	void TriggerTimedCameraEvent(const UObject* WorldContextObject, FName CameraEventName, float duration);

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"), Category = CCM)
	static void StopCameraEvent(const UObject* WorldContextObject, FName CameraEventName);

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"), Category = CCM)
	static void LockCameraOnActor(const UObject* WorldContextObject, AActor* ActorLookAt, ELockType locktype, float lockStrength = 5.f);

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"), Category = CCM)
	static void StopLockingCameraOnActor(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"), Category = CCM)
	static void LockCameraOnComponent(const UObject* WorldContextObject, USceneComponent* ComponentLookAt, ELockType locktype, float lockStrength);
	
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"), Category = CCM)
	static void ResetCameraPosition(const UObject* WorldContextObject, bool bInstantReset = false);

	/**
	 * Applies the camera preset registered under PresetTag on the local cinematic camera manager.
	 * Camera events already running stay as deltas on top of that preset.
	 * @return False when no cinematic camera manager is available, or the tag is not registered.
	 */
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject", DisplayName = "Apply Camera Preset", Keywords = "camera preset gameplay tag spring arm fov"), Category = CCM)
	static bool ApplyCameraPreset(const UObject* WorldContextObject, FGameplayTag PresetTag);

	/**
	 * Frames the player together with ActorToFrame instead of locking on it: nothing gets centered,
	 * the camera keeps them both on screen from whichever side each is on and slides sideways to give
	 * them a side of the frame each. Starting it gives up any camera lock.
	 * @param ActorToFrame The other actor to keep in frame, usually the enemy being fought.
	 * @param Settings Screen fraction, arm cap, sideways room and interpolation speed.
	 * @param FramingStrength How hard the camera works at it, same scale as the lock strength.
	 */
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject", AutoCreateRefTerm = "Settings", DisplayName = "Start Framing Actors"), Category = CCM)
	static void StartFramingActors(const UObject* WorldContextObject, AActor* ActorToFrame, const FCCMActorFramingSettings& Settings, float FramingStrength = 15.f);

	/** Stops the framing mode and restores the arm length and sideways shift it started from. */
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject", DisplayName = "Stop Framing Actors"), Category = CCM)
	static void StopFramingActors(const UObject* WorldContextObject);

	/**
	 * Where a world point sits on screen for a given point of view, as angles from the center.
	 * @param ViewLocation Where the camera is.
	 * @param ViewRotation Where the camera is looking.
	 * @param WorldPoint The point to measure.
	 * @return X is the yaw and Y the pitch away from the center of the screen, in degrees, signed.
	 */
	UFUNCTION(BlueprintPure, Category = CCM, meta = (DisplayName = "Get View Relative Angles", Keywords = "framing angle yaw pitch screen center"))
	static FVector2D GetViewRelativeAngles(FVector ViewLocation, FRotator ViewRotation, FVector WorldPoint);

	/**
	 * How far a view has to turn along one axis for both of two on-screen angles to sit inside
	 * +/- Bound. Zero while they already fit, and blind to which of the two is on which side, so a
	 * camera driven by it never picks a favourite side.
	 * @param FirstAngle Angle from the center of the screen of the first actor, in degrees.
	 * @param SecondAngle Angle from the center of the screen of the second actor, in degrees.
	 * @param Bound Half of the screen they are allowed to use, as an angle in degrees.
	 * @return Degrees to add to the view along that axis, 0 when nothing is out.
	 */
	UFUNCTION(BlueprintPure, Category = CCM, meta = (DisplayName = "Get Framing Correction", Keywords = "framing screen edge on screen composition"))
	static float GetFramingCorrection(float FirstAngle, float SecondAngle, float Bound);
	
	/** 
	 * Play a level sequence with optional actor bindings.
	 * @param WorldContextObject - World context for getting the camera manager
	 * @param Sequence - The level sequence to play
	 * @param TransformOrigin - Transform used as the origin point for the sequence (starting point)
	 * @param Bindings - Array of binding structs containing tag names and optional actors to bind
	 * @return The created LevelSequenceActor, or nullptr if creation failed
	 */
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject", AdvancedDisplay = "TransformOrigin,Bindings", AutoCreateRefTerm = "TransformOrigin,Bindings"), Category = CCM)
	static ALevelSequenceActor* PlayGameplaySequenceWithBinding(
		UObject* WorldContextObject,
		ULevelSequence* Sequence,
		const FTransform& TransformOrigin,
		const TArray<FCCMSequenceBinding>& Bindings);

	/** Stops the level sequence started with PlayGameplaySequenceWithBinding. No-op if none is playing. */
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"), Category = CCM)
	static void StopSequence(const UObject* WorldContextObject);
};
