// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Presets/ACFProceduralPreset.h"
#include "Curves/CurveFloat.h"
#include "ACFProceduralPresetAimBounceMotion.generated.h"

USTRUCT(BlueprintType)
struct ASCENTFIRSTPERSON_API FACFProceduralAimBounceMotionOffset
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Aim Bounce")
	FRotator Rotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Aim Bounce")
	FVector Translation = FVector::ZeroVector;
};

UCLASS(meta=(DisplayName="Preset: Aim Bounce Motion"))
class ASCENTFIRSTPERSON_API UACFProceduralPresetAimBounceMotion : public UACFProceduralPreset
{
	GENERATED_BODY()

public:
	UACFProceduralPresetAimBounceMotion();

	// Curve used for apply Aiming Bounce, by default it starts at 0.0, peaking 1.0 at 0.5 time (mid Aiming), reseting back to 0.0 at 1.0 time (aiming finished).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Aim Bounce")
	FRuntimeFloatCurve AimingBounceCurve;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Aim Bounce")
	float OffsetInterpolationSpeed = 12.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Aim Bounce")
	bool bApplyAimingBounceToFP = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Aim Bounce", meta=(EditCondition="bApplyAimingBounceToFP", EditConditionHides))
	FACFProceduralAimBounceMotionOffset AimInOffsetFP;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Aim Bounce", meta=(EditCondition="bApplyAimingBounceToFP", EditConditionHides))
	FACFProceduralAimBounceMotionOffset AimOutOffsetFP;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Aim Bounce")
	bool bApplyAimingBounceToTP = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Aim Bounce", meta=(EditCondition="bApplyAimingBounceToTP", EditConditionHides))
	FACFProceduralAimBounceMotionOffset AimInOffsetTP;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Aim Bounce", meta=(EditCondition="bApplyAimingBounceToTP", EditConditionHides))
	FACFProceduralAimBounceMotionOffset AimOutOffsetTP;
	
};
