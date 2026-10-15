// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "Components/ACFProceduralAimMotionComponent.h"

#include "Animation/AnimInstanceProxy.h"
#include "BoneContainer.h"
#include "BonePose.h"
#include "Curves/CurveFloat.h"
#include "Presets/ACFProceduralPreset.h"
#include "Presets/ACFProceduralPresetAimMotion.h"
#include "Camera/CameraComponent.h"
#include "Components/ACFProceduralRecoilMotionComponent.h"
#include "GameFramework/Character.h"

UACFProceduralAimMotionComponent::UACFProceduralAimMotionComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Stage = 2;
}

void UACFProceduralAimMotionComponent::BeforeEvaluate(float DeltaSeconds, FComponentSpacePoseContext& Context, FCompactPoseBoneIndex& BoneIndex)
{
	USkeleton* Skeleton = Context.AnimInstanceProxy->GetSkeleton();
	if (!Skeleton) return;

	const UACFProceduralPresetAimMotion* PresetAim = UACFProceduralPresetData::GetPresetOrDefault<UACFProceduralPresetAimMotion>(GetEffectivePresetData());
	if (!PresetAim) return;

	// The Aim Point solver reads the pose directly in ComputePoseOutputs and needs no reference sampling.
	if (PresetAim->Solver != EACFProceduralAimSolver::Legacy) return;

	if (bCanCacheRef)
	{
		const int32 MyBoneIndex = BoneIndex.GetInt();
		FName MyBoneName = Skeleton->GetReferenceSkeleton().GetBoneName(MyBoneIndex);
		const int32 ParentBoneIndex = Skeleton->GetReferenceSkeleton().GetParentIndex(MyBoneIndex);
		FName ParentBoneName = Skeleton->GetReferenceSkeleton().GetBoneName(ParentBoneIndex);

		if (!bIsRefCached && PresetAim->ReferenceAnimation)
		{
			FTransform RefGunCS = ExtractBoneTransform(PresetAim->ReferenceAnimation, Skeleton, MyBoneName);
			FTransform RefHeadCS = ExtractBoneTransform(PresetAim->ReferenceAnimation, Skeleton, ParentBoneName);

			if (!PresetAim->OriginRelativeRotation.IsZero())
			{
				RefGunCS.SetRotation(RefGunCS.GetRotation() * PresetAim->OriginRelativeRotation.Quaternion());
			}

			if (!CachedPivotOffset.IsZero())
			{
				FVector AdjustedPivot = RefGunCS.GetRotation().RotateVector(CachedPivotOffset);
				RefGunCS.AddToTranslation(AdjustedPivot);
			}

			CachedRefRelativeTransform = RefGunCS.GetRelativeTransform(RefHeadCS);
			bIsRefCached = true;
		}

		if (Context.Pose.GetPose().IsValidIndex(BoneIndex))
		{
			FTransform CurrentGunCS = Context.Pose.GetComponentSpaceTransform(BoneIndex);

			FCompactPoseBoneIndex HeadIndex = Context.Pose.GetPose().GetBoneContainer().GetCompactPoseIndexFromSkeletonIndex(
				Skeleton->GetReferenceSkeleton().FindBoneIndex(ParentBoneName));

			FTransform CurrentHeadCS = FTransform::Identity;
			if (Context.Pose.GetPose().IsValidIndex(HeadIndex))
			{
				CurrentHeadCS = Context.Pose.GetComponentSpaceTransform(HeadIndex);
			}

			CurrentRelativeTransform = CurrentGunCS.GetRelativeTransform(CurrentHeadCS);
			bHasValidIncomingPose = true;
		}
	}
}

void UACFProceduralAimMotionComponent::ComputeOutputs(float DeltaSeconds, const UAnimInstance* AnimInstance, const FACFProceduralMotionInput& Input, TArray<FACFProceduralMotionOutput>& Outs)
{
	CachedPivotOffset = Input.PivotOffset;
	if (!bCanCacheRef) { bCanCacheRef = true; }

	CachedAimAlpha = 0.0f;
	CachedAdditiveAlpha = 0.0f;

	const UACFProceduralPresetAimMotion* PresetAim = UACFProceduralPresetData::GetPresetOrDefault<UACFProceduralPresetAimMotion>(GetEffectivePresetData());
	if (!PresetAim) return;

	const float AimingState = Input.AimingState * (1.0f - FMath::Clamp(ReadCurve(AnimInstance, PresetAim->MaskAimCurve), 0.0f, 1.0f));

	if (PresetAim->bUseAimingFOV)
	{
		RefreshFOV(DeltaSeconds, PresetAim->bAimFOVAffectedByCurve ? AimingState : Input.AimingState, PresetAim);
	}

	if (AimingState <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	CachedAimAlpha = FMath::Clamp(AimingState, 0.0f, 1.0f);

	if (PresetAim->Solver == EACFProceduralAimSolver::Legacy)
	{
		// The Aim Point solver emits from ComputePoseOutputs, where the current pose is available.
		ComputeLegacyOutputs(AnimInstance, Input, AimingState, PresetAim, Outs);
		return;
	}

	// No curve gate here: the delta is measured against a live reference pose, so it is already zero
	// unless something is playing. AimRatio alone decides how much of it survives aiming.
	CachedAdditiveAlpha = FMath::Clamp(PresetAim->AimRatio, 0.0f, 1.0f);
}

bool UACFProceduralAimMotionComponent::NeedsReferencePose() const
{
	const UACFProceduralPresetAimMotion* PresetAim = UACFProceduralPresetData::GetPresetOrDefault<UACFProceduralPresetAimMotion>(GetEffectivePresetData());

	return PresetAim
		&& PresetAim->Solver == EACFProceduralAimSolver::AimPoint
		&& PresetAim->AimRatio > KINDA_SMALL_NUMBER;
}

void UACFProceduralAimMotionComponent::ComputePoseOutputs(float DeltaSeconds, const FACFProceduralMotionPoseContext& Context, TArray<FACFProceduralMotionOutput>& Outs)
{
	const UACFProceduralPresetAimMotion* PresetAim = UACFProceduralPresetData::GetPresetOrDefault<UACFProceduralPresetAimMotion>(GetEffectivePresetData());
	if (!PresetAim || PresetAim->Solver != EACFProceduralAimSolver::AimPoint)
	{
		return;
	}

	if (CachedAimAlpha <= KINDA_SMALL_NUMBER || Context.ParentBoneIndex == INDEX_NONE)
	{
		return;
	}

	// Solve X so that X * AimPointTransform lands on the parent bone origin with component-space identity
	// rotation. Procedural Aim Offset rotates the spine afterwards, which carries it onto the camera axis.
	//
	//   X.Rotation = Inverse(Aim.Rotation)
	//   X.Location = Parent.Location - X.Rotation.Rotate(Aim.Location)
	//
	// The aim point's own rotation supplies the equipment's orientation, so there is no forward-axis rule.
	const FQuat InverseAimRotation = AimPointTransform.GetRotation().Inverse();
	const FTransform ParentCS = Context.GetComponentSpaceTransform(Context.ParentBoneIndex);

	FTransform Target;
	Target.SetRotation(InverseAimRotation);
	Target.SetLocation(ParentCS.GetLocation() - InverseAimRotation.RotateVector(AimPointTransform.GetLocation()));

	// Re-apply whatever the live animation is contributing over the reference, in the aimed weapon's own
	// frame. Identity while only the base pose plays, so this stays correct at full strength.
	if (Context.bHasReferencePose && CachedAdditiveAlpha > KINDA_SMALL_NUMBER)
	{
		const FTransform Delta = Context.GetBoneLocalTransform().GetRelativeTransform(Context.ReferenceBoneLocal);

		FTransform ScaledDelta;
		ScaledDelta.Blend(FTransform::Identity, Delta, CachedAdditiveAlpha);

		Target = ScaledDelta * Target;
	}

	FACFProceduralMotionOutput Out;
	Out.Stage = Stage;
	Out.Composition = EACFProceduralMotionComposition::LerpToTarget;
	Out.Space = EACFProceduralMotionOutputSpace::Component;
	Out.Transform = Target;
	Out.Weight = CachedAimAlpha;
	Outs.Add(Out);
}

void UACFProceduralAimMotionComponent::ComputeLegacyOutputs(const UAnimInstance* AnimInstance, const FACFProceduralMotionInput& Input, float AimingState, const UACFProceduralPresetAimMotion* PresetAim, TArray<FACFProceduralMotionOutput>& Outs)
{
	const FRotator AimOffsetRot = AimOffset.Rotator();
	const FQuat TargetAimRot = FQuat::MakeFromRotator(FRotator(/*Pitch*/ AimOffsetRot.Roll, /*Yaw*/ AimOffsetRot.Yaw, /*Roll*/ AimOffsetRot.Pitch));

	FVector TargetAimLoc = AimOffset.GetLocation();
	TargetAimLoc -= Input.PivotOffset;
	TargetAimLoc.Z *= -1.f;
	TargetAimLoc.X *= -1.f;
	Swap(TargetAimLoc.X, TargetAimLoc.Y);
	TargetAimLoc = TargetAimRot.RotateVector(TargetAimLoc);

	// Position Reset
	{
		FACFProceduralMotionOutput Out;
		Out.Stage = Stage;
		Out.Composition = EACFProceduralMotionComposition::LerpToTarget;
		Out.Space = EACFProceduralMotionOutputSpace::ParentBone;
		Out.Transform = FTransform::Identity;
		Out.Weight = AimingState;
		Out.bMaskLocation = false;
		Out.bMaskRotation = true;
		Outs.Add(Out);
	}

	// Rotation Align
	{
		FACFProceduralMotionOutput Out;
		Out.Stage = Stage;
		Out.Composition = EACFProceduralMotionComposition::LerpToTarget;
		Out.Space = EACFProceduralMotionOutputSpace::Component;
		Out.Transform = FTransform(FRotator(0.0f, 90.0f, 0.0f));
		Out.Weight = AimingState;
		Out.bMaskLocation = true;
		Out.bMaskRotation = false;
		Outs.Add(Out);
	}

	// Additive Offset
	{
		FACFProceduralMotionOutput Out;
		Out.Stage = Stage;
		Out.Composition = EACFProceduralMotionComposition::Additive;
		Out.Space = EACFProceduralMotionOutputSpace::Component;
		Out.Transform = FTransform(TargetAimRot, TargetAimLoc);
		Out.Weight = AimingState;
		Out.bMaskLocation = false;
		Out.bMaskRotation = false;
		Outs.Add(Out);
	}

	// Motion Re-injection
	if (bHasValidIncomingPose && bIsRefCached && PresetAim->AimRatio > KINDA_SMALL_NUMBER)
	{
		FTransform MotionDelta = CurrentRelativeTransform.GetRelativeTransform(CachedRefRelativeTransform);

		FACFProceduralMotionOutput OutMotion;
		OutMotion.Stage = Stage;
		OutMotion.Composition = EACFProceduralMotionComposition::Additive;
		OutMotion.Space = EACFProceduralMotionOutputSpace::Bone;

		OutMotion.Transform = MotionDelta;
		OutMotion.Weight = FMath::Clamp(AimingState * PresetAim->AimRatio * ReadCurve(AnimInstance, PresetAim->AimAdditiveMotionAlphaCurve), 0.0f, 1.0f);
		OutMotion.bMaskLocation = false;
		OutMotion.bMaskRotation = false;
		Outs.Add(OutMotion);
	}
}

FTransform UACFProceduralAimMotionComponent::ExtractBoneTransform(const UAnimSequence* Animation, USkeleton* Skeleton, FName BoneName) const
{
	if (!Animation || !Skeleton) return FTransform::Identity;

	const int32 BoneIndex = Skeleton->GetReferenceSkeleton().FindBoneIndex(BoneName);
	if (BoneIndex == INDEX_NONE) return FTransform::Identity;

	TArray<FBoneIndexType> RequiredBones;
	RequiredBones.Add(BoneIndex);
	Skeleton->GetReferenceSkeleton().EnsureParentsExistAndSort(RequiredBones);

	const UE::Anim::FCurveFilterSettings CurveFilterSettings(UE::Anim::ECurveFilterMode::DisallowAll, nullptr, 0);
	FBoneContainer BoneContainer(RequiredBones, CurveFilterSettings, *Skeleton);

	FCompactPose Pose;
	Pose.SetBoneContainer(&BoneContainer);
	FBlendedCurve Curve;
	Curve.InitFrom(BoneContainer);
	UE::Anim::FStackAttributeContainer Attributes;
	FAnimationPoseData AnimationPoseData(Pose, Curve, Attributes);

	FAnimExtractContext Context(0.0, false);
	Animation->GetBonePose(AnimationPoseData, Context, false);

	FCompactPoseBoneIndex CompactIndex = BoneContainer.MakeCompactPoseIndex(FMeshPoseBoneIndex(BoneIndex));

	if (Pose.IsValidIndex(CompactIndex))
	{
		FCSPose<FCompactPose> CSPose;
		CSPose.InitPose(Pose);
		return CSPose.GetComponentSpaceTransform(CompactIndex);
	}

	return FTransform::Identity;
}

void UACFProceduralAimMotionComponent::RefreshFOV(float DeltaTime, float AimState, const UACFProceduralPresetAimMotion* Preset)
{
	if (!IsValid(Camera))
	{
		if (IsValid(AnimOwnerCharacter))
		{
			Camera = AnimOwnerCharacter->GetComponentByClass<UCameraComponent>();
			if (IsValid(Camera))
			{
				DefaultFOV = Camera->FieldOfView;
			}
			else
			{
				return;
			}
		}
		else
		{
			return;
		}
	}

	const float CurrentFOV = Camera->FieldOfView;
	const float TargetFOV = FMath::Lerp(DefaultFOV, Preset->AimingFOV, AimState);

	if (CurrentFOV == TargetFOV)
		return;

	const float NewFOV = FMath::FInterpTo(CurrentFOV, TargetFOV, DeltaTime, TargetFOV > CurrentFOV ? Preset->AimOutFOVInterpSpeed : Preset->AimInFOVInterpSpeed);
	Camera->SetFieldOfView(NewFOV);
}

void UACFProceduralAimMotionComponent::SetAimPointTransform(const FTransform& InAimPoint)
{
	AimPointTransform = InAimPoint;
	AimPointTransform.SetScale3D(FVector::OneVector);

	// Recoil pivots on the sight while aiming, which is this same point. Only the owning actor is searched;
	// a recoil component anywhere else keeps the ADSOffset it was given.
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	TInlineComponentArray<UACFProceduralRecoilMotionComponent*> RecoilComponents(Owner);
	for (UACFProceduralRecoilMotionComponent* Recoil : RecoilComponents)
	{
		Recoil->ADSOffset = AimPointTransform;
	}
}

void UACFProceduralAimMotionComponent::SetDefaultFOV(float InValue)
{
	DefaultFOV = InValue;
}

TSubclassOf<UACFProceduralPreset> UACFProceduralAimMotionComponent::GetPresetClass() const
{
	return UACFProceduralPresetAimMotion::StaticClass();
}
