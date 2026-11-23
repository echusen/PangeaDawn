// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BoneControllers/AnimNode_SkeletalControlBase.h"
#include "Types/ACFFirstPersonTypes.h"
#include "GameFramework/Actor.h"
#include "AnimNode_ACFProceduralMotion.generated.h"

class UACFProceduralMotionComponent;

/**
 * Composes the procedural motion stack onto a single bone.
 *
 * Harvests UACFProceduralMotionComponent ops from the owning actor and Input.SourceActors during PreUpdate
 * (game thread), then applies them stage by stage during evaluation (worker thread).
 */
USTRUCT(BlueprintInternalUseOnly)
struct ASCENTFIRSTPERSON_API FAnimNode_ACFProceduralMotion : public FAnimNode_SkeletalControlBase
{
	GENERATED_BODY()

public:
	FAnimNode_ACFProceduralMotion() = default;

	/** The weapon holder bone. Usually a virtual bone parented under head. */
	UPROPERTY(EditAnywhere, Category="SkeletalControl")
	FBoneReference BoneToModify;


	UPROPERTY(EditAnywhere, Category="Procedural", meta=(PinShownByDefault))
	FACFProceduralMotionInput Input;

	/**
	 * Mirror of Bone Rebase's PivotOffsetLocation on the pivot pair.
	 *
	 * The reference branch is tapped upstream of Bone Rebase, so it does not carry that offset. Feeding the
	 * same value here keeps both branches in step; without it the offset leaks into the bleed-through delta
	 * as a constant error. Wire both pins from the same per-equipment source. Leave zero if the rebase has
	 * no pivot offset.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Procedural", meta=(PinShownByDefault))
	FVector ReferenceOffsetLocation = FVector::ZeroVector;

	/** Mirror of Bone Rebase's PivotOffsetRotation on the pivot pair. See ReferenceOffsetLocation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Procedural", meta=(PinShownByDefault))
	FRotator ReferenceOffsetRotation = FRotator::ZeroRotator;

	/**
	 * Optional. The equipment's base pose, tapped before any montage slot or locomotion additive.
	 *
	 * Components that read it get the delta the live animation is contributing on top of it — a reload
	 * montage moves the weapon while aiming, an idle pose does not. Leave unconnected to disable that.
	 * Only evaluated when a component reports NeedsReferencePose().
	 *
	 * Local space: BoneToModify is read straight out of the pose, where it is already parent-relative.
	 * If the live branch's Bone Rebase applies pivot offsets, this branch needs the same ones or the
	 * delta carries a constant error.
	 */
	UPROPERTY(EditAnywhere, Category="Procedural")
	FPoseLink ReferencePose;

public:
	//~ Begin FAnimNode_SkeletalControlBase
	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override;
	virtual void UpdateInternal(const FAnimationUpdateContext& Context) override;
	virtual bool HasPreUpdate() const override { return true; }
	virtual void PreUpdate(const UAnimInstance* InAnimInstance) override;
	virtual void EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms) override;
	virtual bool IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones) override;
	virtual void InitializeBoneReferences(const FBoneContainer& RequiredBones) override;
	//~ End FAnimNode_SkeletalControlBase

private:
	bool IsReferencePoseLinked() const { return ReferencePose.LinkID != INDEX_NONE; }

	void ResetStageOps();
	void CopyCustomStageOps();
	void AddStageOp(const FACFProceduralMotionOutput& Out);
	void ResolveReferencePose(FACFProceduralMotionPoseContext& PoseContext);
	void ApplyStageOp(FTransform& InOutBoneCS, const FACFProceduralMotionStageOp& Op, FComponentSpacePoseContext& Output, FCompactPoseBoneIndex BoneIndex) const;

	/** Ops to apply, indexed by stage. Inner arrays are emptied but never shrunk, to reuse allocations. */
	TArray<TArray<FACFProceduralMotionStageOp>> StageOps;

	/** Every component harvested this frame. Written in PreUpdate, read during evaluation. */
	TArray<TWeakObjectPtr<UACFProceduralMotionComponent>> EvaluationComps;

	/** Per-frame scratch, kept as members to avoid reallocating each update. */
	TSet<AActor*> ActorsToProcess;
	TArray<AActor*> AttachedActors;
	TInlineComponentArray<UACFProceduralMotionComponent*, 16> ActorComps;
};
