// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AnimationModifier.h"
#include "ACFProceduralBoneMotionExtractor.generated.h"

class UAnimSequence;

UCLASS(meta=(DisplayName="Procedural Bone Motion Extractor Modifier"))
class ASCENTFIRSTPERSONUNCOOKED_API UACFProceduralBoneMotionExtractor : public UAnimationModifier
{
	GENERATED_BODY()

public:
	UACFProceduralBoneMotionExtractor();

	// UAnimationModifier Interface
	virtual void OnApply_Implementation(UAnimSequence* AnimationSequence) override;
	virtual void OnRevert_Implementation(UAnimSequence* AnimationSequence) override;

protected:
	void AddTransformKey(UAnimSequence* AnimationSequence, const FTransform& InTransform, float InTime);

public:
	UPROPERTY(EditAnywhere, Category="Settings")
	FName CurveName;

	UPROPERTY(EditAnywhere, Category="Settings")
	FName BoneName;

	UPROPERTY(EditAnywhere, Category="Settings")
	TObjectPtr<UAnimSequence> SourceAnimation;

	UPROPERTY(EditAnywhere, Category="Settings")
	float SampleRate;

	UPROPERTY(EditAnywhere, Category="Settings")
	FVector LocationScale;

	UPROPERTY(EditAnywhere, Category="Settings")
	FVector RotationScale;

	UPROPERTY(EditAnywhere, Category="Settings")
	FRotator OrientationOffset;

	UPROPERTY(EditAnywhere, Category = "Settings")
	FVector LocationOffset;

	UPROPERTY(EditAnywhere, Category = "Settings")
	FRotator RotationOffset;

protected:
	UPROPERTY()
	TArray<FName> CurveNames;

	UPROPERTY()
	float Playback;

	UPROPERTY()
	FTransform RefPose;
	
};
