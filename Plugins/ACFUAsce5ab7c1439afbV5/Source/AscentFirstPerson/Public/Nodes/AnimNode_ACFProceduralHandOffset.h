// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BoneControllers/AnimNode_SkeletalControlBase.h"
#include "Types/ACFFirstPersonTypes.h"
#include "AnimNode_ACFProceduralHandOffset.generated.h"

USTRUCT(BlueprintInternalUseOnly)
struct ASCENTFIRSTPERSON_API FAnimNode_ACFProceduralHandOffset : public FAnimNode_SkeletalControlBase
{
	GENERATED_BODY()

	FAnimNode_ACFProceduralHandOffset();

	UPROPERTY(EditAnywhere, Category="Hand Offset")
	FBoneReference HandLeftBone;

	UPROPERTY(EditAnywhere, Category="Hand Offset")
	FBoneReference HandRightBone;

	UPROPERTY(EditAnywhere, Category="Hand Offset")
	FBoneReference LowerarmLeftBone;

	UPROPERTY(EditAnywhere, Category="Hand Offset")
	FBoneReference LowerarmRightBone;

	/** Optional, unlike the bones above: unset leaves the left arm's clavicle alone. */
	UPROPERTY(EditAnywhere, Category="Hand Offset")
	FBoneReference ClavicleLeftBone;

	/** Optional, unlike the bones above: unset leaves the right arm's clavicle alone. */
	UPROPERTY(EditAnywhere, Category="Hand Offset")
	FBoneReference ClavicleRightBone;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hand Offset", meta=(PinShownByDefault))
	FACFProceduralHandOffsetData HandOffsetData;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hand Alpha", meta=(PinShownByDefault))
	float HandOffsetAlpha = 1.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hand Alpha", meta=(PinShownByDefault))
	float LowerarmOffsetAlpha = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hand Alpha", meta=(PinShownByDefault))
	float ClavicleOffsetAlpha = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hand Alpha", meta=(PinShownByDefault))
	FName MaskHandOffsetCurve = TEXT("MaskHandOffset");
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hand Alpha", meta=(PinShownByDefault))
	FName MaskLowerarmOffsetCurve = TEXT("MaskLowerarmOffset");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hand Alpha", meta=(PinShownByDefault))
	FName MaskClavicleOffsetCurve = TEXT("MaskClavicleOffset");

public:
	// FAnimNode_SkeletalControlBase Interface
	virtual void EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms) override;
	virtual bool IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones) override;

protected:
	virtual void InitializeBoneReferences(const FBoneContainer& RequiredBones) override;

private:
	// Helper to reduce code duplication
	void ProcessBone(FBoneReference& BoneRef, FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms, const FVector& LocationOffset, const FRotator& RotationOffset, float InAlpha = 1.0f);
};