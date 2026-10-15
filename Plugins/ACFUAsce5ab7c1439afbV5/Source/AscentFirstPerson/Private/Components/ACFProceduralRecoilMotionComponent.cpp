// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "Components/ACFProceduralRecoilMotionComponent.h"

#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "Engine/World.h"

#include "Presets/ACFProceduralPreset.h"
#include "Presets/ACFProceduralPresetRecoilMotion.h"

UACFProceduralRecoilMotionComponent::UACFProceduralRecoilMotionComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Stage = 5;

	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UACFProceduralRecoilMotionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const UACFProceduralPresetRecoilMotion* PresetRecoil = UACFProceduralPresetData::GetPresetOrDefault<UACFProceduralPresetRecoilMotion>(GetEffectivePresetData());
	if (!PresetRecoil) return;

	const float CurrentTime = GetWorld()->GetTimeSeconds();
	const FACFAdvancedOvershootConfig& OvershootConfig = GetCurrentOvershootConfig();

	// Process rotation axes
	const float RawPitch = ProcessAxis(RotationState.Y, OvershootConfig.RotationOvershoot, DeltaTime, CurrentTime);
	const float RawYaw = ProcessAxis(RotationState.Z, OvershootConfig.RotationOvershoot, DeltaTime, CurrentTime);
	const float RawRoll = ProcessAxis(RotationState.X, OvershootConfig.RotationOvershoot, DeltaTime, CurrentTime);

	// Process location axes
	const float RawX = ProcessAxis(LocationState.X, OvershootConfig.LocationOvershoot, DeltaTime, CurrentTime);
	const float RawY = ProcessAxis(LocationState.Y, OvershootConfig.LocationOvershoot, DeltaTime, CurrentTime);
	const float RawZ = ProcessAxis(LocationState.Z, OvershootConfig.LocationOvershoot, DeltaTime, CurrentTime);

	FRotator RawRotation(RawPitch, RawYaw, RawRoll);
	FVector RawLocation(RawX, RawY, RawZ);

	// Apply smoothing during auto fire
	const bool bInAutoFire = FireMode == EACFRecoilFireMode::Auto && ConsecutiveShots > 1;

	if (bInAutoFire)
	{
		const float SmoothSpeed = PresetRecoil->AutoFireSmoothing;
		SmoothedRotation.Pitch = FMath::FInterpTo(SmoothedRotation.Pitch, RawRotation.Pitch, DeltaTime, SmoothSpeed);
		SmoothedRotation.Yaw = FMath::FInterpTo(SmoothedRotation.Yaw, RawRotation.Yaw, DeltaTime, SmoothSpeed);
		SmoothedRotation.Roll = FMath::FInterpTo(SmoothedRotation.Roll, RawRotation.Roll, DeltaTime, SmoothSpeed);
		SmoothedLocation = FMath::VInterpTo(SmoothedLocation, RawLocation, DeltaTime, SmoothSpeed);

		OutputRotation = SmoothedRotation;
		OutputLocation = SmoothedLocation;
	}
	else
	{
		SmoothedRotation = RawRotation;
		SmoothedLocation = RawLocation;
		OutputRotation = RawRotation;
		OutputLocation = RawLocation;
	}

	// Calculate and apply continuous progression
	TargetPitchProgression = FMath::FInterpTo(TargetPitchProgression, 0.0f, DeltaTime, PresetRecoil->PitchProgress.Damping);
	CurrentPitchProgression = FMath::FInterpTo(CurrentPitchProgression, TargetPitchProgression, DeltaTime, PresetRecoil->PitchProgress.Acceleration);

	TargetZProgression = FMath::FInterpTo(TargetZProgression, 0.0f, DeltaTime, PresetRecoil->ZProgress.Damping);
	CurrentZProgression = FMath::FInterpTo(CurrentZProgression, TargetZProgression, DeltaTime, PresetRecoil->ZProgress.Acceleration);

	// Add progression directly to the smoothed outputs
	OutputRotation.Pitch += CurrentPitchProgression;
	OutputLocation.Z += CurrentZProgression;

	// Apply pivot offset
	const FVector PivotOffset = FMath::Lerp(PresetRecoil->PivotOffset, PresetRecoil->AimPivotOffset, AimingState);
	if (!PivotOffset.IsNearlyZero())
	{
		const FVector RotatedPivot = OutputRotation.RotateVector(PivotOffset) - PivotOffset;
		OutputLocation += RotatedPivot;
	}

	// Apply global multipliers
	OutputRotation.Pitch *= PresetRecoil->RotationMultiplier.Y * RecoilScale;
	OutputRotation.Yaw *= PresetRecoil->RotationMultiplier.Z * RecoilScale;
	OutputRotation.Roll *= PresetRecoil->RotationMultiplier.X * RecoilScale;
	OutputLocation *= PresetRecoil->LocationMultiplier * RecoilScale;

	// Controller recoil
	if (bEnableControllerRecoil)
	{
		TickControllerRecoil(DeltaTime);
	}

	// Disable tick when animation complete
	if (IsRecoilComplete() && ControllerRecoilCurrent.IsNearlyZero(0.01f))
	{
		SetComponentTickEnabled(false);
	}
}

void UACFProceduralRecoilMotionComponent::Init(float FireRateRPM /*= 600.0f*/)
{
	FireRate = FMath::Max(FireRateRPM, 60.0f);

	RotationState.Reset();
	LocationState.Reset();
	ConsecutiveShots = 0;
	bIsFiring = false;

	CurrentPitchProgression = 0.0f;
	TargetPitchProgression = 0.0f;
	CurrentZProgression = 0.0f;
	TargetZProgression = 0.0f;
}

void UACFProceduralRecoilMotionComponent::Fire()
{
	// Controller recoil ticks outside the output gate, so a missing required preset has to stop it here.
	if (!HasRequiredPreset()) return;

	const UACFProceduralPresetRecoilMotion* PresetRecoil = UACFProceduralPresetData::GetPresetOrDefault<UACFProceduralPresetRecoilMotion>(GetEffectivePresetData());
	if (!PresetRecoil) return;

	const float CurrentTime = GetWorld()->GetTimeSeconds();
	const float TimeBetweenShots = 60.0f / FireRate;

	// Track consecutive shots for auto-fire behavior
	if (CurrentTime - LastShotTime < TimeBetweenShots * 1.5f)
	{
		ConsecutiveShots++;
	}
	else
	{
		ConsecutiveShots = 1;
	}

	LastShotTime = CurrentTime;
	bIsFiring = true;

	GenerateRecoilTargets();
	SetComponentTickEnabled(true);
}

void UACFProceduralRecoilMotionComponent::StopFiring()
{
	bIsFiring = false;
	GetWorld()->GetTimerManager().ClearTimer(SimulationTimerHandle);
}

void UACFProceduralRecoilMotionComponent::SetFireMode(EACFRecoilFireMode NewMode)
{
	FireMode = NewMode;
}

void UACFProceduralRecoilMotionComponent::StartSimulation()
{
	GetWorld()->GetTimerManager().SetTimer(SimulationTimerHandle, this, &UACFProceduralRecoilMotionComponent::Fire, 60/FireRate, true);
}

void UACFProceduralRecoilMotionComponent::StopSimulation()
{
	GetWorld()->GetTimerManager().ClearTimer(SimulationTimerHandle);
}

FTransform UACFProceduralRecoilMotionComponent::GetRecoilTransform() const
{
	return FTransform(OutputRotation, OutputLocation);
}

float UACFProceduralRecoilMotionComponent::GetTimeSinceLastShot() const
{
	if (!GetWorld())
	{
		return 0.0f;
	}

	return GetWorld()->GetTimeSeconds() - LastShotTime;
}

void UACFProceduralRecoilMotionComponent::ComputeOutputs(float DeltaSeconds, const UAnimInstance* AnimInstance, const FACFProceduralMotionInput& Input, TArray<FACFProceduralMotionOutput>& Outs)
{
	AimingState = Input.AimingState;

	FTransform ComponentRecoil = GetRecoilTransform();

	FVector Target = ADSOffset.GetLocation();
	Target.X = 0.f; Target.Y = 0.f;
	const FVector Delta = (ComponentRecoil.Rotator().RotateVector(Target) - Target);
	ComponentRecoil.AddToTranslation(Delta);

	FTransform FinalDelta = ComponentRecoil;
	{
		FVector T = FinalDelta.GetLocation(); Swap(T.X, T.Y); FinalDelta.SetLocation(T);
		FRotator R = FinalDelta.Rotator();  Swap(R.Roll, R.Pitch); FinalDelta.SetRotation(R.Quaternion());
	}

	const UACFProceduralPresetRecoilMotion* PresetRecoil = UACFProceduralPresetData::GetPresetOrDefault<UACFProceduralPresetRecoilMotion>(GetEffectivePresetData());
	if (!PresetRecoil)
	{
		return;
	}

	FACFProceduralMotionOutput Out;
	Out.Stage = Stage;
	Out.Composition = EACFProceduralMotionComposition::Additive;
	Out.Space = EACFProceduralMotionOutputSpace::Component;
	Out.Transform = FinalDelta;
	Out.Weight = FMath::Lerp(PresetRecoil->RecoilScale, PresetRecoil->AimingRecoilScale, AimingState);
	Outs.Add(Out);
}

float UACFProceduralRecoilMotionComponent::ProcessAxis(FACFRecoilAxisState& State, const FACFOvershootParams& Params, float DeltaTime, float CurrentTime) const
{
	if (!State.bActive)
	{
		State.Current = FMath::FInterpTo(State.Current, 0.0f, DeltaTime, Params.Decay * 2.0f);
		if (FMath::IsNearlyZero(State.Current, 0.001f))
		{
			State.Current = 0.0f;
		}
		return State.Current;
	}

	const UACFProceduralPresetRecoilMotion* PresetRecoil = UACFProceduralPresetData::GetPresetOrDefault<UACFProceduralPresetRecoilMotion>(GetEffectivePresetData());
	const float Duration = PresetRecoil ? PresetRecoil->RecoilDuration : 0.2f;
	const float ElapsedTime = CurrentTime - State.StartTime;

	const float Omega = Params.Frequency * PI * 2.0f;

	// Attack envelope: quickly rises from 0 to 1
	const float AttackEnvelope = 1.0f - FMath::Exp(-Params.AttackSpeed * ElapsedTime);

	// Decay envelope: exponentially falls to 0
	const float DecayEnvelope = FMath::Exp(-Params.Decay * ElapsedTime);

	// Oscillation: cos starts at 1 (immediate kick), then oscillates through 0, -1, 0, 1...
	const float Oscillation = FMath::Cos(Omega * ElapsedTime);

	// Combined: rises quickly, then decays with overshoot back to zero
	State.Current = State.Target * AttackEnvelope * DecayEnvelope * Oscillation;

	// Check completion
	if (ElapsedTime > Duration * 4.0f && FMath::Abs(State.Current) < 0.01f)
	{
		State.bActive = false;
		State.Current = 0.0f;
	}

	return State.Current;
}

void UACFProceduralRecoilMotionComponent::GenerateRecoilTargets()
{
	const float CurrentTime = GetWorld()->GetTimeSeconds();
	const FACFRotationRecoilConfig& RotConfig = GetRotationConfig();
	const FACFLocationRecoilConfig& LocConfig = GetLocationConfig();
	const FACFAdvancedOvershootConfig& OvershootConfig = GetCurrentOvershootConfig();

	const UACFProceduralPresetRecoilMotion* PresetRecoil = UACFProceduralPresetData::GetPresetOrDefault<UACFProceduralPresetRecoilMotion>(GetEffectivePresetData());
	if (!PresetRecoil) return;

	// Apply auto-fire multiplier for consecutive shots
	float AutoMultiplier = 1.0f;
	if (ConsecutiveShots > 1 && FireMode == EACFRecoilFireMode::Auto)
	{
		AutoMultiplier = PresetRecoil->AutoFireMultiplier;
	}

	// Generate rotation targets
	auto SetupRotationAxis = [&](FACFRecoilAxisState& State, const FVector2D& Range, float Multiplier = 1.0f)
	{
		State.Target = FMath::FRandRange(Range.X, Range.Y) * Multiplier * AutoMultiplier;
		State.StartTime = CurrentTime;
		State.bActive = true;
	};

	// Pitch (Y axis in UE)
	SetupRotationAxis(RotationState.Y, RotConfig.PitchKick);

	// Yaw (Z axis)
	SetupRotationAxis(RotationState.Z, RotConfig.YawSpread);

	// Roll (X axis) - optionally alternate direction
	FVector2D RollRange = RotConfig.RollTilt;
	if (PresetRecoil->bAlternateRoll)
	{
		if (bLastRollPositive)
		{
			RollRange = FVector2D(RotConfig.RollTilt.X, 0.0f);
		}
		else
		{
			RollRange = FVector2D(0.0f, RotConfig.RollTilt.Y);
		}
		bLastRollPositive = !bLastRollPositive;
	}
	SetupRotationAxis(RotationState.X, RollRange);

	// Generate location targets
	auto SetupLocationAxis = [&](FACFRecoilAxisState& State, const FVector2D& Range, float Multiplier = 1.0f)
	{
		const float NewTarget = FMath::FRandRange(Range.X, Range.Y) * Multiplier * AutoMultiplier;

		State.Velocity = (NewTarget - State.Target) * OvershootConfig.LocationOvershoot.AttackSpeed;
		State.Target = NewTarget;
		State.StartTime = CurrentTime;
		State.bActive = true;
	};

	// Kickback (X axis - negative)
	SetupLocationAxis(LocationState.X, LocConfig.KickBack);

	// Horizontal shift (Y axis)
	SetupLocationAxis(LocationState.Y, LocConfig.HorizontalShift);

	// Lift up (Z axis)
	SetupLocationAxis(LocationState.Z, LocConfig.LiftUp);

	// Progression Accumulation
	const float ProgressionAimMultiplier = FMath::Lerp(1.0f, PresetRecoil->ProgressAimAlpha, AimingState);

	TargetPitchProgression += PresetRecoil->PitchProgress.Amount * ProgressionAimMultiplier;
	TargetZProgression += PresetRecoil->ZProgress.Amount * ProgressionAimMultiplier;

	// Setup controller recoil target
	if (bEnableControllerRecoil && !GetWorld()->GetTimerManager().IsTimerActive(SimulationTimerHandle))
	{
		const FACFControllerRecoilConfig& CtrlConfig = PresetRecoil->ControllerRecoil;
		ControllerRecoilTarget.X += FMath::FRandRange(CtrlConfig.HorizontalKick.X, CtrlConfig.HorizontalKick.Y);
		ControllerRecoilTarget.Y += FMath::FRandRange(CtrlConfig.VerticalKick.X, CtrlConfig.VerticalKick.Y);
	}
}

void UACFProceduralRecoilMotionComponent::TickControllerRecoil(float DeltaTime)
{
	const UACFProceduralPresetRecoilMotion* PresetRecoil = UACFProceduralPresetData::GetPresetOrDefault<UACFProceduralPresetRecoilMotion>(GetEffectivePresetData());
	if (!PresetRecoil) return;

	if (!IsValid(AnimOwnerCharacter))
	{
		return;
	}

	const FACFControllerRecoilConfig& Config = PresetRecoil->ControllerRecoil;
	const FVector2D PreviousRecoil = ControllerRecoilCurrent;

	// Interpolate current to target
	ControllerRecoilCurrent.X = FMath::FInterpTo(ControllerRecoilCurrent.X, ControllerRecoilTarget.X, DeltaTime, Config.RecoilSpeed);
	ControllerRecoilCurrent.Y = FMath::FInterpTo(ControllerRecoilCurrent.Y, ControllerRecoilTarget.Y, DeltaTime, Config.RecoilSpeed);

	// Decay target when not firing
	if (!bIsFiring)
	{
		// Disabled
		//ControllerRecoilTarget = FMath::Vector2DInterpTo(ControllerRecoilTarget, FVector2D::ZeroVector, DeltaTime, Config.RecoverySpeed);
	}

	// Apply delta to controller
	const FVector2D RecoilDelta = ControllerRecoilCurrent - PreviousRecoil;
	AnimOwnerCharacter->AddControllerPitchInput(-RecoilDelta.Y);
	AnimOwnerCharacter->AddControllerYawInput(RecoilDelta.X);
}

bool UACFProceduralRecoilMotionComponent::IsRecoilComplete() const
{
	const bool bRotationDone = !RotationState.X.bActive && !RotationState.Y.bActive && !RotationState.Z.bActive;
	const bool bLocationDone = !LocationState.X.bActive && !LocationState.Y.bActive && !LocationState.Z.bActive;
	const bool bOutputNearZero = OutputRotation.IsNearlyZero(0.01f) && OutputLocation.IsNearlyZero(0.01f);

	const bool bProgressionDone = FMath::IsNearlyZero(CurrentPitchProgression, 0.01f) && FMath::IsNearlyZero(CurrentZProgression, 0.01f);

	return bRotationDone && bLocationDone && bOutputNearZero && bProgressionDone;
}

const FACFAdvancedOvershootConfig& UACFProceduralRecoilMotionComponent::GetCurrentOvershootConfig() const
{
	const UACFProceduralPresetRecoilMotion* PresetRecoil = UACFProceduralPresetData::GetPresetOrDefault<UACFProceduralPresetRecoilMotion>(GetEffectivePresetData());

	if (!PresetRecoil)
	{
		static FACFAdvancedOvershootConfig DefaultConfig;
		return DefaultConfig;
	}

	// Use auto config for sustained fire
	if (ConsecutiveShots > 1 && FireMode == EACFRecoilFireMode::Auto)
	{
		return PresetRecoil->AutoOvershoot;
	}

	return PresetRecoil->SemiOvershoot;
}

const FACFRotationRecoilConfig& UACFProceduralRecoilMotionComponent::GetRotationConfig() const
{
	const UACFProceduralPresetRecoilMotion* PresetRecoil = UACFProceduralPresetData::GetPresetOrDefault<UACFProceduralPresetRecoilMotion>(GetEffectivePresetData());

	if (!PresetRecoil)
	{
		static FACFRotationRecoilConfig DefaultConfig;
		return DefaultConfig;
	}

	return AimingState >= 0.5f ? PresetRecoil->AimRotation : PresetRecoil->HipRotation;
}

const FACFLocationRecoilConfig& UACFProceduralRecoilMotionComponent::GetLocationConfig() const
{
	const UACFProceduralPresetRecoilMotion* PresetRecoil = UACFProceduralPresetData::GetPresetOrDefault<UACFProceduralPresetRecoilMotion>(GetEffectivePresetData());

	if (!PresetRecoil)
	{
		static FACFLocationRecoilConfig DefaultConfig;
		return DefaultConfig;
	}

	return AimingState >= 0.5f ? PresetRecoil->AimLocation : PresetRecoil->HipLocation;
}

TSubclassOf<UACFProceduralPreset> UACFProceduralRecoilMotionComponent::GetPresetClass() const
{
	return UACFProceduralPresetRecoilMotion::StaticClass();
}
