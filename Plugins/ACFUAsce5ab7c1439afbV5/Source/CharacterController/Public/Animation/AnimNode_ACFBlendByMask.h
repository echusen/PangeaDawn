// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNodeBase.h"
#include "Animation/ACFBoneBlendMask.h"
#include "AnimNode_ACFBlendByMask.generated.h"

class UACFBoneBlendMask;
class UAnimSequence;
class UAnimSequenceBase;

UENUM(BlueprintType)
enum class EACFMaskBlendMode : uint8
{
	/** Lerps between the base pose and the blend pose per-bone, using the branch alphas.
	 * At partial alphas the limbs are dragged towards the blend pose's absolute orientation, which can look stiff. */
	Override,
	/** The blend pose is treated as an already-authored local space additive delta, accumulated on top of the base pose scaled by the branch alphas. */
	Additive,
	/** The blend pose is a normal full pose composed overlay-first (like the MM additive overlay graph):
	 * the base pose's additive delta relative to the neutral reference (ReferencePoseSource) is layered on top
	 * of the blend pose (ApplyAdditive(Base=Overlay, Delta=Base-Neutral)), and the result is blended against the
	 * live base by the branch alphas. Masked parts sit on the authored overlay pose and only inherit the base's
	 * deviation from neutral (breathing, lean), instead of keeping the full locomotion motion. */
	DynamicAdditive
};

/** Neutral pose DynamicAdditive subtracts from the blend pose to build the additive delta. Editor-only setting (no pin). */
UENUM(BlueprintType)
enum class EACFMaskReferencePose : uint8
{
	/** Skeleton reference pose (bind / T-pose). The base pose's deviation is measured from bind. */
	SkeletonReferencePose UMETA(DisplayName = "T-Pose (Skeleton Ref)"),
	/** The live base pose. Base - Base = identity, so this degenerates into a plain override of the blend pose. */
	CurrentBasePose UMETA(DisplayName = "Current Anim Pose"),
	/** First frame of a chosen animation (e.g. a relaxed neutral stand), used as the neutral reference the
	 * base pose's deviation is measured from. This matches the NeutralPose evaluator of the MM overlay graph. */
	AnimSequenceFirstFrame UMETA(DisplayName = "Anim Sequence (First Frame)")
};

/**
 * Blends a pose on top of a base pose with per-body-part alphas defined in a
 * UACFBoneBlendMask data asset (branch points; deeper branch points have priority
 * over parent ones). The mask is exposed as a pin and can be swapped at runtime:
 * the per-bone weights (and pose cross-fade on mask/anim swap) are interpolated over BlendTime
 * instead of snapping. OverlayBlendAlpha (default 1) scales all mask weights globally - same pin
 * pattern as BlendTime; leave unconnected for full authored mask weights in any AnimBP.
 */
USTRUCT(BlueprintInternalUseOnly)
struct CHARACTERCONTROLLER_API FAnimNode_ACFBlendByMask : public FAnimNode_Base
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Links)
	FPoseLink BasePose;

	/** Pose blended on top of the base pose using the branch alphas from the blend mask. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Links)
	FPoseLink BlendPose;

	/**
	 * Branch points (bone + alpha) defining which body parts are affected by the blend pose and how much.
	 * Can change at runtime: weights are smoothly blended towards the new values over BlendTime.
	 * When None, the blend pose is smoothly blended out.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Config, meta = (PinShownByDefault))
	TObjectPtr<UACFBoneBlendMask> BlendMask;

	/** Time used to interpolate per-bone weights and pose cross-fade when the mask/animation changes. 0 = instant. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Config, meta = (ClampMin = 0.0, PinShownByDefault))
	float BlendTime = 0.3f;

	/**
	 * Global scale on top of every per-bone mask weight. Default 1 = full mask when unconnected
	 * (safe in any AnimBP). On overlay layers, wire OverlayBlendAlfa (from entry BlendAlpha).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Config, meta = (ClampMin = 0.0, ClampMax = 1.0, PinShownByDefault))
	float OverlayBlendAlpha = 1.f;

	/**
	 * Optional: the animation currently driving BlendPose upstream (e.g. the overlay anim variable
	 * feeding the sequence player/evaluator connected to BlendPose). Not evaluated by this node -
	 * only compared frame to frame so an animation swap also triggers a BlendTime cross-fade, not
	 * just a mask swap.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Config, meta = (PinShownByDefault))
	TObjectPtr<UAnimSequenceBase> CurrentAnimation;

	/** How the blend pose is applied on top of the base pose. Driven by the overlay data via the
	 * generic layer's CurrentBlendMode: connect it to this pin. Defaults to DynamicAdditive when
	 * left unconnected. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Config, meta = (PinShownByDefault))
	EACFMaskBlendMode BlendMode = EACFMaskBlendMode::DynamicAdditive;

	/** Neutral pose used by DynamicAdditive to build the additive delta (DynamicAdditive only). Editor setting, no pin. */
	UPROPERTY(EditAnywhere, Category = Config)
	EACFMaskReferencePose ReferencePoseSource = EACFMaskReferencePose::SkeletonReferencePose;

	/** Animation whose first frame is used as the neutral reference when ReferencePoseSource is AnimSequenceFirstFrame. */
	UPROPERTY(EditAnywhere, Category = Config, meta = (EditCondition = "ReferencePoseSource == EACFMaskReferencePose::AnimSequenceFirstFrame", EditConditionHides))
	TObjectPtr<UAnimSequence> ReferencePoseAnimation;

	/**
	 * Cross-fades the blend pose itself over BlendTime when BlendMask or CurrentAnimation changes. Turn it off when
	 * the BlendPose input already blends its own swaps, e.g. a sequence set with inertial blending behind a Dead
	 * Blending or Inertialization node: both together compound into a slow start and a late catch-up. The per-bone
	 * mask weights blend over BlendTime either way. Editor setting, no pin.
	 */
	UPROPERTY(EditAnywhere, Category = Config)
	bool bCrossFadeBlendPose = true;

	/**
	 * Max LOD that this node is allowed to run.
	 * For example if you have LODThreshold to be 2, it will run until LOD 2 (based on 0 index)
	 * when the component LOD becomes 3, it will stop update/evaluate.
	 */
	UPROPERTY(EditAnywhere, Category = Performance, meta = (DisplayName = "LOD Threshold"))
	int32 LODThreshold = INDEX_NONE;

private:
	// Weights resolved from the current blend mask, per compact pose bone index
	TArray<float> TargetBoneWeights;

	// Blend spaces resolved from the current blend mask, per compact pose bone index
	TArray<EACFMaskBlendSpace> TargetBlendSpaces;

	// Weights actually used for blending, interpolated towards TargetBoneWeights
	TArray<float> CurrentBoneWeights;

	// Highest current weight, used for relevancy, curves and debug
	float MaxCurrentWeight = 0.f;

	uint16 RequiredBonesSerialNumber = 0;

	bool bBlendPoseRelevant = false;

	// When true, current weights snap to their target on the next update (set on initialization and LOD changes)
	bool bSnapWeights = true;

	// BlendMask/CurrentAnimation seen on the previous update, to detect a swap
	const UACFBoneBlendMask* LastBlendMask = nullptr;
	const UAnimSequenceBase* LastAnimation = nullptr;

	// Cached BlendPose actually used last evaluation. When BlendMask/CurrentAnimation changes, the
	// new BlendPose is cross-faded from this cached pose over BlendTime so rotations do not snap
	// even when the per-bone mask weights are unchanged (e.g. Aim -> Idle with the same upper-body weight).
	TArray<FTransform> CachedBlendPoseTransforms;
	TArray<FTransform> PreviousBlendPoseTransforms;
	float BlendPoseTransitionAlpha = 1.f;
	float BlendPoseTransitionDuration = 0.f;
	float LastUpdateDeltaTime = 0.f;
	bool bPendingBlendPoseTransition = false;

public:
	FAnimNode_ACFBlendByMask() = default;

	// FAnimNode_Base interface
	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override;
	virtual void Update_AnyThread(const FAnimationUpdateContext& Context) override;
	virtual void Evaluate_AnyThread(FPoseContext& Output) override;
	virtual void GatherDebugData(FNodeDebugData& DebugData) override;
	virtual int32 GetLODThreshold() const override { return LODThreshold; }
	// End of FAnimNode_Base interface

private:
	// Resolves TargetBoneWeights from the current blend mask
	void RebuildTargetBoneWeights(const FBoneContainer& RequiredBones);
};
