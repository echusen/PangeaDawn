// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "Presets/ACFProceduralPresetRecoilMotion.h"

UACFProceduralPresetRecoilMotion::UACFProceduralPresetRecoilMotion()
{
	PitchProgress.Acceleration = 8.0f;
	PitchProgress.Damping = 12.0f;
	// PitchProgress.Amount = -8.0f;

	ZProgress.Acceleration = 5.0f;
	ZProgress.Damping = 12.0f;
	// ZProgress.Amount = 4.0f;
	
	SemiOvershoot.RotationOvershoot.Frequency = 3.0f;
	SemiOvershoot.RotationOvershoot.Decay = 8.0f;
	SemiOvershoot.RotationOvershoot.AttackSpeed = 10.0f;

	SemiOvershoot.LocationOvershoot.Frequency = 3.0f;
	SemiOvershoot.LocationOvershoot.Decay = 8.0f;
	SemiOvershoot.LocationOvershoot.AttackSpeed = 10.0f;

	AutoOvershoot.RotationOvershoot.Frequency = 4.0f;
	AutoOvershoot.RotationOvershoot.Decay = 10.0f;
	AutoOvershoot.RotationOvershoot.AttackSpeed = 40.0f;

	AutoOvershoot.LocationOvershoot.Frequency = 4.0f;
	AutoOvershoot.LocationOvershoot.Decay = 10.0f;
	AutoOvershoot.LocationOvershoot.AttackSpeed = 40.0f;

	AimRotation.PitchKick = FVector2D(-2.0f, -1.5f);
	AimRotation.YawSpread = FVector2D(-0.3f, 0.3f);
	AimRotation.RollTilt = FVector2D(-1.0f, 1.0f);
	AimLocation.KickBack = FVector2D(-5.0f, -3.0f);
	AimLocation.LiftUp = FVector2D(0.3f, 0.8f);
	AimLocation.HorizontalShift = FVector2D(-0.2f, 0.2f);
}
