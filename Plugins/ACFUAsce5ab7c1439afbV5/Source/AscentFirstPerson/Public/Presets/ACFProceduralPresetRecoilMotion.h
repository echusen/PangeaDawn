// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ACFProceduralPreset.h"
#include "ACFProceduralPresetRecoilMotion.generated.h"

UENUM(BlueprintType)
enum class EACFRecoilFireMode : uint8
{
	Semi,
	Auto
};

// Overshoot curve parameters - controls the damped oscillation behavior
USTRUCT(BlueprintType)
struct FACFOvershootParams
{
	GENERATED_BODY()

	// Oscillation frequency - higher values = faster oscillation
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Overshoot", meta = (ClampMin = "0.5", ClampMax = "15.0"))
	float Frequency = 4.0f;

	// Decay rate - higher values = faster settle to rest
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Overshoot", meta = (ClampMin = "1.0", ClampMax = "20.0"))
	float Decay = 6.0f;

	// Attack speed - how fast to reach peak recoil (0 = instant)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Overshoot", meta = (ClampMin = "0.0", ClampMax = "50.0"))
	float AttackSpeed = 25.0f;
};

// Simple rotation recoil configuration
USTRUCT(BlueprintType)
struct FACFRotationRecoilConfig
{
	GENERATED_BODY()

	// Upward pitch kick (positive = up in UE convention for weapon space)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotation")
	FVector2D PitchKick = FVector2D(-2.0f, -1.0f);

	// Horizontal yaw spread (random between min/max)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotation")
	FVector2D YawSpread = FVector2D(-0.5f, 0.5f);

	// Roll tilt range
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotation")
	FVector2D RollTilt = FVector2D(-1.5f, 1.5f);
};

// Simple location recoil configuration
USTRUCT(BlueprintType)
struct FACFLocationRecoilConfig
{
	GENERATED_BODY()

	// Backward kick (negative X)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Location")
	FVector2D KickBack = FVector2D(-8.0f, -5.0f);

	// Upward lift (positive Z)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Location")
	FVector2D LiftUp = FVector2D(0.5f, 1.5f);

	// Horizontal shift (Y axis)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Location")
	FVector2D HorizontalShift = FVector2D(-0.3f, 0.3f);
};

// Controller/camera recoil that affects player aim
USTRUCT(BlueprintType)
struct FACFControllerRecoilConfig
{
	GENERATED_BODY()

	// Vertical recoil per shot (pitch)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Controller")
	FVector2D VerticalKick = FVector2D(0.3f, 0.5f);

	// Horizontal recoil per shot (yaw)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Controller")
	FVector2D HorizontalKick = FVector2D(-0.15f, 0.15f);

	// Interpolation speed to target recoil
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Controller", meta = (ClampMin = "1.0", ClampMax = "50.0"))
	float RecoilSpeed = 15.0f;

	// Return to center speed when not firing
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Controller", meta = (ClampMin = "1.0", ClampMax = "20.0"))
	float RecoverySpeed = 5.0f;
};

// Advanced overshoot settings per fire mode
USTRUCT(BlueprintType)
struct FACFAdvancedOvershootConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Advanced")
	FACFOvershootParams RotationOvershoot;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Advanced")
	FACFOvershootParams LocationOvershoot;
};

// For Z and Pitch progress
USTRUCT(BlueprintType)
struct FACFProgressConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Progress")
	float Acceleration = 0.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Progress")
	float Damping = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Progress")
	float Amount = 0.0f;
};

UCLASS(meta=(DisplayName="Preset: Recoil Motion"))
class ASCENTFIRSTPERSON_API UACFProceduralPresetRecoilMotion : public UACFProceduralPreset
{
	GENERATED_BODY()

public:
	UACFProceduralPresetRecoilMotion();

	// Hip fire rotation recoil
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simple | Hip Fire")
	FACFRotationRecoilConfig HipRotation;

	// Hip fire location recoil
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simple | Hip Fire")
	FACFLocationRecoilConfig HipLocation;

	// ADS rotation recoil (typically reduced)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simple | Aiming")
	FACFRotationRecoilConfig AimRotation;

	// ADS location recoil (typically reduced)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simple | Aiming")
	FACFLocationRecoilConfig AimLocation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simple | Progress")
	FACFProgressConfig PitchProgress;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simple | Progress")
	FACFProgressConfig ZProgress;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simple | Progress")
	float ProgressAimAlpha = 0.f;

	// Controller/camera recoil
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simple | Controller")
	FACFControllerRecoilConfig ControllerRecoil;

	// Duration of the recoil animation
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simple | Timing", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float RecoilDuration = 0.2f;

	// Rounds per minute, so recoil can tell a burst from separate shots. The first person component hands it to the
	// recoil components whenever the held item changes; code driving a recoil component directly calls Init instead.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simple | Timing", meta = (ClampMin = "60.0"))
	float FireRateRPM = 600.f;

	// Handed over by the first person component along with FireRateRPM; SetFireMode otherwise.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simple | Timing")
	EACFRecoilFireMode FireMode = EACFRecoilFireMode::Semi;

	// Overshoot parameters for single/semi-auto fire
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Advanced | Semi")
	FACFAdvancedOvershootConfig SemiOvershoot;

	// Overshoot parameters for full-auto fire
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Advanced | Auto")
	FACFAdvancedOvershootConfig AutoOvershoot;

	// Multiplier applied during full-auto (stacks per shot)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Advanced | Auto", meta = (ClampMin = "0.5", ClampMax = "1.5"))
	float AutoFireMultiplier = 0.85f;

	// Smoothing applied during auto fire (higher = smoother transitions)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Advanced | Auto", meta = (ClampMin = "5.0", ClampMax = "30.0"))
	float AutoFireSmoothing = 12.0f;

	// Alternates roll direction each shot for more natural feel
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Advanced | Feel")
	bool bAlternateRoll = true;

	// Pivot point offset for rotation (creates different visual effect)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Advanced | Pivot")
	FVector PivotOffset = FVector::ZeroVector;

	// Pivot offset when aiming
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Advanced | Pivot")
	FVector AimPivotOffset = FVector::ZeroVector;

	// Global multiplier for all rotation values
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Advanced | Multipliers")
	FVector RotationMultiplier = FVector::OneVector;

	// Global multiplier for all location values
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Advanced | Multipliers")
	FVector LocationMultiplier = FVector::OneVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Advanced | Multipliers")
	float RecoilScale = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Advanced | Multipliers")
	float AimingRecoilScale = 0.5f;
};
