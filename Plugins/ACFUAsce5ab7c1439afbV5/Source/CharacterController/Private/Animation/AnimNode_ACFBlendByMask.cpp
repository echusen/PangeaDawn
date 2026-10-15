// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "Animation/AnimNode_ACFBlendByMask.h"

#include "Animation/ACFBoneBlendMask.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimTypes.h"
#include "Animation/AttributesRuntime.h"
#include "AnimationRuntime.h"
#include "BonePose.h"
#include "Math/ScalarRegister.h"
#include "Misc/MemStack.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimNode_ACFBlendByMask)

void FAnimNode_ACFBlendByMask::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(Initialize_AnyThread)
	FAnimNode_Base::Initialize_AnyThread(Context);

	BasePose.Initialize(Context);
	BlendPose.Initialize(Context);

	bSnapWeights = true;
	LastBlendMask = nullptr;
	LastAnimation = nullptr;
	BlendPoseTransitionAlpha = 1.f;
	BlendPoseTransitionDuration = 0.f;
	LastUpdateDeltaTime = 0.f;
	bPendingBlendPoseTransition = false;
	CachedBlendPoseTransforms.Reset();
	PreviousBlendPoseTransforms.Reset();
}

void FAnimNode_ACFBlendByMask::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(CacheBones_AnyThread)
	BasePose.CacheBones(Context);
	BlendPose.CacheBones(Context);
}

void FAnimNode_ACFBlendByMask::RebuildTargetBoneWeights(const FBoneContainer& RequiredBones)
{
	const int32 NumBones = RequiredBones.GetCompactPoseNumBones();

	// On LOD changes the compact pose indices are remapped, so interpolating from
	// the previous weights would blend unrelated bones: snap instead
	if (RequiredBones.GetSerialNumber() != RequiredBonesSerialNumber)
	{
		RequiredBonesSerialNumber = RequiredBones.GetSerialNumber();
		bSnapWeights = true;
	}

	TargetBoneWeights.Reset(NumBones);
	TargetBoneWeights.AddZeroed(NumBones);
	TargetBlendSpaces.Reset(NumBones);
	TargetBlendSpaces.AddDefaulted(NumBones);

	const UACFBoneBlendMask* Mask = BlendMask;
	if (!Mask || Mask->Branches.IsEmpty())
	{
		return;
	}

	TMap<FName, float> BranchAlphas;
	BranchAlphas.Reserve(Mask->Branches.Num());
	TMap<FName, EACFMaskBlendSpace> BranchSpaces;
	BranchSpaces.Reserve(Mask->Branches.Num());
	for (const FACFBoneBlendBranch& Branch : Mask->Branches)
	{
		if (Branch.Bone.BoneName != NAME_None)
		{
			BranchAlphas.Add(Branch.Bone.BoneName, FMath::Clamp(Branch.Alpha, 0.f, 1.f));
			BranchSpaces.Add(Branch.Bone.BoneName, Branch.BlendSpace);
		}
	}

	const FReferenceSkeleton& RefSkeleton = RequiredBones.GetReferenceSkeleton();

	// Compact pose bones are sorted parents-first, so each bone can inherit the
	// weight of its parent unless it is a branch point itself. This gives deeper
	// branch points priority over parent ones.
	for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
	{
		const FCompactPoseBoneIndex CompactIndex(BoneIndex);
		const FName BoneName = RefSkeleton.GetBoneName(RequiredBones.MakeMeshPoseIndex(CompactIndex).GetInt());

		if (const float* BranchAlpha = BranchAlphas.Find(BoneName))
		{
			TargetBoneWeights[BoneIndex] = *BranchAlpha;
			TargetBlendSpaces[BoneIndex] = BranchSpaces.FindRef(BoneName);
		}
		else
		{
			const FCompactPoseBoneIndex ParentIndex = RequiredBones.GetParentBoneIndex(CompactIndex);
			if (ParentIndex != INDEX_NONE)
			{
				TargetBoneWeights[BoneIndex] = TargetBoneWeights[ParentIndex.GetInt()];
				TargetBlendSpaces[BoneIndex] = TargetBlendSpaces[ParentIndex.GetInt()];
			}
		}
	}
}

void FAnimNode_ACFBlendByMask::Update_AnyThread(const FAnimationUpdateContext& Context)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(Update_AnyThread)

	bBlendPoseRelevant = false;
	float RootMotionWeight = 0.f;

	if (IsLODEnabled(Context.AnimInstanceProxy))
	{
		GetEvaluateGraphExposedInputs().Execute(Context);

		const float DeltaTime = Context.GetDeltaTime();
		LastUpdateDeltaTime = DeltaTime;

		// A mask or animation swap starts a BlendTime pose cross-fade so weights/pose never snap.
		const UACFBoneBlendMask* CurrentMaskPtr = BlendMask;
		const UAnimSequenceBase* CurrentAnimPtr = CurrentAnimation;
		if (bCrossFadeBlendPose && !bSnapWeights && (CurrentMaskPtr != LastBlendMask || CurrentAnimPtr != LastAnimation))
		{
			bPendingBlendPoseTransition = true;
		}
		LastBlendMask = CurrentMaskPtr;
		LastAnimation = CurrentAnimPtr;

		// Rebuilt every update so both mask swaps (pin) and value changes inside the
		// same asset are picked up; the interpolation below smooths any change
		RebuildTargetBoneWeights(Context.AnimInstanceProxy->GetRequiredBones());

		// Global intensity from the OverlayBlendAlpha pin (default 1 = full mask, any AnimBP).
		const float ClampedOverlayBlendAlpha = FMath::Clamp(OverlayBlendAlpha, 0.f, 1.f);
		const int32 NumBones = TargetBoneWeights.Num();
		if (ClampedOverlayBlendAlpha < 1.f - ZERO_ANIMWEIGHT_THRESH)
		{
			for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
			{
				TargetBoneWeights[BoneIndex] *= ClampedOverlayBlendAlpha;
			}
		}

		if (bSnapWeights)
		{
			CurrentBoneWeights = TargetBoneWeights;
			bSnapWeights = false;
		}
		else if (BlendTime <= UE_KINDA_SMALL_NUMBER)
		{
			CurrentBoneWeights = TargetBoneWeights;
		}
		else
		{
			CurrentBoneWeights.SetNumZeroed(NumBones);
			const float InterpSpeed = 1.f / BlendTime;
			for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
			{
				CurrentBoneWeights[BoneIndex] = FMath::FInterpConstantTo(CurrentBoneWeights[BoneIndex], TargetBoneWeights[BoneIndex], DeltaTime, InterpSpeed);
			}
		}

		MaxCurrentWeight = 0.f;
		for (const float Weight : CurrentBoneWeights)
		{
			MaxCurrentWeight = FMath::Max(MaxCurrentWeight, Weight);
		}

		bBlendPoseRelevant = FAnimWeight::IsRelevant(MaxCurrentWeight);
		if (bBlendPoseRelevant)
		{
			// In Override mode the blend pose takes over root motion in proportion to the
			// root bone weight; in Additive mode root motion is left to the base pose
			if (BlendMode == EACFMaskBlendMode::Override && CurrentBoneWeights.Num() > 0)
			{
				RootMotionWeight = CurrentBoneWeights[0];
			}

			BlendPose.Update(Context.FractionalWeightAndRootMotion(MaxCurrentWeight, RootMotionWeight));
		}
	}

	const float BaseRootMotionWeight = 1.f - RootMotionWeight;
	if (BaseRootMotionWeight < 1.f - ZERO_ANIMWEIGHT_THRESH)
	{
		BasePose.Update(Context.FractionalWeightAndRootMotion(1.f, BaseRootMotionWeight));
	}
	else
	{
		BasePose.Update(Context);
	}
}

void FAnimNode_ACFBlendByMask::Evaluate_AnyThread(FPoseContext& Output)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(Evaluate_AnyThread)

	BasePose.Evaluate(Output);

	const int32 NumBones = Output.Pose.GetNumBones();
	if (!bBlendPoseRelevant || CurrentBoneWeights.Num() != NumBones)
	{
		return;
	}

	// Only a pre-authored additive input expects an additive pose; Override and DynamicAdditive take a normal full pose.
	const bool bPreAuthoredAdditive = (BlendMode == EACFMaskBlendMode::Additive);
	const bool bDynamicAdditive = (BlendMode == EACFMaskBlendMode::DynamicAdditive);

	FPoseContext BlendPoseContext(Output, bPreAuthoredAdditive);
	BlendPose.Evaluate(BlendPoseContext);

	if (bPendingBlendPoseTransition)
	{
		if (CachedBlendPoseTransforms.Num() == NumBones)
		{
			PreviousBlendPoseTransforms = CachedBlendPoseTransforms;
			BlendPoseTransitionAlpha = 0.f;
			BlendPoseTransitionDuration = BlendTime;
		}
		else
		{
			BlendPoseTransitionAlpha = 1.f;
			PreviousBlendPoseTransforms.Reset();
		}
		bPendingBlendPoseTransition = false;
	}

	if (BlendPoseTransitionAlpha < 1.f && PreviousBlendPoseTransforms.Num() == NumBones)
	{
		const float DeltaTime = LastUpdateDeltaTime;
		BlendPoseTransitionAlpha = (BlendPoseTransitionDuration > UE_KINDA_SMALL_NUMBER)
			? FMath::Clamp(BlendPoseTransitionAlpha + DeltaTime / BlendPoseTransitionDuration, 0.f, 1.f)
			: 1.f;

		for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
		{
			const FCompactPoseBoneIndex CompactIndex(BoneIndex);
			FTransform BlendedBone;
			BlendedBone.Blend(PreviousBlendPoseTransforms[BoneIndex], BlendPoseContext.Pose[CompactIndex], BlendPoseTransitionAlpha);
			BlendPoseContext.Pose[CompactIndex] = BlendedBone;
		}
		BlendPoseContext.Pose.NormalizeRotations();
	}

	// DynamicAdditive extracts the blend pose's additive delta relative to a neutral reference pose,
	// chosen via ReferencePoseSource (editor setting, no pin), then layers it on top of the live base.
	FPoseContext RefPoseContext(Output);
	if (bDynamicAdditive)
	{
		switch (ReferencePoseSource)
		{
		case EACFMaskReferencePose::CurrentBasePose:
			RefPoseContext.Pose.CopyBonesFrom(Output.Pose);
			RefPoseContext.Curve.CopyFrom(Output.Curve);
			break;

		case EACFMaskReferencePose::AnimSequenceFirstFrame:
			if (ReferencePoseAnimation)
			{
				FAnimationPoseData RefPoseData(RefPoseContext);
				ReferencePoseAnimation->GetAnimationPose(RefPoseData, FAnimExtractContext(0.0, false));
			}
			else
			{
				RefPoseContext.ResetToRefPose();
			}
			break;

		case EACFMaskReferencePose::SkeletonReferencePose:
		default:
			RefPoseContext.ResetToRefPose();
			break;
		}
	}

	const bool bNeedsMeshSpaceBlend = !bPreAuthoredAdditive && TargetBlendSpaces.Contains(EACFMaskBlendSpace::MeshSpace);

	// Local-space pass: all bones for additive modes, only local-space branches for the others.
	for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
	{
		const float BoneWeight = CurrentBoneWeights[BoneIndex];
		if (BoneWeight <= ZERO_ANIMWEIGHT_THRESH)
		{
			continue;
		}

		// Mesh-space branches are handled in the component-space pass below.
		if (!bPreAuthoredAdditive && TargetBlendSpaces.IsValidIndex(BoneIndex) && TargetBlendSpaces[BoneIndex] == EACFMaskBlendSpace::MeshSpace)
		{
			continue;
		}

		const FCompactPoseBoneIndex CompactIndex(BoneIndex);
		FTransform& OutBone = Output.Pose[CompactIndex];
		const FTransform& LayerBone = BlendPoseContext.Pose[CompactIndex];

		if (bPreAuthoredAdditive)
		{
			FTransform::BlendFromIdentityAndAccumulate(OutBone, LayerBone, ScalarRegister(BoneWeight));
		}
		else if (bDynamicAdditive)
		{
			// Overlay-first composition, mirroring the MM additive overlay graph
			// (ApplyAdditive(Base=Overlay, Delta=Base-Neutral) into a LayeredBoneBlend):
			// the masked bone sits on the authored overlay pose and only inherits the
			// base pose's deviation from the neutral reference, then blends against the
			// live base by the branch alpha. Base-first (base + overlay-delta) kept the
			// full locomotion motion on the masked limbs, so weapon holds drifted with
			// arm swings.
			FTransform BaseDelta = OutBone;
			FAnimationRuntime::ConvertTransformToAdditive(BaseDelta, RefPoseContext.Pose[CompactIndex]);
			FTransform Composited = LayerBone;
			FTransform::BlendFromIdentityAndAccumulate(Composited, BaseDelta, ScalarRegister(1.f));
			Composited.NormalizeRotation();
			OutBone.BlendWith(Composited, BoneWeight);
		}
		else
		{
			OutBone.BlendWith(LayerBone, BoneWeight);
		}
	}

	if (bNeedsMeshSpaceBlend)
	{
		// Mesh-space branches affect rotations only: translations and scales stay in local
		// space so children keep following their parents. All deltas are computed against a
		// snapshot of the *unmodified* base rotations, then converted back to local in one go
		// (like FAnimationRuntime::AccumulateMeshSpaceRotationAdditiveToLocalPose). Reading the
		// base progressively while writing parents would compound the parents' deltas into
		// every child of the same chain (e.g. the spine during aiming).
		const FBoneContainer& BoneContainer = Output.Pose.GetBoneContainer();

		TArray<FQuat> BaseMSRot;
		BaseMSRot.SetNumUninitialized(NumBones);
		TArray<FQuat> BlendMSRot;
		BlendMSRot.SetNumUninitialized(NumBones);
		TArray<FQuat> RefMSRot;
		if (bDynamicAdditive)
		{
			RefMSRot.SetNumUninitialized(NumBones);
		}

		// Compact pose bones are sorted parents-first
		for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
		{
			const FCompactPoseBoneIndex CompactIndex(BoneIndex);
			const int32 ParentIndex = BoneContainer.GetParentBoneIndex(CompactIndex).GetInt();

			BaseMSRot[BoneIndex] = (ParentIndex != INDEX_NONE) ? BaseMSRot[ParentIndex] * Output.Pose[CompactIndex].GetRotation() : Output.Pose[CompactIndex].GetRotation();
			BlendMSRot[BoneIndex] = (ParentIndex != INDEX_NONE) ? BlendMSRot[ParentIndex] * BlendPoseContext.Pose[CompactIndex].GetRotation() : BlendPoseContext.Pose[CompactIndex].GetRotation();
			if (bDynamicAdditive)
			{
				RefMSRot[BoneIndex] = (ParentIndex != INDEX_NONE) ? RefMSRot[ParentIndex] * RefPoseContext.Pose[CompactIndex].GetRotation() : RefPoseContext.Pose[CompactIndex].GetRotation();
			}
		}

		TArray<FQuat> NewMSRot = BaseMSRot;
		TArray<bool> bRotChanged;
		bRotChanged.Init(false, NumBones);

		for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
		{
			const float BoneWeight = CurrentBoneWeights[BoneIndex];
			if (BoneWeight <= ZERO_ANIMWEIGHT_THRESH || !TargetBlendSpaces.IsValidIndex(BoneIndex) || TargetBlendSpaces[BoneIndex] != EACFMaskBlendSpace::MeshSpace)
			{
				continue;
			}

			if (bDynamicAdditive)
			{
				// Overlay-first in mesh space, mirroring ApplyMeshSpaceAdditive(Base=Overlay,
				// Delta=Base-Neutral) + mesh-space LayeredBoneBlend: rotate the overlay pose by
				// the base's mesh-space deviation from neutral, then blend base vs composited
				// by the branch alpha.
				const FQuat BaseDeltaMS = BaseMSRot[BoneIndex] * RefMSRot[BoneIndex].Inverse();
				const FQuat CompositedMS = (BaseDeltaMS * BlendMSRot[BoneIndex]).GetNormalized();
				NewMSRot[BoneIndex] = FQuat::Slerp(BaseMSRot[BoneIndex], CompositedMS, BoneWeight).GetNormalized();

				// Translations stay local (same split as ApplyMeshSpaceAdditive): composite
				// overlay + (base - neutral), lerped by the branch alpha. Scale is left to the
				// base pose.
				const FCompactPoseBoneIndex CompactIndex(BoneIndex);
				FTransform& OutBone = Output.Pose[CompactIndex];
				const FVector CompositedT = BlendPoseContext.Pose[CompactIndex].GetTranslation()
					+ (OutBone.GetTranslation() - RefPoseContext.Pose[CompactIndex].GetTranslation());
				OutBone.SetTranslation(FMath::Lerp(OutBone.GetTranslation(), CompositedT, BoneWeight));
			}
			else
			{
				// Override: rotation blended in mesh space, translation/scale in local space
				// (same split as the engine's mesh-space rotation blend).
				NewMSRot[BoneIndex] = FQuat::Slerp(BaseMSRot[BoneIndex], BlendMSRot[BoneIndex], BoneWeight).GetNormalized();

				const FCompactPoseBoneIndex CompactIndex(BoneIndex);
				FTransform& OutBone = Output.Pose[CompactIndex];
				const FTransform& LayerBone = BlendPoseContext.Pose[CompactIndex];
				OutBone.SetTranslation(FMath::Lerp(OutBone.GetTranslation(), LayerBone.GetTranslation(), BoneWeight));
				OutBone.SetScale3D(FMath::Lerp(OutBone.GetScale3D(), LayerBone.GetScale3D(), BoneWeight));
			}
			bRotChanged[BoneIndex] = true;
		}

		// Convert the modified mesh-space rotations back to local. Unmodified bones whose
		// parent changed are recomputed against the parent's new rotation so they keep their
		// absolute orientation, matching the engine's mesh-space semantics.
		for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
		{
			const FCompactPoseBoneIndex CompactIndex(BoneIndex);
			const int32 ParentIndex = BoneContainer.GetParentBoneIndex(CompactIndex).GetInt();
			const bool bParentChanged = (ParentIndex != INDEX_NONE) && bRotChanged[ParentIndex];
			if (!bRotChanged[BoneIndex] && !bParentChanged)
			{
				continue;
			}

			const FQuat NewLocalRot = (ParentIndex != INDEX_NONE) ? NewMSRot[ParentIndex].Inverse() * NewMSRot[BoneIndex] : NewMSRot[BoneIndex];
			Output.Pose[CompactIndex].SetRotation(NewLocalRot.GetNormalized());
		}
	}

	// Curves
	if (bPreAuthoredAdditive)
	{
		Output.Curve.Accumulate(BlendPoseContext.Curve, MaxCurrentWeight);
	}
	else if (bDynamicAdditive)
	{
		// Same additive semantics as the pose: base + weight * (blend - reference). Keeps the
		// base pose's curves (foot IK, layering flags, ...) alive instead of lerping them
		// towards the overlay pose's usually empty curve set.
		Output.Curve.Accumulate(BlendPoseContext.Curve, MaxCurrentWeight);
		Output.Curve.Accumulate(RefPoseContext.Curve, -MaxCurrentWeight);
	}
	else
	{
		Output.Curve.LerpTo(BlendPoseContext.Curve, MaxCurrentWeight);
	}

	// Attributes
	if (BlendPoseContext.CustomAttributes.ContainsData())
	{
		if (bPreAuthoredAdditive)
		{
			UE::Anim::Attributes::AccumulateAttributes(BlendPoseContext.CustomAttributes, Output.CustomAttributes, MaxCurrentWeight, AAT_LocalSpaceBase);
		}
		else
		{
			UE::Anim::FStackAttributeContainer BaseAttributes;
			BaseAttributes.MoveFrom(Output.CustomAttributes);
			UE::Anim::Attributes::BlendAttributesPerBone(BaseAttributes, BlendPoseContext.CustomAttributes, CurrentBoneWeights, Output.CustomAttributes);
		}
	}

	CachedBlendPoseTransforms.SetNumUninitialized(NumBones);
	for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
	{
		const FCompactPoseBoneIndex CompactIndex(BoneIndex);
		CachedBlendPoseTransforms[BoneIndex] = BlendPoseContext.Pose[CompactIndex];
	}

	Output.Pose.NormalizeRotations();
}

void FAnimNode_ACFBlendByMask::GatherDebugData(FNodeDebugData& DebugData)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(GatherDebugData)

	FString DebugLine = DebugData.GetNodeName(this);
	DebugLine += FString::Printf(TEXT("(Mask: %s, Max Weight: %.2f)"), BlendMask ? *BlendMask->GetName() : TEXT("None"), MaxCurrentWeight);
	DebugData.AddDebugItem(DebugLine);

	BasePose.GatherDebugData(DebugData.BranchFlow(1.f));
	BlendPose.GatherDebugData(DebugData.BranchFlow(MaxCurrentWeight));
}
