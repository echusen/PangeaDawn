// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Presets/ACFProceduralPreset.h"
#include "ACFProceduralPresetAdditiveMotion.generated.h"

UCLASS(meta=(DisplayName="Preset: Additive Motion"))
class ASCENTFIRSTPERSON_API UACFProceduralPresetAdditiveMotion : public UACFProceduralPreset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Additive")
	float AdditiveScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Additive")
	float AimingAdditiveScale = 1.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Additive", meta=(GetOptions="GetCurveNameOptions"))
	FName MaskAdditiveCurve = FName("MaskAdditive");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Additive", meta=(GetOptions="GetCurveNameOptions"))
	FName XAxisCurve = FName("IK_X");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Additive", meta=(GetOptions="GetCurveNameOptions"))
	FName YAxisCurve = FName("IK_Y");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Additive", meta=(GetOptions="GetCurveNameOptions"))
	FName ZAxisCurve = FName("IK_Z");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Additive", meta=(GetOptions="GetCurveNameOptions"))
	FName PitchAxisCurve = FName("IK_Pitch");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Additive", meta=(GetOptions="GetCurveNameOptions"))
	FName YawAxisCurve = FName("IK_Yaw");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Additive", meta=(GetOptions="GetCurveNameOptions"))
	FName RollAxisCurve = FName("IK_Roll");
	
};
