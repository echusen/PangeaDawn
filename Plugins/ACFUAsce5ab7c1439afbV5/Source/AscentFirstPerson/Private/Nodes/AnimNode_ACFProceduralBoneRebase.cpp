// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "Nodes/AnimNode_ACFProceduralBoneRebase.h"
#include "Animation/AnimInstanceProxy.h"

void FACFBonePair::Initialize(const FBoneContainer& RequiredBones)
{
	SourceBone.Initialize(RequiredBones);
	TargetBone.Initialize(RequiredBones);
}

bool FACFBonePair::IsValidToEvaluate(const FBoneContainer& RequiredBones) const
{
	return SourceBone.IsValidToEvaluate(RequiredBones) && TargetBone.IsValidToEvaluate(RequiredBones);
}

bool FACFBonePair::IsTargetValidToEvaluate(const FBoneContainer& RequiredBones) const
{
	return TargetBone.IsValidToEvaluate(RequiredBones);
}

FAnimNode_ACFProceduralBoneRebase::FAnimNode_ACFProceduralBoneRebase()
{
	AlphaInputType = EAnimAlphaInputType::Float;
}

void FAnimNode_ACFProceduralBoneRebase::InitializeBoneReferences(const FBoneContainer& RequiredBones)
{
	PivotPair.Initialize(RequiredBones);
	for (FACFBonePair& BonePair : BonePairs)
	{
		BonePair.Initialize(RequiredBones);
	}

	ResolvedPivotSourceOverride.Initialize(RequiredBones);
}

bool FAnimNode_ACFProceduralBoneRebase::IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones)
{
	// Anything resolvable is enough. ProcessBonePair already skips a pair it cannot handle, so requiring
	// every pair meant one bad entry silently disabled the whole rebase and the rest of the stack ran on
	// unrebased bones — FINDINGS #15.
	if (PivotPair.IsTargetValidToEvaluate(RequiredBones))
	{
		return true;
	}

	for (const FACFBonePair& BonePair : BonePairs)
	{
		if (BonePair.IsValidToEvaluate(RequiredBones))
		{
			return true;
		}
	}

	return false;
}

void FAnimNode_ACFProceduralBoneRebase::EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms)
{
	const FBoneContainer& BoneContainer = Output.Pose.GetPose().GetBoneContainer();

	// A bound pin changes per held item, not per frame, so the name lookup only runs when it does.
	if (ResolvedPivotSourceOverride.BoneName != PivotSourceBoneOverride)
	{
		ResolvedPivotSourceOverride = FBoneReference(PivotSourceBoneOverride);
		ResolvedPivotSourceOverride.Initialize(BoneContainer);
	}

	// The holder's source is optional: with none set the offsets still land, on wherever the animation
	// already put the bone.
	if (ResolvedPivotSourceOverride.IsValidToEvaluate(BoneContainer))
	{
		FACFBonePair OverriddenPivotPair = PivotPair;
		OverriddenPivotPair.SourceBone = ResolvedPivotSourceOverride;
		ProcessBonePair(OverriddenPivotPair, Output, OutBoneTransforms, true, &PivotOffsetLocation, &PivotOffsetRotation);
	}
	else
	{
		ProcessBonePair(PivotPair, Output, OutBoneTransforms, true, &PivotOffsetLocation, &PivotOffsetRotation);
	}

	for (FACFBonePair& BonePair : BonePairs)
	{
		ProcessBonePair(BonePair, Output, OutBoneTransforms);
	}

	OutBoneTransforms.Sort([](const FBoneTransform& A, const FBoneTransform& B)
	{
		return A.BoneIndex < B.BoneIndex;
	});
}

void FAnimNode_ACFProceduralBoneRebase::ProcessBonePair(FACFBonePair& Pair, FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms, bool bSourceOptional /*= false*/, const FVector* LocationOffset /*= nullptr*/, const FRotator* RotationOffset /*= nullptr*/)
{
	const FBoneContainer& BoneContainer = Output.Pose.GetPose().GetBoneContainer();

	if (!Pair.IsTargetValidToEvaluate(BoneContainer))
	{
		return;
	}

	const bool bHasSource = Pair.SourceBone.IsValidToEvaluate(BoneContainer);
	if (!bHasSource && !bSourceOptional)
	{
		return;
	}

	FCompactPoseBoneIndex TargetIndex = Pair.TargetBone.GetCompactPoseIndex(BoneContainer);

	// No source means "leave it where it is and just offset it", which is what a holder that is already a
	// true virtual bone wants.
	FCompactPoseBoneIndex ReadIndex = bHasSource ? Pair.SourceBone.GetCompactPoseIndex(BoneContainer) : TargetIndex;

	FTransform NewTransform = Output.Pose.GetComponentSpaceTransform(ReadIndex);

	if (RotationOffset && !RotationOffset->IsZero())
	{
		FQuat RotOffsetQuat = FQuat(*RotationOffset);
		NewTransform.SetRotation(NewTransform.GetRotation() * RotOffsetQuat);
	}

	if (LocationOffset && !LocationOffset->IsZero())
	{
		FVector LocalOffset = NewTransform.GetRotation().RotateVector(*LocationOffset);
		NewTransform.AddToTranslation(LocalOffset);
	}

	OutBoneTransforms.Add(FBoneTransform(TargetIndex, NewTransform));
}