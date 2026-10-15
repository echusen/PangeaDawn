// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "Components/ACFProceduralAimBounceMotionComponent.h"

#include "Presets/ACFProceduralPreset.h"
#include "Presets/ACFProceduralPresetAimBounceMotion.h"

UACFProceduralAimBounceMotionComponent::UACFProceduralAimBounceMotionComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Stage = 3;
}

void UACFProceduralAimBounceMotionComponent::ComputeOutputs(float DeltaSeconds, const UAnimInstance* AnimInstance, const FACFProceduralMotionInput& Input, TArray<FACFProceduralMotionOutput>& Outs)
{
	const UACFProceduralPresetAimBounceMotion* Preset = UACFProceduralPresetData::GetPresetOrDefault<UACFProceduralPresetAimBounceMotion>(GetEffectivePresetData());
	if (!Preset)
	{
		return;
	}

	FAimBounceState& State = Input.bFirstPerson ? StateFP : StateTP;

	const float PreviousAimingState = State.LastAimingState;
	State.LastAimingState = Input.AimingState;

	const bool bApplyToThisView = Input.bFirstPerson ? Preset->bApplyAimingBounceToFP : Preset->bApplyAimingBounceToTP;
	if (!bApplyToThisView)
	{
		return;
	}

	const FRichCurve* BounceCurve = Preset->AimingBounceCurve.GetRichCurveConst();
	if (!BounceCurve)
	{
		return;
	}

	// Latch the direction at the extremes, otherwise track which way the blend is moving.
	if (Input.AimingState >= 0.99f)
	{
		State.bIsAimingIn = false;
	}
	else if (Input.AimingState <= 0.01f)
	{
		State.bIsAimingIn = true;
	}
	else if (FMath::Abs(Input.AimingState - PreviousAimingState) > KINDA_SMALL_NUMBER)
	{
		State.bIsAimingIn = Input.AimingState > PreviousAimingState;
	}

	const FACFProceduralAimBounceMotionOffset& TargetOffset = Input.bFirstPerson
		? (State.bIsAimingIn ? Preset->AimInOffsetFP : Preset->AimOutOffsetFP)
		: (State.bIsAimingIn ? Preset->AimInOffsetTP : Preset->AimOutOffsetTP);

	State.CurrentRotationOffset = FMath::RInterpTo(State.CurrentRotationOffset, TargetOffset.Rotation, DeltaSeconds, Preset->OffsetInterpolationSpeed);
	State.CurrentTranslationOffset = FMath::VInterpTo(State.CurrentTranslationOffset, TargetOffset.Translation, DeltaSeconds, Preset->OffsetInterpolationSpeed);

	const float BounceAlpha = BounceCurve->Eval(Input.AimingState);
	if (BounceAlpha <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	if (State.CurrentRotationOffset.IsZero() && State.CurrentTranslationOffset.IsZero())
	{
		return;
	}

	FACFProceduralMotionOutput Out;
	Out.Stage = Stage;
	Out.Composition = EACFProceduralMotionComposition::Additive;
	Out.Space = EACFProceduralMotionOutputSpace::Component;
	Out.Transform = FTransform(State.CurrentRotationOffset, State.CurrentTranslationOffset);
	Out.Weight = BounceAlpha;
	Outs.Add(Out);
}

TSubclassOf<UACFProceduralPreset> UACFProceduralAimBounceMotionComponent::GetPresetClass() const
{
	return UACFProceduralPresetAimBounceMotion::StaticClass();
}
