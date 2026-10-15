// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BoneControllers/AnimNode_SkeletalControlBase.h"
#include "AnimNode_ACFProceduralAimOffset.generated.h"

USTRUCT(BlueprintType)
struct ASCENTFIRSTPERSON_API FACFBoneParams
{
	GENERATED_BODY()

	FACFBoneParams() {}
	FACFBoneParams(const FBoneReference& InBone, float InWeight)
		: Bone(InBone)
		, Weight(InWeight)
	{
	}

	UPROPERTY(EditAnywhere, Category=Defaults)
	FBoneReference Bone;

	UPROPERTY(EditAnywhere, Category=Defaults)
	float Weight = 1.f;
};

USTRUCT(BlueprintInternalUseOnly)
struct ASCENTFIRSTPERSON_API FAnimNode_ACFProceduralAimOffset : public FAnimNode_SkeletalControlBase
{
	GENERATED_BODY()

	FAnimNode_ACFProceduralAimOffset();

	UPROPERTY(EditAnywhere, Category="Settings", meta=(DisplayName="Auto Fill Rotation from Camera"))
	uint8 bAutoFillRotationFromCamera : 1;

	UPROPERTY(EditAnywhere, Category="Settings", meta=(EditCondition="!bAutoFillRotationFromCamera", DisplayName="Auto Fill Rotation from Leaning (Component)"))
	uint8 bAutoFillRotationFromLeaning : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings", meta=(PinHiddenByDefault, EditCondition="!bAutoFillRotationFromCamera && !bAutoFillRotationFromLeaning", DisplayName="Aim Rotation"))
	FRotator CameraRelativeRotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, Category="Settings", meta=(DisplayName="Spine To Head Bone Names"))
	TArray<FACFBoneParams> SpineBoneParams;

	// Cached data
	FRotator AutoCameraRelativeRotation = FRotator::ZeroRotator;

public:
	// FAnimNode_Base Interface
	virtual bool HasPreUpdate() const override { return true; }
	virtual void PreUpdate(const UAnimInstance* InAnimInstance) override;

	// FAnimNode_SkeletalControlBase Interface
	virtual void EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms) override;
	virtual bool IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones) override;

protected:
	virtual void InitializeBoneReferences(const FBoneContainer& RequiredBones) override;

private:
	void RefreshAutoCameraRelativeRotation(const UAnimInstance* InAnimInstance);
};