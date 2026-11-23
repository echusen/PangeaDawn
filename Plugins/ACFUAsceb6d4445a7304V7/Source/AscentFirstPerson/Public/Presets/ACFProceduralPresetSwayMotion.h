// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Presets/ACFProceduralPreset.h"
#include "Engine/DataAsset.h"
#include "Kismet/KismetMathLibrary.h"
#include "ACFProceduralPresetSwayMotion.generated.h"

USTRUCT(BlueprintType)
struct FACFVectorSpringSettings
{
	GENERATED_BODY()

public:
	/** How tightly each axis is pulled back to its target. Higher snaps back faster (a light item), lower is looser (a heavy item). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category=Defaults)
	FVector SpringStiffness{FVector::OneVector};

	/** Damping of each axis's spring, relative to critical damping. 1 settles without overshooting, below 1 wobbles past the target before settling. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category=Defaults)
	FVector SpringDamping{FVector::OneVector};

	/**
	 * How fast each axis's target follows the input before the spring acts on it (an interpolation speed). Higher reacts
	 * sooner, lower smooths the input and delays the sway. 0 follows the input instantly.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category=Defaults)
	FVector SmoothDamping{FVector::OneVector};

	/** Time scale of each axis's spring: it multiplies the frame time, so higher plays the whole spring motion faster. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category=Defaults)
	FVector SmoothSpeed{FVector::OneVector};

	/**
	 * Strength per axis: the input is multiplied by it, then limited by Threshold.

	 * ROTATION: X = Roll (tilt), Y = Pitch (up and down), Z = Yaw (turn).
	   Aim sway: turning drives Roll and Yaw, looking up and down drives Pitch.
	   Move sway: strafing drives Roll and Yaw, moving forward and back drives Pitch.
	 * TRANSLATION (Aim sway): Y = sideways, from turning. Z = up and down, from looking up and down. X is not used.
	 * TRANSLATION (Move sway): X = forward and back, from moving forward and back. Y = sideways, from strafing.
	   Z = up and down, from moving forward and back.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category=Defaults)
	FVector Scale{FVector::OneVector};

	/** Largest sway per axis, in degrees or centimetres. The input times Scale is clamped to it, so an axis at 0 does not sway at all. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category=Defaults)
	FVector Threshold{FVector::ZeroVector};
};

USTRUCT(BlueprintType)
struct FACFSpringSwaySettings
{
	GENERATED_BODY()

	/** Positional sway: the item moves sideways, up and down, forward and back. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Defaults)
	FACFVectorSpringSettings TranslationSettings;

	/** Rotational sway: the item tilts and turns. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Defaults)
	FACFVectorSpringSettings RotationSettings;

	/** Not used. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Defaults)
	FTransform SpaceOffset{FTransform::Identity};
};

USTRUCT(BlueprintType)
struct FACFSwayData
{
	GENERATED_BODY()

	/** Reacts to looking around: the change of the view rotation each frame. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Aiming")
	FACFSpringSwaySettings AimSwaySettings;

	/** Reacts to movement: the movement input (velocity for other players' characters), forward and sideways. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement")
	FACFSpringSwaySettings MoveSwaySettings;

	/** Strength of both sways when fully aimed, blended in linearly with the aiming alpha. 1 keeps the hip sway, 0 removes it. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category=Defaults)
	float AimScale{0.9f};

	/** Point the item rotates around, relative to the weapon holder. Move it to the stock or the handle to make that the anchor. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category=Defaults)
	FVector PivotOffset{FVector::ZeroVector};
};

struct FACFSwaySpringState
{
	FFloatSpringState X;
	FFloatSpringState Y;
	FFloatSpringState Z;

	FFloatSpringState Pitch;
	FFloatSpringState Yaw;
	FFloatSpringState Roll;

	void Reset()
	{
		X.Reset();
		Y.Reset();
		Z.Reset();

		Pitch.Reset();
		Yaw.Reset();
		Roll.Reset();
	}
};

/** Quick Reference:
- Mouse Input: Maps to Aim Sway.
	* Mouse X (Left/Right): Affects Roll and Yaw.
	* Mouse Y (Up/Down): Affects Pitch.

- Movement Input: Maps to Move Sway.
	* Move X (Strafe): Affects Roll and Yaw.
	* Move Y (Forward/Back): Affects Pitch.
*/
UCLASS(Blueprintable, BlueprintType)
class ASCENTFIRSTPERSON_API UACFSwayPreset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Defaults)
	FACFSwayData SwayData;

};

UCLASS(meta=(DisplayName="Preset: Sway Motion"))
class ASCENTFIRSTPERSON_API UACFProceduralPresetSwayMotion : public UACFProceduralPreset
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category="Sway")
	const FACFSwayData& GetSwayData() const { return SwayPreset ? SwayPreset->SwayData : SwayData; }

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sway")
	TObjectPtr<UACFSwayPreset> SwayPreset;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sway", meta=(EditCondition="HasNoSwayPreset()", EditConditionHides))
	FACFSwayData SwayData;

	/** Reads the look and movement input from the pawn being animated. Off, nothing feeds the sway, so it stays still. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sway|Input")
	bool bAutoReadFromOwner = true;

	// Strengthens the sway response to horizontal mouse movement.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sway|Input", meta=(EditCondition="bAutoReadFromOwner"))
	float MouseToInputScaleYaw = 1.0f;
	
	// Strengthens the sway response to vertical mouse movement.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sway|Input", meta=(EditCondition="bAutoReadFromOwner"))
	float MouseToInputScalePitch = 1.0f;
	
	// Master strength for WASD movement sway.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sway|Input", meta=(EditCondition="bAutoReadFromOwner"))
	float MoveVectorScale = 1.0f;

private:
	UFUNCTION()
	bool HasNoSwayPreset() const { return !::IsValid(SwayPreset); }
	
};
