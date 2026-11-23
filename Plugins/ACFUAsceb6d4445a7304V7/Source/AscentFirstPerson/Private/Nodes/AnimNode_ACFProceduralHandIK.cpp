// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "Nodes/AnimNode_ACFProceduralHandIK.h"
#include "TwoBoneIK.h"
#include "Animation/AnimInstanceProxy.h"
#include "AnimationRuntime.h"

FAnimNode_ACFProceduralHandIK::FAnimNode_ACFProceduralHandIK()
{
	AlphaInputType = EAnimAlphaInputType::Float;
}

void FAnimNode_ACFProceduralHandIK::InitializeBoneReferences(const FBoneContainer& RequiredBones)
{
	HandL.Initialize(RequiredBones);
	TargetHandL.Initialize(RequiredBones);
	LowerarmL.Initialize(RequiredBones);

	HandR.Initialize(RequiredBones);
	TargetHandR.Initialize(RequiredBones);
	LowerarmR.Initialize(RequiredBones);
}

bool FAnimNode_ACFProceduralHandIK::IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones)
{
	return HandL.IsValidToEvaluate(RequiredBones) && HandR.IsValidToEvaluate(RequiredBones);
}

void FAnimNode_ACFProceduralHandIK::EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms)
{
	const float LeftCurveValue = MaskHandLeftIKCurve.IsNone() ? 1.0f : (1.0f - Output.Curve.Get(MaskHandLeftIKCurve));
	const float FinalLeftAlpha = FMath::Clamp(HandLeftIKAlpha * LeftCurveValue, 0.0f, 1.0f);

	if (HandL.IsValidToEvaluate() && TargetHandL.IsValidToEvaluate() && LowerarmL.IsValidToEvaluate())
	{
		ProcessHandIK(Output, OutBoneTransforms, HandL, TargetHandL, LowerarmL, FinalLeftAlpha);
	}

	const float RightCurveValue = MaskHandRightIKCurve.IsNone() ? 1.0f : (1.0f - Output.Curve.Get(MaskHandRightIKCurve));
	const float FinalRightAlpha = FMath::Clamp(HandRightIKAlpha * RightCurveValue, 0.0f, 1.0f);

	if (HandR.IsValidToEvaluate() && TargetHandR.IsValidToEvaluate() && LowerarmR.IsValidToEvaluate())
	{
		ProcessHandIK(Output, OutBoneTransforms, HandR, TargetHandR, LowerarmR, FinalRightAlpha);
	}

	OutBoneTransforms.Sort([](const FBoneTransform& A, const FBoneTransform& B)
	{
		return A.BoneIndex < B.BoneIndex;
	});
}

void FAnimNode_ACFProceduralHandIK::ProcessHandIK(
	FComponentSpacePoseContext& Output,
	TArray<FBoneTransform>& OutBoneTransforms,
	const FBoneReference& HandBone,
	const FBoneReference& TargetBone,
	const FBoneReference& JointBone,
	float InAlpha /*= 1.0f*/)
{
	if (InAlpha <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const FBoneContainer& BoneContainer = Output.Pose.GetPose().GetBoneContainer();

	// Get Indices
	FCompactPoseBoneIndex IKBoneIndex = HandBone.GetCompactPoseIndex(BoneContainer);
	FCompactPoseBoneIndex TargetBoneIndex = TargetBone.GetCompactPoseIndex(BoneContainer);
	FCompactPoseBoneIndex LowerLimbIndex = BoneContainer.GetParentBoneIndex(IKBoneIndex);
	FCompactPoseBoneIndex UpperLimbIndex = BoneContainer.GetParentBoneIndex(LowerLimbIndex);

	if (LowerLimbIndex == INDEX_NONE || UpperLimbIndex == INDEX_NONE) return;

	// 1. Get Original Transforms (We need these for blending later)
	const FTransform OriginalUpperTransform = Output.Pose.GetComponentSpaceTransform(UpperLimbIndex);
	const FTransform OriginalLowerTransform = Output.Pose.GetComponentSpaceTransform(LowerLimbIndex);
	const FTransform OriginalEndTransform = Output.Pose.GetComponentSpaceTransform(IKBoneIndex);

	// 2. Create Mutable Copies for Solving
	FTransform UpperLimbCSTransform = OriginalUpperTransform;
	FTransform LowerLimbCSTransform = OriginalLowerTransform;
	FTransform EndBoneCSTransform = OriginalEndTransform;

	// 3. Resolve Targets
	FTransform EffectorTransform = Output.Pose.GetComponentSpaceTransform(TargetBoneIndex);
	FVector DesiredPos = EffectorTransform.GetTranslation();

	FCompactPoseBoneIndex JointTargetIndex = JointBone.GetCompactPoseIndex(BoneContainer);
	FTransform JointTargetTransform = Output.Pose.GetComponentSpaceTransform(JointTargetIndex);
	FVector JointTargetPos = JointTargetTransform.GetTranslation();

	// 4. Solve IK (Updates the Mutable Copies)
	AnimationCore::SolveTwoBoneIK(
		UpperLimbCSTransform,
		LowerLimbCSTransform,
		EndBoneCSTransform,
		JointTargetPos,
		DesiredPos,
		false,
		1.0f,
		1.2f
	);

	// 5. Apply Rotation (Snap to Effector)
	EndBoneCSTransform.SetRotation(EffectorTransform.GetRotation());

	// 6. Blend Logic
	// If InAlpha is not full (1.0), we interpolate between the Original (Anim) and the Solved (IK)
	if (InAlpha < 1.0f - KINDA_SMALL_NUMBER)
	{
		UpperLimbCSTransform.Blend(OriginalUpperTransform, UpperLimbCSTransform, InAlpha);
		LowerLimbCSTransform.Blend(OriginalLowerTransform, LowerLimbCSTransform, InAlpha);
		EndBoneCSTransform.Blend(OriginalEndTransform, EndBoneCSTransform, InAlpha);
	}

	// 7. Output Result
	OutBoneTransforms.Add(FBoneTransform(UpperLimbIndex, UpperLimbCSTransform));
	OutBoneTransforms.Add(FBoneTransform(LowerLimbIndex, LowerLimbCSTransform));
	OutBoneTransforms.Add(FBoneTransform(IKBoneIndex, EndBoneCSTransform));
}