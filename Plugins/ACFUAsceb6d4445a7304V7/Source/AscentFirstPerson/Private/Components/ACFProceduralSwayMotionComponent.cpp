// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "Components/ACFProceduralSwayMotionComponent.h"

#include "Animation/AnimInstance.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PawnMovementComponent.h"

#include "Presets/ACFProceduralPreset.h"
#include "Utility/ACFFirstPersonFunctionLibrary.h"

namespace
{
	/** Remote view pitch is byte-quantized and arrives at a low rate, so it needs smoothing before differencing. */
	constexpr float RemoteViewInterpSpeed = 15.0f;
}

UACFProceduralSwayMotionComponent::UACFProceduralSwayMotionComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Stage = 4;
}

const UACFProceduralPresetSwayMotion* UACFProceduralSwayMotionComponent::GetSwayPreset() const
{
	return UACFProceduralPresetData::GetPresetOrDefault<UACFProceduralPresetSwayMotion>(GetEffectivePresetData());
}

void UACFProceduralSwayMotionComponent::ComputeOutputs(float DeltaSeconds, const UAnimInstance* AnimInstance, const FACFProceduralMotionInput& Input, TArray<FACFProceduralMotionOutput>& Outs)
{
	const UACFProceduralPresetSwayMotion* Preset = GetSwayPreset();
	if (!Preset)
	{
		return;
	}

	const FACFSwayData& SwayData = Preset->GetSwayData();

	AimingState = Input.AimingState;

	AcquireInputsFromOwner(DeltaSeconds, AnimInstance, Preset);
	RefreshSwayTargets(DeltaSeconds, SwayData);
	RefreshAimSway(DeltaSeconds, SwayData);
	RefreshMoveSway(DeltaSeconds, SwayData);

	// Aim sway is authored in weapon space, so the axes are remapped on the way out.
	const FVector AimLocation = AimSwayOutput.GetLocation();
	FVector TargetTranslation(AimLocation.Y, 0.f, AimLocation.Z);

	FRotator TargetRotation = AimSwayOutput.Rotator();
	Swap(TargetRotation.Roll, TargetRotation.Pitch);

	FTransform Result(TargetRotation, TargetTranslation);

	{
		FVector MoveTranslation = MoveSwayOutput.GetLocation();
		Swap(MoveTranslation.X, MoveTranslation.Y);

		FRotator MoveRotation = MoveSwayOutput.Rotator();
		Swap(MoveRotation.Roll, MoveRotation.Pitch);

		Result.AddToTranslation(MoveTranslation);
		Result.SetRotation(Result.GetRotation() * MoveRotation.Quaternion());
	}

	// Rotate around the configured pivot instead of the bone origin.
	const FVector PivotOffset = SwayData.PivotOffset;
	Result.AddToTranslation(Result.GetRotation().RotateVector(PivotOffset) - PivotOffset);

	FACFProceduralMotionOutput Out;
	Out.Stage = Stage;
	Out.Composition = EACFProceduralMotionComposition::Additive;
	Out.Space = EACFProceduralMotionOutputSpace::Component;
	Out.Transform = Result;
	Out.Weight = 1.0f;
	Outs.Add(Out);
}

void UACFProceduralSwayMotionComponent::RefreshSwayTargets(float DeltaSeconds, const FACFSwayData& SwayData)
{
	// --- Aim target ---
	{
		const FACFVectorSpringSettings& RotSettings = SwayData.AimSwaySettings.RotationSettings;
		const FRotator Current = AimSwayTarget.Rotator();

		const float Roll = FMath::FInterpTo(Current.Roll, FMath::Clamp(RotSettings.Scale.X * MouseDeltaInput.X, -RotSettings.Threshold.X, RotSettings.Threshold.X), DeltaSeconds, RotSettings.SmoothDamping.X);
		const float Pitch = FMath::FInterpTo(Current.Pitch, FMath::Clamp(RotSettings.Scale.Y * MouseDeltaInput.Y, -RotSettings.Threshold.Y, RotSettings.Threshold.Y), DeltaSeconds, RotSettings.SmoothDamping.Y);
		const float Yaw = FMath::FInterpTo(Current.Yaw, FMath::Clamp(RotSettings.Scale.Z * MouseDeltaInput.X, -RotSettings.Threshold.Z, RotSettings.Threshold.Z), DeltaSeconds, RotSettings.SmoothDamping.Z);

		AimSwayTarget.SetRotation(FRotator(Pitch, Yaw, Roll).Quaternion());

		const FACFVectorSpringSettings& LocSettings = SwayData.AimSwaySettings.TranslationSettings;
		const FVector CurrentLoc = AimSwayTarget.GetLocation();

		const float Y = FMath::FInterpTo(CurrentLoc.Y, FMath::Clamp(LocSettings.Scale.Y * MouseDeltaInput.X, -LocSettings.Threshold.Y, LocSettings.Threshold.Y), DeltaSeconds, LocSettings.SmoothDamping.Y);
		const float Z = FMath::FInterpTo(CurrentLoc.Z, FMath::Clamp(LocSettings.Scale.Z * MouseDeltaInput.Y, -LocSettings.Threshold.Z, LocSettings.Threshold.Z), DeltaSeconds, LocSettings.SmoothDamping.Z);

		AimSwayTarget.SetLocation(FVector(0.f, Y, Z));
	}

	// --- Move target ---
	{
		const FACFVectorSpringSettings& RotSettings = SwayData.MoveSwaySettings.RotationSettings;
		const FRotator Current = MoveSwayTarget.Rotator();

		const float Roll = FMath::FInterpTo(Current.Roll, FMath::Clamp(RotSettings.Scale.X * MoveInput.X, -RotSettings.Threshold.X, RotSettings.Threshold.X), DeltaSeconds, RotSettings.SmoothDamping.X);
		const float Pitch = FMath::FInterpTo(Current.Pitch, FMath::Clamp(RotSettings.Scale.Y * MoveInput.Y, -RotSettings.Threshold.Y, RotSettings.Threshold.Y), DeltaSeconds, RotSettings.SmoothDamping.Y);
		const float Yaw = FMath::FInterpTo(Current.Yaw, FMath::Clamp(RotSettings.Scale.Z * MoveInput.X, -RotSettings.Threshold.Z, RotSettings.Threshold.Z), DeltaSeconds, RotSettings.SmoothDamping.Z);

		MoveSwayTarget.SetRotation(FRotator(Pitch, Yaw, Roll).Quaternion());

		const FACFVectorSpringSettings& LocSettings = SwayData.MoveSwaySettings.TranslationSettings;
		const FVector CurrentLoc = MoveSwayTarget.GetLocation();

		const float X = FMath::FInterpTo(CurrentLoc.X, FMath::Clamp(LocSettings.Scale.X * MoveInput.Y, -LocSettings.Threshold.X, LocSettings.Threshold.X), DeltaSeconds, LocSettings.SmoothDamping.X);
		const float Y = FMath::FInterpTo(CurrentLoc.Y, FMath::Clamp(LocSettings.Scale.Y * MoveInput.X, -LocSettings.Threshold.Y, LocSettings.Threshold.Y), DeltaSeconds, LocSettings.SmoothDamping.Y);
		const float Z = FMath::FInterpTo(CurrentLoc.Z, FMath::Clamp(LocSettings.Scale.Z * MoveInput.Y, -LocSettings.Threshold.Z, LocSettings.Threshold.Z), DeltaSeconds, LocSettings.SmoothDamping.Z);

		MoveSwayTarget.SetLocation(FVector(X, Y, Z));
	}
}

void UACFProceduralSwayMotionComponent::RefreshAimSway(float DeltaSeconds, const FACFSwayData& SwayData)
{
	const float Weight = SwayScale * FMath::Lerp(1.0f, SwayData.AimScale, AimingState);

	{
		const FACFVectorSpringSettings& S = SwayData.AimSwaySettings.RotationSettings;
		const FRotator Target = AimSwayTarget.Rotator();
		FRotator Out = AimSwayOutput.Rotator();

		Out.Pitch = UKismetMathLibrary::FloatSpringInterp(Out.Pitch, Target.Pitch, AimSpring.Pitch, S.SpringStiffness.Y, S.SpringDamping.Y, DeltaSeconds * S.SmoothSpeed.Y);
		Out.Yaw = UKismetMathLibrary::FloatSpringInterp(Out.Yaw, Target.Yaw, AimSpring.Yaw, S.SpringStiffness.Z, S.SpringDamping.Z, DeltaSeconds * S.SmoothSpeed.Z);
		Out.Roll = UKismetMathLibrary::FloatSpringInterp(Out.Roll, Target.Roll, AimSpring.Roll, S.SpringStiffness.X, S.SpringDamping.X, DeltaSeconds * S.SmoothSpeed.X);

		AimSwayOutput.SetRotation(FQuat::Slerp(FQuat::Identity, Out.Quaternion(), Weight));
	}

	{
		const FACFVectorSpringSettings& S = SwayData.AimSwaySettings.TranslationSettings;
		const FVector Target = AimSwayTarget.GetLocation();
		FVector Out = AimSwayOutput.GetLocation();

		Out.X = UKismetMathLibrary::FloatSpringInterp(Out.X, Target.X, AimSpring.X, S.SpringStiffness.X, S.SpringDamping.X, DeltaSeconds * S.SmoothSpeed.X);
		Out.Y = UKismetMathLibrary::FloatSpringInterp(Out.Y, Target.Y, AimSpring.Y, S.SpringStiffness.Y, S.SpringDamping.Y, DeltaSeconds * S.SmoothSpeed.Y);
		Out.Z = UKismetMathLibrary::FloatSpringInterp(Out.Z, Target.Z, AimSpring.Z, S.SpringStiffness.Z, S.SpringDamping.Z, DeltaSeconds * S.SmoothSpeed.Z);

		AimSwayOutput.SetLocation(FMath::Lerp(FVector::ZeroVector, Out, Weight));
	}
}

void UACFProceduralSwayMotionComponent::RefreshMoveSway(float DeltaSeconds, const FACFSwayData& SwayData)
{
	const float Weight = SwayScale * FMath::Lerp(1.0f, SwayData.AimScale, AimingState);

	{
		const FACFVectorSpringSettings& S = SwayData.MoveSwaySettings.RotationSettings;
		const FRotator Target = MoveSwayTarget.Rotator();
		FRotator Out = MoveSwayOutput.Rotator();

		Out.Pitch = UKismetMathLibrary::FloatSpringInterp(Out.Pitch, Target.Pitch, MoveSpring.Pitch, S.SpringStiffness.Y, S.SpringDamping.Y, DeltaSeconds * S.SmoothSpeed.Y);
		Out.Yaw = UKismetMathLibrary::FloatSpringInterp(Out.Yaw, Target.Yaw, MoveSpring.Yaw, S.SpringStiffness.Z, S.SpringDamping.Z, DeltaSeconds * S.SmoothSpeed.Z);
		Out.Roll = UKismetMathLibrary::FloatSpringInterp(Out.Roll, Target.Roll, MoveSpring.Roll, S.SpringStiffness.X, S.SpringDamping.X, DeltaSeconds * S.SmoothSpeed.X);

		MoveSwayOutput.SetRotation(FQuat::Slerp(FQuat::Identity, Out.Quaternion(), Weight));
	}

	{
		const FACFVectorSpringSettings& S = SwayData.MoveSwaySettings.TranslationSettings;
		const FVector Target = MoveSwayTarget.GetLocation();
		FVector Out = MoveSwayOutput.GetLocation();

		Out.X = UKismetMathLibrary::FloatSpringInterp(Out.X, Target.X, MoveSpring.X, S.SpringStiffness.X, S.SpringDamping.X, DeltaSeconds * S.SmoothSpeed.X);
		Out.Y = UKismetMathLibrary::FloatSpringInterp(Out.Y, Target.Y, MoveSpring.Y, S.SpringStiffness.Y, S.SpringDamping.Y, DeltaSeconds * S.SmoothSpeed.Y);
		Out.Z = UKismetMathLibrary::FloatSpringInterp(Out.Z, Target.Z, MoveSpring.Z, S.SpringStiffness.Z, S.SpringDamping.Z, DeltaSeconds * S.SmoothSpeed.Z);

		MoveSwayOutput.SetLocation(FMath::Lerp(FVector::ZeroVector, Out, Weight));
	}
}

void UACFProceduralSwayMotionComponent::AcquireInputsFromOwner(float DeltaSeconds, const UAnimInstance* AnimInstance, const UACFProceduralPresetSwayMotion* Preset)
{
	if (!Preset || !Preset->bAutoReadFromOwner || !AnimInstance)
	{
		return;
	}

	const APawn* Pawn = Cast<APawn>(AnimInstance->GetOwningActor());
	if (!Pawn)
	{
		// Anim instance is not on a pawn (weapon-owned AnimBP, editor preview). Nothing to read.
		MouseDeltaInput = FVector2D::ZeroVector;
		MoveInput = FVector2D::ZeroVector;
		bHasLastViewRotation = false;
		return;
	}

	RefreshLookInput(DeltaSeconds, Pawn, Preset);
	RefreshMoveInput(Pawn, Preset);
}

void UACFProceduralSwayMotionComponent::RefreshLookInput(float DeltaSeconds, const APawn* Pawn, const UACFProceduralPresetSwayMotion* Preset)
{
	// GetCameraRotation falls back to the replicated base aim rotation, so this still resolves for
	// simulated proxies and spectated pawns, which have no local PlayerController.
	FRotator ViewRotation = UACFFirstPersonFunctionLibrary::GetCameraRotation(Pawn);

	// Switching pawns (respawn, spectator target change) would otherwise spike the delta.
	if (LastViewSource.Get() != Pawn)
	{
		LastViewSource = Pawn;
		bHasLastViewRotation = false;
	}

	if (!Pawn->IsLocallyControlled())
	{
		SmoothedViewRotation = bHasLastViewRotation
			? FMath::RInterpTo(SmoothedViewRotation, ViewRotation, DeltaSeconds, RemoteViewInterpSpeed)
			: ViewRotation;

		ViewRotation = SmoothedViewRotation;
	}

	if (bHasLastViewRotation)
	{
		const FRotator Delta = (ViewRotation - LastViewRotation).GetNormalized();
		MouseDeltaInput.X = Delta.Yaw * Preset->MouseToInputScaleYaw;
		MouseDeltaInput.Y = Delta.Pitch * Preset->MouseToInputScalePitch;
	}
	else
	{
		MouseDeltaInput = FVector2D::ZeroVector;
	}

	LastViewRotation = ViewRotation;
	bHasLastViewRotation = true;
}

void UACFProceduralSwayMotionComponent::RefreshMoveInput(const APawn* Pawn, const UACFProceduralPresetSwayMotion* Preset)
{
	FVector MoveVector = Pawn->GetLastMovementInputVector();

	if (!Pawn->IsLocallyControlled())
	{
		// Movement input is never replicated. Velocity is, so normalise it back into input range.
		const UPawnMovementComponent* MovementComp = Pawn->GetMovementComponent();
		const float MaxSpeed = MovementComp ? MovementComp->GetMaxSpeed() : 0.f;
		const FVector Velocity = Pawn->GetVelocity();

		MoveVector = (MaxSpeed > KINDA_SMALL_NUMBER) ? Velocity / MaxSpeed : Velocity.GetSafeNormal();
	}

	const FRotationMatrix YawMatrix(FRotator(0.f, Pawn->GetActorRotation().Yaw, 0.f));
	const float ForwardMag = FVector::DotProduct(MoveVector, YawMatrix.GetUnitAxis(EAxis::X));
	const float RightMag = FVector::DotProduct(MoveVector, YawMatrix.GetUnitAxis(EAxis::Y));

	MoveInput.X = FMath::Clamp(RightMag * Preset->MoveVectorScale, -1.f, 1.f);
	MoveInput.Y = FMath::Clamp(ForwardMag * Preset->MoveVectorScale, -1.f, 1.f);
}

TSubclassOf<UACFProceduralPreset> UACFProceduralSwayMotionComponent::GetPresetClass() const
{
	return UACFProceduralPresetSwayMotion::StaticClass();
}
