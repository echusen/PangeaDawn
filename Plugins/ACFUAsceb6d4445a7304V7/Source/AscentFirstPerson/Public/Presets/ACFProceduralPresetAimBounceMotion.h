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

	/**
	 * Bounce strength over the aim blend, read at time = aiming alpha (0 at hip, 1 fully aimed). By default it starts
	 * at 0, peaks at 1 at 0.7 and settles back to 0 at 1, once aiming has finished blending.
	 */
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
