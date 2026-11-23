// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BoneControllers/AnimNode_SkeletalControlBase.h"
#include "AnimNode_ACFProceduralBoneRebase.generated.h"

USTRUCT(BlueprintType)
struct ASCENTFIRSTPERSON_API FACFBonePair
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category=Defaults)
	FBoneReference SourceBone;

	UPROPERTY(EditAnywhere, Category=Defaults)
	FBoneReference TargetBone;

	void Initialize(const FBoneContainer& RequiredBones);

	/** Both ends resolve. What an ordinary copy needs. */
	bool IsValidToEvaluate(const FBoneContainer& RequiredBones) const;

	/** The destination resolves. Enough on its own when the source is optional. */
	bool IsTargetValidToEvaluate(const FBoneContainer& RequiredBones) const;
};

USTRUCT(BlueprintInternalUseOnly)
struct ASCENTFIRSTPERSON_API FAnimNode_ACFProceduralBoneRebase : public FAnimNode_SkeletalControlBase
{
	GENERATED_BODY()

	FAnimNode_ACFProceduralBoneRebase();

	/**
	 * The weapon holder, and optionally a bone to copy onto it.
	 *
	 * `TargetBone` is the holder itself and is what the rest of the stack composes onto.
	 *
	 * `SourceBone` is **optional**. Set it to rebase the holder onto a bone that is reliably animated —
	 * usually a virtual bone hanging off the hand, such as `VB hand_r_to_ik_hand_gun`, which sits under the
	 * spine hierarchy and so picks up additive layers and upper-body masks. Copying `ik_hand_gun` directly
	 * is not always safe: whether it carries those layers depends on the graph.
	 *
	 * Leave `SourceBone` unset to leave the holder wherever the incoming animation put it and apply only
	 * the offsets below. That is the right choice when the holder is already a true virtual bone whose
	 * parent chain does the work.
	 */
	UPROPERTY(EditAnywhere, Category="Bones")
	FACFBonePair PivotPair;

	/**
	 * Replaces PivotPair's SourceBone while it names a bone the skeleton has, so the holder can follow a different
	 * hand per item. Bind it to the ACF anim instance's GetItemReferenceBone: the first person component measures the
	 * held item's points from that same bone, so the holder and the points stay in agreement. None, or a bone the
	 * skeleton lacks, keeps SourceBone, which then acts as the default.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bones", meta=(PinHiddenByDefault))
	FName PivotSourceBoneOverride;

	/** Straight copies. Both ends are required; a pair that cannot resolve is skipped, not fatal. */
	UPROPERTY(EditAnywhere, Category="Bones")
	TArray<FACFBonePair> BonePairs;

	/** Applied to the holder whether or not it was copied from a source. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Offset", meta=(PinShownByDefault))
	FVector PivotOffsetLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Offset", meta=(PinShownByDefault))
	FRotator PivotOffsetRotation = FRotator::ZeroRotator;

public:
	// FAnimNode_SkeletalControlBase Interface
	virtual void EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms) override;
	virtual bool IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones) override;

protected:
	virtual void InitializeBoneReferences(const FBoneContainer& RequiredBones) override;

private:
	/** bSourceOptional writes the target from its own current transform when no source resolves. */
	void ProcessBonePair(FACFBonePair& Pair, FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms, bool bSourceOptional = false, const FVector* LocationOffset = nullptr, const FRotator* RotationOffset = nullptr);

	/** PivotSourceBoneOverride resolved against the required bones. Re-resolved when the name or the bones change. */
	FBoneReference ResolvedPivotSourceOverride;
};