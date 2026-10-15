// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/ACFCameraAimRig.h"
#include "BoneControllers/AnimNode_SkeletalControlBase.h"

#include "AnimNode_ACFCameraAim.generated.h"

/** One resolved chain bone, rebuilt whenever the rig or the bone container changes. */
struct FACFResolvedAimChainBone {
    FCompactPoseBoneIndex BoneIndex = FCompactPoseBoneIndex(INDEX_NONE);

    /** Fraction of the total rotation accumulated from the root of the chain down to this bone. */
    float CumulativePitch = 0.f;
    float CumulativeYaw = 0.f;
    float CumulativeRoll = 0.f;
};

/**
 * Orients the upper body onto the view rotation analytically, with no aim offset blendspace and no
 * additive pose involved. It follows the rotation it is given, normally the control rotation, and
 * knows nothing about which camera is rendering. Three stages, each of which can be switched off:
 *
 *  - Aim Chain: the delta between the view rotation and the actor rotation is spread over the spine
 *    with normalized per axis weights. The rotation accumulated at the tip of the chain is exactly
 *    the full delta - the increments are composed, not summed, so it stays exact at any pitch.
 *  - Socket Lock: the lock bone is solved so the aim socket ends up pointing along the view, offset
 *    by SocketRotationOffset. Everything below the lock bone (arms, hands, weapon) follows rigidly,
 *    which is what keeps the hold steady whatever the locomotion does. Bones are never displaced:
 *    a rotation carries no position, so only orientation is solved.
 *  - Anchor Stabilization: the view anchor bone is turned onto the view rotation, so animated head
 *    rotation does not leak into a camera riding that bone.
 *
 * The node is driven by the bEnabled bool pin from the Alpha category, so it can be switched on and
 * off at runtime and costs a virtual call when off.
 */
USTRUCT(BlueprintInternalUseOnly)
struct CHARACTERCONTROLLER_API FAnimNode_ACFCameraAim : public FAnimNode_SkeletalControlBase {
    GENERATED_BODY()

public:
    /** Bones to drive and where they have to end up. Pin, so it can be swapped per weapon at runtime. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ACF, meta = (PinShownByDefault))
    TObjectPtr<UACFCameraAimRig> Rig;

    /**
     * World space rotation the whole node solves against, normally the control rotation. Wire Get
     * View Rotation from the camera aim component, which samples it on the Game Thread before the
     * mesh ticks. Left unconnected, the node reads the same value off the mesh's UACFAnimInstance.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ACF, meta = (PinShownByDefault))
    FRotator ViewRotation = FRotator::ZeroRotator;

    // ---- Aim Chain ---- //

    UPROPERTY(EditAnywhere, Category = "ACF|Aim Chain")
    bool bEnableAimChain = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF|Aim Chain", meta = (ClampMin = 0.0, ClampMax = 1.0, PinHiddenByDefault))
    float AimChainAlpha = 1.f;

    /** Min and max pitch, in degrees, the chain is allowed to take from the view frame. */
    UPROPERTY(EditAnywhere, Category = "ACF|Aim Chain")
    FVector2D PitchLimits = FVector2D(-70.f, 70.f);

    /** Min and max yaw, in degrees, the chain is allowed to take from the view frame. */
    UPROPERTY(EditAnywhere, Category = "ACF|Aim Chain")
    FVector2D YawLimits = FVector2D(-70.f, 70.f);

    /**
     * 0 = the chain follows the view exactly, which is what an eye height camera needs: any smoothing
     * here makes the weapon swim behind the crosshair. A small value is fine over the shoulder.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF|Aim Chain", meta = (ClampMin = 0.0, PinHiddenByDefault))
    float AimInterpSpeed = 0.f;

    // ---- Socket Lock ---- //

    UPROPERTY(EditAnywhere, Category = "ACF|Socket Lock")
    bool bEnableSocketLock = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF|Socket Lock", meta = (ClampMin = 0.0, ClampMax = 1.0, PinHiddenByDefault))
    float SocketLockAlpha = 1.f;

    // ---- Anchor Stabilization ---- //

    UPROPERTY(EditAnywhere, Category = "ACF|Anchor Stabilization")
    bool bEnableAnchorStabilization = false;

    /** 1 = the anchor takes the view orientation outright, killing every bit of animated head rotation. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF|Anchor Stabilization", meta = (ClampMin = 0.0, ClampMax = 1.0, PinHiddenByDefault))
    float AnchorRotationAlpha = 1.f;

public:
    FAnimNode_ACFCameraAim();

    // FAnimNode_Base interface
    virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
    virtual void GatherDebugData(FNodeDebugData& DebugData) override;
    // End of FAnimNode_Base interface

    // FAnimNode_SkeletalControlBase interface
    virtual void UpdateInternal(const FAnimationUpdateContext& Context) override;
    virtual void EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms) override;
    virtual bool IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones) override;
    virtual void InitializeBoneReferences(const FBoneContainer& RequiredBones) override;
    // End of FAnimNode_SkeletalControlBase interface

private:
    /**
     * Copies the bone references out of the rig into the node. The rig is a shared asset and
     * FBoneReference/FSocketReference cache indices when initialized, so initializing the asset's
     * own copies would have every character using that rig fight over them at different LODs.
     */
    void CopyReferencesFromRig(const FAnimInstanceProxy* inProxy);

    /** View rotation to solve against: the pin when it holds something, the anim instance otherwise. */
    bool ResolveViewRotation(const FComponentSpacePoseContext& Output, FRotator& outViewWorld) const;

    /**
     * Component space transform a bone ends up with once the stages that already ran are applied.
     * Bones under the last modified one follow it; anything else is still the incoming pose.
     */
    FTransform GetTransformAfterPreviousStages(const FTransform& inBoneOriginalCS) const;

    /** Adds the transform, or replaces the one already written for that bone. */
    static void AddOrReplaceBoneTransform(TArray<FBoneTransform>& outBoneTransforms, const FCompactPoseBoneIndex& inBoneIndex, const FTransform& inTransform);

    /** Chain resolved for the current rig and LOD, sorted root to tip with cumulative weights. */
    TArray<FACFResolvedAimChainBone> CachedChain;

    /** Node owned copies of the rig's references, see CopyReferencesFromRig. */
    FBoneSocketTarget resolvedAimSocket;
    FBoneReference resolvedLockBone;
    FBoneReference resolvedAnchorBone;

    /** Deepest bone modified so far this evaluation, used to propagate to its descendants. */
    FTransform lastModifiedOriginalCS = FTransform::Identity;
    FTransform lastModifiedNewCS = FTransform::Identity;
    bool bHasModifiedBone = false;

    /** Smoothed view delta, only used when AimInterpSpeed is above 0. */
    FRotator smoothedAimDelta = FRotator::ZeroRotator;
    bool bResetInterpolation = true;

    TWeakObjectPtr<UACFCameraAimRig> cachedRig;
    uint16 cachedBonesSerialNumber = 0;
    float lastUpdateDeltaTime = 0.f;

    /** Anim instance update counter seen last time the node ran, to detect it was skipped while off. */
    int16 lastUpdateCounter = INDEX_NONE;
};
