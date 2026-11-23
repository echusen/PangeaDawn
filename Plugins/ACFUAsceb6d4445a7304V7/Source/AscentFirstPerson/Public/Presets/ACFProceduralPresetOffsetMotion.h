// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ACFProceduralPreset.h"
#include "ACFProceduralPresetOffsetMotion.generated.h"

UCLASS(meta=(DisplayName="Preset: Offset Motion"))
class ASCENTFIRSTPERSON_API UACFProceduralPresetOffsetMotion : public UACFProceduralPreset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="First Person")
	FRotator OffsetRotationFP = FRotator::ZeroRotator;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="First Person")
	FVector OffsetLocationFP = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="First Person", meta=(GetOptions="GetCurveNameOptions"))
	FName Curve_MaskOffsetFP = TEXT("MaskOffset");
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Third Person")
	FVector OffsetLocationTP = FVector::ZeroVector;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Third Person")
	FVector AimingOffsetLocationTP = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Third Person", meta=(GetOptions="GetCurveNameOptions"))
	FName Curve_MaskOffsetTP = TEXT("MaskOffset");

};
