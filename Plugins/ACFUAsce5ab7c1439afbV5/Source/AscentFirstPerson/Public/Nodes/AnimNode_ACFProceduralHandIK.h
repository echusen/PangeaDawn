// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BoneControllers/AnimNode_SkeletalControlBase.h"
#include "AnimNode_ACFProceduralHandIK.generated.h"

USTRUCT(BlueprintInternalUseOnly)
struct ASCENTFIRSTPERSON_API FAnimNode_ACFProceduralHandIK : public FAnimNode_SkeletalControlBase
{
	GENERATED_BODY()

	FAnimNode_ACFProceduralHandIK();

	// --- Left Side Config ---
	UPROPERTY(EditAnywhere, Category="Left Hand")
	FBoneReference HandL;

	UPROPERTY(EditAnywhere, Category="Left Hand")
	FBoneReference TargetHandL;

	UPROPERTY(EditAnywhere, Category="Left Hand")
	FBoneReference LowerarmL;

	// --- Right Side Config ---
	UPROPERTY(EditAnywhere, Category="Right Hand")
	FBoneReference HandR;

	UPROPERTY(EditAnywhere, Category="Right Hand")
	FBoneReference TargetHandR;

	UPROPERTY(EditAnywhere, Category="Right Hand")
	FBoneReference LowerarmR;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Alpha", meta=(PinShownByDefault))
	float HandLeftIKAlpha = 1.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Alpha", meta=(PinShownByDefault))
	float HandRightIKAlpha = 1.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Alpha", meta=(PinShownByDefault))
	FName MaskHandLeftIKCurve = TEXT("MaskHandLeftIK");
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Alpha", meta=(PinShownByDefault))
	FName MaskHandRightIKCurve = TEXT("MaskHandRightIK");

public:
	// FAnimNode_SkeletalControlBase Interface
	virtual void EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms) override;
	virtual bool IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones) override;

protected:
	virtual void InitializeBoneReferences(const FBoneContainer& RequiredBones) override;

private:
	void ProcessHandIK(
		FComponentSpacePoseContext& Output,
		TArray<FBoneTransform>& OutBoneTransforms,
		const FBoneReference& HandBone,
		const FBoneReference& TargetBone,
		const FBoneReference& JointBone,
		float InAlpha = 1.0f
	);
};