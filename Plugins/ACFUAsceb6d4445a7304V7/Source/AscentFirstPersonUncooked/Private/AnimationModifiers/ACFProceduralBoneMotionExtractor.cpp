// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "AnimationModifiers/ACFProceduralBoneMotionExtractor.h"

#include "AnimationBlueprintLibrary.h"
#include "AnimPose.h"

UACFProceduralBoneMotionExtractor::UACFProceduralBoneMotionExtractor()
{
	CurveName = TEXT("IK");
	BoneName = TEXT("ik_hand_gun");

	SampleRate = 60.0f;
	LocationScale = FVector::OneVector;
	RotationScale = FVector::OneVector;

	CurveNames = {
		TEXT("_X"),
		TEXT("_Y"),
		TEXT("_Z"),
		TEXT("_Pitch"),
		TEXT("_Yaw"),
		TEXT("_Roll"),
	};
}

void UACFProceduralBoneMotionExtractor::OnApply_Implementation(UAnimSequence* AnimationSequence)
{
	if (!SourceAnimation)
	{
		SourceAnimation = AnimationSequence;
	}

	for (const FName& CurrCurveName : CurveNames)
	{
		const FName TargetCurveName(CurveName.ToString() + CurrCurveName.ToString());
		
		if (UAnimationBlueprintLibrary::DoesCurveExist(AnimationSequence, TargetCurveName, ERawCurveTrackTypes::RCT_Float))
		{
			UAnimationBlueprintLibrary::RemoveCurve(AnimationSequence, TargetCurveName);
		}

		UAnimationBlueprintLibrary::AddCurve(AnimationSequence, TargetCurveName);
	}

	FAnimPoseEvaluationOptions EvaluationOptions;
	EvaluationOptions.EvaluationType = EAnimDataEvalType::Source;
	EvaluationOptions.bShouldRetarget = false;
	EvaluationOptions.bExtractRootMotion = false;
	EvaluationOptions.bIncorporateRootMotionIntoPose = true;
	EvaluationOptions.bRetrieveAdditiveAsFullPose = true;
	EvaluationOptions.bEvaluateCurves = true;

	{
		FAnimPose AnimPose;
		UAnimPoseExtensions::GetAnimPoseAtTime(SourceAnimation, 0.0f, EvaluationOptions, AnimPose);

		RefPose = UAnimPoseExtensions::GetBonePose(AnimPose, BoneName, EAnimPoseSpaces::World);
	}

	float AnimPlayLength = AnimationSequence->GetPlayLength();
	Playback = 0.0f;

	while (Playback < AnimPlayLength)
	{
		FAnimPose AnimPose;
		UAnimPoseExtensions::GetAnimPoseAtTime(AnimationSequence, Playback, EvaluationOptions, AnimPose);

		const FTransform& CurrPose = UAnimPoseExtensions::GetBonePose(AnimPose, BoneName, EAnimPoseSpaces::World);

		FTransform OriginalDelta = CurrPose.GetRelativeTransform(RefPose);

		FTransform OrientationFix = FTransform(OrientationOffset.Quaternion());
		FTransform AdjustedDelta = OrientationFix * OriginalDelta * OrientationFix.Inverse();

		FTransform LocalOffsetTransform = FTransform(RotationOffset, LocationOffset);
		FTransform FinalTransform = AdjustedDelta * LocalOffsetTransform;

		AddTransformKey(AnimationSequence, FinalTransform, Playback);

		Playback = Playback + (1.0f / SampleRate);
	}
}

void UACFProceduralBoneMotionExtractor::OnRevert_Implementation(UAnimSequence* AnimationSequence)
{
	for (const FName& CurrCurveName : CurveNames)
	{
		const FName TargetCurveName(CurveName.ToString() + CurrCurveName.ToString());
		UAnimationBlueprintLibrary::RemoveCurve(AnimationSequence, TargetCurveName);
	}
}

void UACFProceduralBoneMotionExtractor::AddTransformKey(UAnimSequence* AnimationSequence, const FTransform& InTransform, float InTime)
{
	if (!ensure(CurveNames.Num() >= 6)) return;

	const FVector TargetLoc = InTransform.GetLocation() * LocationScale;

	FRotator TargetRot = InTransform.Rotator();
	TargetRot.Roll *= RotationScale.X;
	TargetRot.Pitch *= RotationScale.Y;
	TargetRot.Yaw *= RotationScale.Z;

	const FName CurveNameX(CurveName.ToString() + CurveNames[0].ToString());
	const FName CurveNameY(CurveName.ToString() + CurveNames[1].ToString());
	const FName CurveNameZ(CurveName.ToString() + CurveNames[2].ToString());

	UAnimationBlueprintLibrary::AddFloatCurveKey(AnimationSequence, CurveNameX, InTime, TargetLoc.X);
	UAnimationBlueprintLibrary::AddFloatCurveKey(AnimationSequence, CurveNameY, InTime, TargetLoc.Y);
	UAnimationBlueprintLibrary::AddFloatCurveKey(AnimationSequence, CurveNameZ, InTime, TargetLoc.Z);

	const FName CurveNamePitch(CurveName.ToString() + CurveNames[3].ToString());
	const FName CurveNameYaw(CurveName.ToString() + CurveNames[4].ToString());
	const FName CurveNameRoll(CurveName.ToString() + CurveNames[5].ToString());

	UAnimationBlueprintLibrary::AddFloatCurveKey(AnimationSequence, CurveNamePitch, InTime, TargetRot.Pitch);
	UAnimationBlueprintLibrary::AddFloatCurveKey(AnimationSequence, CurveNameYaw, InTime, TargetRot.Yaw);
	UAnimationBlueprintLibrary::AddFloatCurveKey(AnimationSequence, CurveNameRoll, InTime, TargetRot.Roll);
}
