// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "Nodes/AnimNode_ACFProceduralMotion.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "AnimationRuntime.h"
#include "GameFramework/Actor.h"

#include "ACFFirstPersonLog.h"
#include "Components/ACFProceduralMotionComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Debug/ACFProceduralMotionDebug.h"

#if !UE_BUILD_SHIPPING
static TAutoConsoleVariable<int32> CVarLogReferenceDelta(
	TEXT("ACF.FirstPerson.Debug.ReferenceDelta"),
	0,
	TEXT("Log the reference pose delta for the modified bone.\n")
	TEXT("Near identity while only the base pose plays. A delta that never settles means the live branch\n")
	TEXT("carries a correction the reference branch does not -- see Docs/ADS-REDESIGN.md.\n")
	TEXT("<=0: off, 1: on"),
	ECVF_Default);
#endif

void FAnimNode_ACFProceduralMotion::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	Super::Initialize_AnyThread(Context);

	if (IsReferencePoseLinked())
	{
		ReferencePose.Initialize(Context);
	}

	ResetStageOps();
	EvaluationComps.Reset();
}

void FAnimNode_ACFProceduralMotion::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context)
{
	Super::CacheBones_AnyThread(Context);

	if (IsReferencePoseLinked())
	{
		ReferencePose.CacheBones(Context);
	}
}

void FAnimNode_ACFProceduralMotion::UpdateInternal(const FAnimationUpdateContext& Context)
{
	Super::UpdateInternal(Context);

	// Always update when linked, even if we end up not evaluating it. Evaluating a branch that was never
	// updated trips the graph traversal checks.
	if (IsReferencePoseLinked())
	{
		ReferencePose.Update(Context);
	}
}

bool FAnimNode_ACFProceduralMotion::IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones)
{
	return BoneToModify.IsValidToEvaluate(RequiredBones);
}

void FAnimNode_ACFProceduralMotion::InitializeBoneReferences(const FBoneContainer& RequiredBones)
{
	BoneToModify.Initialize(RequiredBones);
}

void FAnimNode_ACFProceduralMotion::ResetStageOps()
{
	for (TArray<FACFProceduralMotionStageOp>& Stage : StageOps)
	{
		Stage.Reset();
	}
}

void FAnimNode_ACFProceduralMotion::PreUpdate(const UAnimInstance* InAnimInstance)
{
	EvaluationComps.Reset();

	// Ops are harvested during evaluation, not here, so that components can emit pose-derived ops.
	if (!InAnimInstance || Input.HasValidCustomStageOps())
	{
		return;
	}

	const float DeltaSeconds = InAnimInstance->GetDeltaSeconds();

	ActorsToProcess.Reset();

	if (AActor* Owner = InAnimInstance->GetOwningActor())
	{
		ActorsToProcess.Add(Owner);

		// Opt-in: stowed equipment usually stays attached, so this harvests the whole loadout at once.
		if (Input.bHarvestAttachedActors)
		{
			AttachedActors.Reset();
			Owner->GetAttachedActors(AttachedActors, true, true);

			for (AActor* Attached : AttachedActors)
			{
				if (IsValid(Attached))
				{
					ActorsToProcess.Add(Attached);
				}
			}
		}
	}

	for (const TObjectPtr<AActor>& ActorPtr : Input.SourceActors)
	{
		if (AActor* Actor = ActorPtr.Get())
		{
			ActorsToProcess.Add(Actor);
		}
	}

	for (AActor* SourceActor : ActorsToProcess)
	{
		if (!SourceActor || !SourceActor->HasActorBegunPlay())
		{
			continue;
		}

		ActorComps.Reset();
		SourceActor->GetComponents(ActorComps);

		for (UACFProceduralMotionComponent* Comp : ActorComps)
		{
			if (!IsValid(Comp) || !Comp->IsRegistered())
			{
				continue;
			}

			// ActorComps is per-actor scratch, so components have to be accumulated here or evaluation
			// would only ever see the last actor's.
			EvaluationComps.Add(Comp);

			Comp->UpdateOutput(DeltaSeconds, InAnimInstance, Input);
		}
	}
}

void FAnimNode_ACFProceduralMotion::CopyCustomStageOps()
{
	const int32 CustomStageCount = Input.CustomStageOps.Num();
	if (StageOps.Num() < CustomStageCount)
	{
		StageOps.SetNum(CustomStageCount);
	}

	for (int32 StageIndex = 0; StageIndex < CustomStageCount; ++StageIndex)
	{
		StageOps[StageIndex] = Input.CustomStageOps[StageIndex];
	}
}

void FAnimNode_ACFProceduralMotion::AddStageOp(const FACFProceduralMotionOutput& Out)
{
	if (Out.Weight <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	if (Out.Composition == EACFProceduralMotionComposition::Additive && Out.Transform.Equals(FTransform::Identity))
	{
		return;
	}

	const int32 StageIndex = FMath::Max(Out.Stage, 0);
	if (StageOps.Num() <= StageIndex)
	{
		StageOps.SetNum(StageIndex + 1);
	}

	FACFProceduralMotionStageOp& Op = StageOps[StageIndex].AddDefaulted_GetRef();
	Op.Transform = Out.Transform;
	Op.Weight = FMath::Clamp(Out.Weight, 0.f, 1.f);
	Op.Space = Out.Space;
	Op.Comp = Out.Composition;
	Op.bMaskLocation = Out.bMaskLocation;
	Op.bMaskRotation = Out.bMaskRotation;
}

void FAnimNode_ACFProceduralMotion::ResolveReferencePose(FACFProceduralMotionPoseContext& PoseContext)
{
	if (!IsReferencePoseLinked() || PoseContext.ParentBoneIndex == INDEX_NONE)
	{
		return;
	}

	// A full extra pose evaluation, so only pay for it when a component actually reads the result.
	bool bAnyComponentNeedsIt = false;
	for (const TWeakObjectPtr<UACFProceduralMotionComponent>& WeakComp : EvaluationComps)
	{
		const UACFProceduralMotionComponent* Comp = WeakComp.Get();
		if (Comp && Comp->IsRegistered() && Comp->NeedsReferencePose())
		{
			bAnyComponentNeedsIt = true;
			break;
		}
	}

	if (!bAnyComponentNeedsIt)
	{
		return;
	}

	FPoseContext ReferenceContext(PoseContext.PoseContext.AnimInstanceProxy);
	ReferencePose.Evaluate(ReferenceContext);

	if (!ReferenceContext.Pose.IsValidIndex(PoseContext.BoneIndex))
	{
		return;
	}

	// A local pose already stores every bone relative to its parent, virtual bones included, so this is
	// the whole conversion.
	FTransform ReferenceLocal = ReferenceContext.Pose[PoseContext.BoneIndex];

	// Same order as FAnimNode_ACFProceduralBoneRebase::ProcessBonePair: rotate, then offset along the result.
	// Applying it to the local transform is equivalent to applying it in component space.
	if (!ReferenceOffsetRotation.IsZero())
	{
		ReferenceLocal.SetRotation(ReferenceLocal.GetRotation() * ReferenceOffsetRotation.Quaternion());
	}

	if (!ReferenceOffsetLocation.IsZero())
	{
		ReferenceLocal.AddToTranslation(ReferenceLocal.GetRotation().RotateVector(ReferenceOffsetLocation));
	}

	PoseContext.ReferenceBoneLocal = ReferenceLocal;
	PoseContext.bHasReferencePose = true;

#if !UE_BUILD_SHIPPING
	if (CVarLogReferenceDelta.GetValueOnAnyThread() > 0)
	{
		const FTransform Delta = PoseContext.GetBoneLocalTransform().GetRelativeTransform(PoseContext.ReferenceBoneLocal);

		UE_LOG(LogACFFirstPerson, Log, TEXT("Reference delta on %s: rotation %s, translation %s"),
			*BoneToModify.BoneName.ToString(),
			*Delta.Rotator().ToCompactString(),
			*Delta.GetTranslation().ToCompactString());
	}
#endif
}

void FAnimNode_ACFProceduralMotion::EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms)
{
	check(OutBoneTransforms.Num() == 0);

	const FBoneContainer& BoneContainer = Output.Pose.GetPose().GetBoneContainer();
	FCompactPoseBoneIndex BoneIndex = BoneToModify.GetCompactPoseIndex(BoneContainer);

	const float DeltaSeconds = Output.AnimInstanceProxy->GetDeltaSeconds();

	ResetStageOps();

#if ACF_FP_DEBUG_CAPTURE
	const bool bCaptureDebug = FACFProceduralMotionDebugRegistry::IsCaptureEnabled();
	FACFProceduralMotionDebugSnapshot DebugSnapshot;
#endif

	if (Input.HasValidCustomStageOps())
	{
		CopyCustomStageOps();
	}
	else
	{
		FACFProceduralMotionPoseContext PoseContext(Output, Input, BoneIndex, BoneContainer.GetParentBoneIndex(BoneIndex));
		ResolveReferencePose(PoseContext);

#if ACF_FP_DEBUG_CAPTURE
		DebugSnapshot.bHasReferencePose = PoseContext.bHasReferencePose;
#endif

		for (const TWeakObjectPtr<UACFProceduralMotionComponent>& WeakComp : EvaluationComps)
		{
			UACFProceduralMotionComponent* Comp = WeakComp.Get();
			if (!Comp || !Comp->IsRegistered())
			{
				continue;
			}

			Comp->BeforeEvaluate(DeltaSeconds, Output, BoneIndex);
			Comp->UpdatePoseOutputs(DeltaSeconds, PoseContext);

#if ACF_FP_DEBUG_CAPTURE
			if (bCaptureDebug)
			{
				FACFProceduralMotionDebugComponent& DebugComp = DebugSnapshot.Components.AddDefaulted_GetRef();
				DebugComp.Component = WeakComp;
				Comp->GetDebugState(DebugComp.Values);
			}
#endif

			for (const FACFProceduralMotionOutput& Out : Comp->GetCachedOutputs())
			{
				AddStageOp(Out);

#if ACF_FP_DEBUG_CAPTURE
				if (bCaptureDebug)
				{
					FACFProceduralMotionDebugOp& DebugOp = DebugSnapshot.Ops.AddDefaulted_GetRef();
					DebugOp.Transform = Out.Transform;
					DebugOp.Weight = Out.Weight;
					DebugOp.Stage = Out.Stage;
					DebugOp.Space = Out.Space;
					DebugOp.Composition = Out.Composition;
					DebugOp.bMaskLocation = Out.bMaskLocation;
					DebugOp.bMaskRotation = Out.bMaskRotation;
					DebugOp.Component = WeakComp;
					DebugOp.ComponentName = Comp->GetName();
				}
#endif
			}
		}
	}

	FTransform BoneCS = Output.Pose.GetComponentSpaceTransform(BoneIndex);

	for (const TArray<FACFProceduralMotionStageOp>& Stage : StageOps)
	{
		for (const FACFProceduralMotionStageOp& Op : Stage)
		{
			ApplyStageOp(BoneCS, Op, Output, BoneIndex);
		}
	}

	OutBoneTransforms.Emplace(BoneIndex, BoneCS);

#if ACF_FP_DEBUG_CAPTURE
	if (bCaptureDebug)
	{
		const USkeletalMeshComponent* Mesh = Output.AnimInstanceProxy->GetSkelMeshComponent();

		DebugSnapshot.Mesh = Mesh;
		const AActor* MeshOwner = Mesh ? Mesh->GetOwner() : nullptr;
		DebugSnapshot.MeshName = MeshOwner ? FString::Printf(TEXT("%s.%s"), *MeshOwner->GetName(), *Mesh->GetName()) : FString();
		DebugSnapshot.BoneName = BoneToModify.BoneName;
		DebugSnapshot.ResultCS = BoneCS;
		DebugSnapshot.AimingState = Input.AimingState;
		DebugSnapshot.bFirstPerson = Input.bFirstPerson;
		DebugSnapshot.TimeSeconds = FPlatformTime::Seconds();

		// Ops arrive component by component, so sort into the order they were actually applied.
		DebugSnapshot.Ops.StableSort([](const FACFProceduralMotionDebugOp& A, const FACFProceduralMotionDebugOp& B)
		{
			return A.Stage < B.Stage;
		});

		Output.Curve.ForEachElement([&DebugSnapshot](const UE::Anim::FCurveElement& Element)
		{
			DebugSnapshot.Curves.Emplace(Element.Name, Element.Value);
		});

		FACFProceduralMotionDebugRegistry::Get().Publish(MoveTemp(DebugSnapshot));
	}
#endif
}

void FAnimNode_ACFProceduralMotion::ApplyStageOp(FTransform& InOutBoneCS, const FACFProceduralMotionStageOp& Op, FComponentSpacePoseContext& Output, FCompactPoseBoneIndex BoneIndex) const
{
	const float LocAlpha = Op.bMaskLocation ? 0.f : Op.Weight;
	const float RotAlpha = Op.bMaskRotation ? 0.f : Op.Weight;

	if (FMath::IsNearlyZero(LocAlpha) && FMath::IsNearlyZero(RotAlpha))
	{
		return;
	}

	const bool bComponentSpace = Op.Space == EACFProceduralMotionOutputSpace::Component;
	const EBoneControlSpace BoneSpace = (Op.Space == EACFProceduralMotionOutputSpace::Bone) ? BCS_BoneSpace : BCS_ParentBoneSpace;
	const FTransform& ComponentTransform = Output.AnimInstanceProxy->GetComponentTransform();

	switch (Op.Comp)
	{
	case EACFProceduralMotionComposition::LerpToTarget:
	{
		FTransform TargetCS = Op.Transform;
		if (!bComponentSpace)
		{
			FAnimationRuntime::ConvertBoneSpaceTransformToCS(ComponentTransform, Output.Pose, TargetCS, BoneIndex, BoneSpace);
		}

		if (LocAlpha > 0.f)
		{
			InOutBoneCS.SetLocation(FMath::Lerp(InOutBoneCS.GetLocation(), TargetCS.GetLocation(), LocAlpha));
		}

		if (RotAlpha > 0.f)
		{
			InOutBoneCS.SetRotation(FQuat::Slerp(InOutBoneCS.GetRotation(), TargetCS.GetRotation(), RotAlpha));
		}

		break;
	}

	case EACFProceduralMotionComposition::Additive:
	{
		const FVector DeltaTranslation = Op.Transform.GetTranslation() * LocAlpha;
		const FQuat DeltaRotation = FQuat::Slerp(FQuat::Identity, Op.Transform.GetRotation(), RotAlpha);

		if (bComponentSpace)
		{
			InOutBoneCS.AddToTranslation(DeltaTranslation);
			InOutBoneCS.SetRotation(DeltaRotation * InOutBoneCS.GetRotation());
			break;
		}

		FTransform LocalTransform = InOutBoneCS;
		FAnimationRuntime::ConvertCSTransformToBoneSpace(ComponentTransform, Output.Pose, LocalTransform, BoneIndex, BoneSpace);

		LocalTransform.AddToTranslation(DeltaTranslation);
		LocalTransform.SetRotation(DeltaRotation * LocalTransform.GetRotation());

		FAnimationRuntime::ConvertBoneSpaceTransformToCS(ComponentTransform, Output.Pose, LocalTransform, BoneIndex, BoneSpace);
		InOutBoneCS = LocalTransform;

		break;
	}

	default:
		// Custom0..Custom5 are reserved and have no composition rule yet.
		break;
	}
}
