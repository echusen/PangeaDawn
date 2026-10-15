// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "Nodes/AnimNode_ACFProceduralHandOffset.h"
#include "Animation/AnimInstanceProxy.h"

FAnimNode_ACFProceduralHandOffset::FAnimNode_ACFProceduralHandOffset()
{
	AlphaInputType = EAnimAlphaInputType::Float;
}

void FAnimNode_ACFProceduralHandOffset::InitializeBoneReferences(const FBoneContainer& RequiredBones)
{
	HandLeftBone.Initialize(RequiredBones);
	HandRightBone.Initialize(RequiredBones);
	LowerarmLeftBone.Initialize(RequiredBones);
	LowerarmRightBone.Initialize(RequiredBones);
	ClavicleLeftBone.Initialize(RequiredBones);
	ClavicleRightBone.Initialize(RequiredBones);
}

bool FAnimNode_ACFProceduralHandOffset::IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones)
{
	// The clavicles are optional, so setups made before they existed keep evaluating.
	return HandLeftBone.IsValidToEvaluate(RequiredBones)
		&& HandRightBone.IsValidToEvaluate(RequiredBones)
		&& LowerarmLeftBone.IsValidToEvaluate(RequiredBones)
		&& LowerarmRightBone.IsValidToEvaluate(RequiredBones);
}

void FAnimNode_ACFProceduralHandOffset::EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms)
{
	ProcessBone(HandLeftBone, Output, OutBoneTransforms, HandOffsetData.HandLeftOffsetLocation, HandOffsetData.HandLeftOffsetRotation
		, FMath::Clamp(HandOffsetAlpha * (1.0f - Output.Curve.Get(MaskHandOffsetCurve)), 0.0f, 1.0f));
	ProcessBone(HandRightBone, Output, OutBoneTransforms, HandOffsetData.HandRightOffsetLocation, HandOffsetData.HandRightOffsetRotation
		, FMath::Clamp(HandOffsetAlpha * (1.0f - Output.Curve.Get(MaskHandOffsetCurve)), 0.0f, 1.0f));
	ProcessBone(LowerarmLeftBone, Output, OutBoneTransforms, HandOffsetData.LowerarmLeftOffsetLocation, HandOffsetData.LowerarmLeftOffsetRotation
		, FMath::Clamp(LowerarmOffsetAlpha * (1.0f - Output.Curve.Get(MaskLowerarmOffsetCurve)), 0.0f, 1.0f));
	ProcessBone(LowerarmRightBone, Output, OutBoneTransforms, HandOffsetData.LowerarmRightOffsetLocation, HandOffsetData.LowerarmRightOffsetRotation
		, FMath::Clamp(LowerarmOffsetAlpha * (1.0f - Output.Curve.Get(MaskLowerarmOffsetCurve)), 0.0f, 1.0f));
	ProcessBone(ClavicleLeftBone, Output, OutBoneTransforms, HandOffsetData.ClavicleLeftOffsetLocation, FRotator::ZeroRotator
		, FMath::Clamp(ClavicleOffsetAlpha * (1.0f - Output.Curve.Get(MaskClavicleOffsetCurve)), 0.0f, 1.0f));
	ProcessBone(ClavicleRightBone, Output, OutBoneTransforms, HandOffsetData.ClavicleRightOffsetLocation, FRotator::ZeroRotator
		, FMath::Clamp(ClavicleOffsetAlpha * (1.0f - Output.Curve.Get(MaskClavicleOffsetCurve)), 0.0f, 1.0f));

	OutBoneTransforms.Sort([](const FBoneTransform& A, const FBoneTransform& B)
	{
		return A.BoneIndex < B.BoneIndex;
	});
}

void FAnimNode_ACFProceduralHandOffset::ProcessBone(FBoneReference& BoneRef, FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms, const FVector& LocationOffset, const FRotator& RotationOffset, float InAlpha /*= 1.0f*/)
{
	const FBoneContainer& BoneContainer = Output.Pose.GetPose().GetBoneContainer();

	if (!BoneRef.IsValidToEvaluate(BoneContainer))
	{
		return;
	}

	FCompactPoseBoneIndex TargetIndex = BoneRef.GetCompactPoseIndex(BoneContainer);

	FTransform NewTransform = Output.Pose.GetComponentSpaceTransform(TargetIndex);

	if (!RotationOffset.IsZero())
	{
		const FQuat RotOffsetQuat = FQuat::Slerp(FQuat::Identity, FQuat(RotationOffset), InAlpha);
		NewTransform.SetRotation(NewTransform.GetRotation() * RotOffsetQuat);
	}

	if (!LocationOffset.IsZero())
	{
		const FVector LocalOffset = LocationOffset * InAlpha;
		NewTransform.AddToTranslation(LocalOffset);
	}

	OutBoneTransforms.Add(FBoneTransform(TargetIndex, NewTransform));
}
