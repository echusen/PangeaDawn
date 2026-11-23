// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimNodeBase.h"
#include "BoneIndices.h"
#include "BonePose.h"

#include "ACFFirstPersonTypes.generated.h"

UENUM(BlueprintType)
enum class EACFProceduralMotionOutputSpace : uint8
{
	Component   = 0,
	Bone        = 1,
	ParentBone  = 2
};

UENUM(BlueprintType)
enum class EACFProceduralMotionComposition : uint8
{
	Additive		= 0, // apply as delta
	LerpToTarget	= 1, // pose = Lerp/Slerp(pose, target, Weight)
	Custom0			= 2, // Non used
	Custom1			= 3, // Non used
	Custom2			= 4, // Non used
	Custom3			= 5, // Non used
	Custom4			= 6, // Non used
	Custom5			= 7  // Non used
};

struct FACFProceduralMotionStageOp
{
	FTransform Transform;
	float Weight = 1.f;
	EACFProceduralMotionOutputSpace Space = EACFProceduralMotionOutputSpace::Component;
	EACFProceduralMotionComposition Comp = EACFProceduralMotionComposition::Additive;
	uint8 bMaskLocation : 1 { false };
	uint8 bMaskRotation : 1 { false };
};

USTRUCT(BlueprintType)
struct FACFProceduralMotionInput
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Procedural")
	TArray<TObjectPtr<AActor>> SourceActors;

	/**
	 * Also harvest components from actors attached to the anim instance's owner, recursively.
	 *
	 * Off by default, and only safe when *everything* attached is meant to be driving motion. Inventories
	 * that keep stowed equipment attached-but-hidden — the usual pattern, and what the demo does — will
	 * harvest every weapon in the loadout at once and apply the whole stack once per weapon.
	 *
	 * Attachment does not mean "in use", and the node has no generic way to tell the difference. Use
	 * SourceActors to say which actors are actually live.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Procedural")
	bool bHarvestAttachedActors = false;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Procedural")
	FVector PivotOffset = FVector::ZeroVector;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Procedural")
	bool bFirstPerson = false;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Procedural")
	float AimingState = 0.0f;
	
	TArray<TArray<FACFProceduralMotionStageOp>> CustomStageOps;

	bool HasValidCustomStageOps() const
	{
		for (const TArray<FACFProceduralMotionStageOp>& CustomStageOp : CustomStageOps)
		{
			if (CustomStageOp.Num() > 0)
			{
				return true;
			}
		}

		return false;
	}
};

/**
 * Evaluation-time context for UACFProceduralMotionComponent::ComputePoseOutputs.
 *
 * Animation worker thread. Pose reads only, no UObject mutation.
 */
struct FACFProceduralMotionPoseContext
{
	FACFProceduralMotionPoseContext(FComponentSpacePoseContext& InPoseContext, const FACFProceduralMotionInput& InInput, FCompactPoseBoneIndex InBoneIndex, FCompactPoseBoneIndex InParentBoneIndex)
		: PoseContext(InPoseContext)
		, Input(InInput)
		, BoneIndex(InBoneIndex)
		, ParentBoneIndex(InParentBoneIndex)
	{
	}

	FTransform GetComponentSpaceTransform(FCompactPoseBoneIndex Index) const
	{
		return PoseContext.Pose.GetComponentSpaceTransform(Index);
	}

	/** BoneIndex relative to ParentBoneIndex in the live pose. Only valid when ParentBoneIndex is set. */
	FTransform GetBoneLocalTransform() const
	{
		return GetComponentSpaceTransform(BoneIndex).GetRelativeTransform(GetComponentSpaceTransform(ParentBoneIndex));
	}

	FComponentSpacePoseContext& PoseContext;

	const FACFProceduralMotionInput& Input;

	/** Bone the stack composes onto. */
	FCompactPoseBoneIndex BoneIndex;

	/** Parent of BoneIndex. INDEX_NONE if the bone is a root. */
	FCompactPoseBoneIndex ParentBoneIndex;

	/**
	 * BoneIndex relative to its parent in the reference pose. Only meaningful while bHasReferencePose.
	 *
	 * Carries the node's reference offsets, which mirror Bone Rebase's pivot offsets, so differencing this
	 * against GetBoneLocalTransform is valid.
	 */
	FTransform ReferenceBoneLocal = FTransform::Identity;

	/** True when the node's Reference Pose pin is connected and resolved this frame. */
	bool bHasReferencePose = false;
};

USTRUCT(BlueprintType)
struct FACFProceduralMotionOutput
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Procedural")
	FTransform Transform = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Procedural")
	float Weight = 1.f;

	/** My usual setup:
	* PreOffset	= 0
	* Offset	= 1
	* ADS		= 2
	* Additive	= 3
	* Sway		= 4
	* Recoil	= 5
	* Post		= 6
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Procedural")
	int32 Stage = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Procedural")
	EACFProceduralMotionOutputSpace Space = EACFProceduralMotionOutputSpace::Component;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Procedural")
	EACFProceduralMotionComposition Composition = EACFProceduralMotionComposition::Additive;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Procedural", meta=(AdvancedDisplay, HiddenByDefault))
	uint8 bMaskLocation : 1 { false };
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Procedural", meta=(AdvancedDisplay, HiddenByDefault))
	uint8 bMaskRotation : 1 { false };
};

USTRUCT(BlueprintType)
struct FACFProceduralMontagePair
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Defaults)
	TObjectPtr<UAnimMontage> MontageFP;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Defaults)
	TObjectPtr<UAnimMontage> MontageTP;

	float GetPlayLength() const
	{
		if (MontageFP) return MontageFP->GetPlayLength();

		if (MontageTP) return MontageTP->GetPlayLength();

		return 0.0f;
	}

	bool IsValidOnlyFP() const
	{
		return ::IsValid(MontageFP);
	}

	bool IsValidOnlyTP() const
	{
		return ::IsValid(MontageTP);
	}

	bool IsValid() const
	{
		return IsValidOnlyFP() && IsValidOnlyTP();
	}
};

USTRUCT(BlueprintType)
struct FACFProceduralEquipmentMontage
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Defaults)
	FACFProceduralMontagePair CharacterMontage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Defaults)
	FACFProceduralMontagePair EquipmentMontage;

	float GetPlayLength() const
	{
		const float CharacterMontageDuration = CharacterMontage.GetPlayLength();

		if (CharacterMontageDuration > 0.0f) return CharacterMontageDuration;

		return EquipmentMontage.GetPlayLength();
	}

	bool IsValidOnlyFP() const
	{
		return CharacterMontage.IsValidOnlyFP();
	}

	bool IsValidOnlyTP() const
	{
		return CharacterMontage.IsValidOnlyTP();
	}

	bool IsValid() const
	{
		return CharacterMontage.IsValid();
	}
};

USTRUCT(BlueprintType)
struct FACFProceduralHandOffsetData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Defaults)
	FVector HandLeftOffsetLocation = FVector::ZeroVector;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Defaults)
	FRotator HandLeftOffsetRotation = FRotator::ZeroRotator;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Defaults)
	FVector HandRightOffsetLocation = FVector::ZeroVector;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Defaults)
	FRotator HandRightOffsetRotation = FRotator::ZeroRotator;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Defaults)
	FVector LowerarmLeftOffsetLocation = FVector::ZeroVector;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Defaults)
	FRotator LowerarmLeftOffsetRotation = FRotator::ZeroRotator;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Defaults)
	FVector LowerarmRightOffsetLocation = FVector::ZeroVector;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Defaults)
	FRotator LowerarmRightOffsetRotation = FRotator::ZeroRotator;

	/** Component space, like the other offsets. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Defaults)
	FVector ClavicleLeftOffsetLocation = FVector::ZeroVector;

	/** Component space, like the other offsets. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Defaults)
	FVector ClavicleRightOffsetLocation = FVector::ZeroVector;

};
