// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "Components/ACFProceduralOffsetMotionComponent.h"

#include "Animation/AnimInstance.h"

#include "Presets/ACFProceduralPresetOffsetMotion.h"

UACFProceduralOffsetMotionComponent::UACFProceduralOffsetMotionComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Stage = 1;
}

void UACFProceduralOffsetMotionComponent::ComputeOutputs(float DeltaSeconds, const UAnimInstance* AnimInstance, const FACFProceduralMotionInput& Input, TArray<FACFProceduralMotionOutput>& Outs)
{
	const UACFProceduralPresetOffsetMotion* Preset = UACFProceduralPresetData::GetPresetOrDefault<UACFProceduralPresetOffsetMotion>(GetEffectivePresetData());
	if (!Preset)
	{
		return;
	}

	if (Input.bFirstPerson)
	{
		const float Weight = OffsetAlpha * (1.0f - FMath::Clamp(ReadCurve(AnimInstance, Preset->Curve_MaskOffsetFP), 0.0f, 1.0f));
		if (Weight <= KINDA_SMALL_NUMBER)
		{
			return;
		}

		FACFProceduralMotionOutput Out;
		Out.Stage = Stage;
		Out.Composition = EACFProceduralMotionComposition::Additive;
		Out.Space = EACFProceduralMotionOutputSpace::Component;
		Out.Transform = FTransform(Preset->OffsetRotationFP, Preset->OffsetLocationFP);
		Out.Weight = Weight;
		Outs.Add(Out);

		return;
	}

	const float Weight = OffsetAlpha * (1.0f - FMath::Clamp(ReadCurve(AnimInstance, Preset->Curve_MaskOffsetTP), 0.0f, 1.0f));
	if (Weight <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	// Third person has no rotation offset, only a hip-to-aim location blend.
	const FVector Location = FMath::Lerp(Preset->OffsetLocationTP, Preset->AimingOffsetLocationTP, FMath::Clamp(Input.AimingState, 0.f, 1.f));

	FACFProceduralMotionOutput Out;
	Out.Stage = Stage;
	Out.Composition = EACFProceduralMotionComposition::Additive;
	Out.Space = EACFProceduralMotionOutputSpace::Component;
	Out.Transform = FTransform(FQuat::Identity, Location);
	Out.Weight = Weight;
	Outs.Add(Out);
}

TSubclassOf<UACFProceduralPreset> UACFProceduralOffsetMotionComponent::GetPresetClass() const
{
	return UACFProceduralPresetOffsetMotion::StaticClass();
}
