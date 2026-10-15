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
	// Controls how "tight" the sway is. High values = snaps back quickly (light object). Low values = loose/floppy (heavy object).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category=Defaults)
	FVector SpringStiffness{FVector::OneVector};

	// Controls vibration/oscillation. High values = stops moving quickly (no wobble). Low values = keeps wobbling like a pendulum.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category=Defaults)
	FVector SpringDamping{FVector::OneVector};

	// How much "lag" the sway has behind the camera. Higher = smoother but more delayed response.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category=Defaults)
	FVector SmoothDamping{FVector::OneVector};

	// The speed of the spring interpolation. Multiplier for the simulation step.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category=Defaults)
	FVector SmoothSpeed{FVector::OneVector};
	
	/**
	Strength multiplier.

	* For ROTATION settings:
		X = Roll strength (Tilt head left/right)
		Y = Pitch strength (Look up/down)
		Z = Yaw strength (Turn left/right)
	* For TRANSLATION settings (Aim):
		Y = Horizontal sway distance
		Z = Vertical sway distance
	* For TRANSLATION settings (Move):
		X = Horizontal sway (Strafe)
		Y = Forward/Back sway
		Z = Vertical Bob (Up/Down)
	*/
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category=Defaults)
	FVector Scale{FVector::OneVector};

	// Maximum limit for the sway. The motion will clamp to these values to prevent clipping or extreme rotations.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category=Defaults)
	FVector Threshold{FVector::ZeroVector};
};

USTRUCT(BlueprintType)
struct FACFSpringSwaySettings
{
	GENERATED_BODY()

	// Positional lag (Weapon physically moves left/right/up/down).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Defaults)
	FACFVectorSpringSettings TranslationSettings;

	// Rotational lag (Weapon tilts and turns).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Defaults)
	FACFVectorSpringSettings RotationSettings;

	// Base offset applied to the weapon when this preset is active.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Defaults)
	FTransform SpaceOffset{FTransform::Identity};
};

USTRUCT(BlueprintType)
struct FACFSwayData
{
	GENERATED_BODY()

	// Reacts to MOUSE look input.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Aiming")
	FACFSpringSwaySettings AimSwaySettings;
	
	// Reacts to KEYBOARD movement input (WASD).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement")
	FACFSpringSwaySettings MoveSwaySettings;

	// Multiplier applied when Aiming Down Sights (ADS). Scales down exponentially, so set to around 0.9 to stabilize sway while aiming.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category=Defaults)
	float AimScale{0.9f};

	// Point around which the weapon rotates. Adjust this to make the stock or handle feel like the anchor point.
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
