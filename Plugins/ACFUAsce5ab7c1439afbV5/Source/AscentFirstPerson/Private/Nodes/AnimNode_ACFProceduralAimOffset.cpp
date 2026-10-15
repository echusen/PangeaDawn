// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "Nodes/AnimNode_ACFProceduralAimOffset.h"

#include "Animation/AnimInstanceProxy.h"
#include "Kismet/KismetMathLibrary.h"
#include "GameFramework/PlayerController.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Pawn.h"

#include "Utility/ACFFirstPersonFunctionLibrary.h"
#include "Components/ACFProceduralLeaningComponent.h"

FAnimNode_ACFProceduralAimOffset::FAnimNode_ACFProceduralAimOffset()
{
	bAutoFillRotationFromCamera = true;
	bAutoFillRotationFromLeaning = false;

	AlphaInputType = EAnimAlphaInputType::Float;

	SpineBoneParams =
	{
		FACFBoneParams(FBoneReference(FName("spine_01")), 1.f / 3.f),
		FACFBoneParams(FBoneReference(FName("spine_02")), 1.f / 3.f),
		FACFBoneParams(FBoneReference(FName("spine_03")), 1.f / 3.f)
	};
}

void FAnimNode_ACFProceduralAimOffset::InitializeBoneReferences(const FBoneContainer& RequiredBones)
{
	for (FACFBoneParams& BoneParams : SpineBoneParams)
	{
		BoneParams.Bone.Initialize(RequiredBones);
	}
}

bool FAnimNode_ACFProceduralAimOffset::IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones)
{
	if (SpineBoneParams.Num() == 0) return false;

	for (const FACFBoneParams& BoneParams : SpineBoneParams)
	{
		if (!BoneParams.Bone.IsValidToEvaluate(RequiredBones)) return false;
	}

	return true;
}

void FAnimNode_ACFProceduralAimOffset::PreUpdate(const UAnimInstance* InAnimInstance)
{
	RefreshAutoCameraRelativeRotation(InAnimInstance);
}

void FAnimNode_ACFProceduralAimOffset::EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms)
{
	const FBoneContainer& BoneContainer = Output.Pose.GetPose().GetBoneContainer();
	const FRotator TargetRot = bAutoFillRotationFromCamera || bAutoFillRotationFromLeaning ? AutoCameraRelativeRotation : CameraRelativeRotation;

	// 1. Construct Aim Quaternion (Component/Mesh Space)
	FQuat OrientationCameraRot(FVector::ForwardVector, 0.f);
	OrientationCameraRot *= FQuat(-OrientationCameraRot.GetRightVector(), FMath::DegreesToRadians(TargetRot.Roll));
	OrientationCameraRot *= FQuat(OrientationCameraRot.GetUpVector().RotateAngleAxis(TargetRot.Roll * 2.f, FVector::RightVector), FMath::DegreesToRadians(TargetRot.Yaw));
	OrientationCameraRot *= FQuat(OrientationCameraRot.GetForwardVector().RotateAngleAxis(TargetRot.Yaw, -FVector::UpVector).RotateAngleAxis(TargetRot.Roll * 2.f, FVector::RightVector), FMath::DegreesToRadians(TargetRot.Pitch));

	// Normalize to ensure stability
	OrientationCameraRot.Normalize();

	// 2. Prepare Forward Kinematics
	FCompactPoseBoneIndex FirstBoneIndex = SpineBoneParams[0].Bone.GetCompactPoseIndex(BoneContainer);
	FCompactPoseBoneIndex ParentIndex = BoneContainer.GetParentBoneIndex(FirstBoneIndex);
	FTransform CurrentParentTransform = Output.Pose.GetComponentSpaceTransform(ParentIndex);

	float AccumulatedWeight = 0.f;

	// 3. Iterate Bones
	for (const FACFBoneParams& BoneParams : SpineBoneParams)
	{
		FCompactPoseBoneIndex BoneIndex = BoneParams.Bone.GetCompactPoseIndex(BoneContainer);

		AccumulatedWeight += BoneParams.Weight;
		const float ClampedWeight = FMath::Min(AccumulatedWeight, 1.f);

		// Slerp calculates the correct intermediate quaternion without axis flipping issues.
		FQuat WeightedAimQuat = FQuat::Slerp(FQuat::Identity, OrientationCameraRot, ClampedWeight);

		// Get original pose
		const FTransform OriginalBoneCSTransform = Output.Pose.GetComponentSpaceTransform(BoneIndex);
		FQuat CurrentCSRotation = OriginalBoneCSTransform.GetRotation();

		// Apply rotation
		FQuat NewCSRotation = WeightedAimQuat * CurrentCSRotation;

		// Reconstruct Position (FK)
		FTransform OriginalLocalTransform = Output.Pose.GetLocalSpaceTransform(BoneIndex);

		FTransform NewCSTransform;
		NewCSTransform.SetRotation(NewCSRotation);
		NewCSTransform.SetScale3D(OriginalBoneCSTransform.GetScale3D());
		NewCSTransform.SetLocation(CurrentParentTransform.TransformPosition(OriginalLocalTransform.GetLocation()));

		OutBoneTransforms.Add(FBoneTransform(BoneIndex, NewCSTransform));

		// Update parent for next bone
		CurrentParentTransform = NewCSTransform;
	}
}

void FAnimNode_ACFProceduralAimOffset::RefreshAutoCameraRelativeRotation(const UAnimInstance* InAnimInstance)
{
	if (!InAnimInstance || (!bAutoFillRotationFromCamera && !bAutoFillRotationFromLeaning)) return;

	USkeletalMeshComponent* MeshComp = InAnimInstance->GetSkelMeshComponent();
	if (!MeshComp) return;

	APawn* OwningPawn = MeshComp->GetOwner<APawn>();
	if (!OwningPawn) return;

	if (bAutoFillRotationFromLeaning)
	{
		if (UACFProceduralLeaningComponent* LeaningComp = OwningPawn->FindComponentByClass<UACFProceduralLeaningComponent>())
		{
			AutoCameraRelativeRotation = FRotator(0.0f, 0.0f, LeaningComp->GetCurrentLeanAngle());
		}
		
		return;
	}

	FRotator TargetCameraRelativeRotation = UACFFirstPersonFunctionLibrary::GetCameraRelativeRotation(OwningPawn);

	APlayerController* PC = OwningPawn->GetController<APlayerController>();
	if (PC && PC->IsLocalController())
	{
		AutoCameraRelativeRotation = TargetCameraRelativeRotation;
	}
	else
	{
		AutoCameraRelativeRotation = UKismetMathLibrary::RInterpTo(AutoCameraRelativeRotation, TargetCameraRelativeRotation, InAnimInstance->GetDeltaSeconds(), 15.0f);
	}
}