#include "Animation/AnimNode_ACFLookAtTarget.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimStats.h"
#include "Animation/AnimTrace.h"
#include "AnimationCoreLibrary.h"
#include "ATSBaseTargetComponent.h"
#include "BonePose.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Kismet/KismetMathLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "SceneManagement.h"
#include "Stats/Stats.h"
#include <GameFramework/Pawn.h>

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimNode_ACFLookAtTarget)

static const FVector DefaultLookAtAxis(0.f, 1.f, 0.f);
static const FVector DefaultLookUpAxis(1.f, 0.f, 0.f);

FAnimNode_ACFLookAtTarget::FAnimNode_ACFLookAtTarget()
    : LookAt_Axis(DefaultLookAtAxis)
    , bUseLookUpAxis(false)
    , LookUp_Axis(DefaultLookUpAxis)
    , NoTargetOffset(ForceInitToZero)
    , LookAtClamp(60.f)
    , InterpolationSpeed(3.f)
    , CurrentLookAtLocation(ForceInitToZero)
    , CurrentTargetLocation(ForceInitToZero)
    , PreviousTargetLocation(ForceInitToZero)
    , AccumulatedInterpoolationTime(0.f)
    , CachedCurrentTargetLocation(ForceInitToZero)
    , LerpedDelta(ForceInit)
    , DeltaTime(0.f)
    , CachedWorldLookAtLocation(ForceInitToZero)
    , CachedNoTargetLookAtLocation(ForceInitToZero)
    , bCachedHasLookAtTarget(false)
    , bCachedHasOwner(false)
{
    Owner = nullptr;
}

void FAnimNode_ACFLookAtTarget::PreUpdate(const UAnimInstance* InAnimInstance)
{
    QUICK_SCOPE_CYCLE_COUNTER(STAT_FAnimNode_ACFLookAtTarget_PreUpdate);

    bCachedHasOwner = false;
    bCachedHasLookAtTarget = false;
    CachedWorldLookAtLocation = FVector::ZeroVector;
    CachedNoTargetLookAtLocation = FVector::ZeroVector;

    const USkeletalMeshComponent* meshComponent = InAnimInstance ? InAnimInstance->GetSkelMeshComponent() : nullptr;
    const AActor* ownerActor = Owner ? static_cast<const AActor*>(Owner) : (meshComponent ? meshComponent->GetOwner() : nullptr);
    if (!ownerActor) {
        return;
    }

    bCachedHasOwner = true;

    FVector eyesView = FVector::ZeroVector;
    FRotator unusedRot = FRotator::ZeroRotator;
    ownerActor->GetActorEyesViewPoint(eyesView, unusedRot);
    CachedNoTargetLookAtLocation = eyesView + (100.f * ownerActor->GetActorForwardVector());

    if (const UATSBaseTargetComponent* targetComp = UATSBaseTargetComponent::FindForActor(ownerActor)) {
        FVector targetLocation = FVector::ZeroVector;
        if (targetComp->GetLookAtLocation(targetLocation)) {
            CachedWorldLookAtLocation = targetLocation;
            bCachedHasLookAtTarget = true;
        }
    }
}

void FAnimNode_ACFLookAtTarget::UpdateInternal(const FAnimationUpdateContext& Context)
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(UpdateInternal)
    FAnimNode_SkeletalControlBase::UpdateInternal(Context);
    DeltaTime = Context.GetDeltaTime();
    AccumulatedInterpoolationTime = FMath::Clamp(AccumulatedInterpoolationTime + Context.GetDeltaTime(), 0.f, InterpolationSpeed);
}

void FAnimNode_ACFLookAtTarget::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(Initialize_AnyThread)
    FAnimNode_SkeletalControlBase::Initialize_AnyThread(Context);

    // initialize
    LookUp_Axis.Initialize();
    if (LookUp_Axis.Axis.IsZero()) {
        UE_LOG(LogAnimation, Warning, TEXT("Zero-length look-up axis specified in LookAt node. Reverting to default. Instance:%s"), *GetFullNameSafe(Context.GetAnimInstanceObject()));
        LookUp_Axis.Axis = DefaultLookUpAxis;
    }
    LookAt_Axis.Initialize();
    if (LookAt_Axis.Axis.IsZero()) {
        UE_LOG(LogAnimation, Warning, TEXT("Zero-length look-at axis specified in LookAt node. Reverting to default. Instance:%s"), *GetFullNameSafe(Context.GetAnimInstanceObject()));
        LookAt_Axis.Axis = DefaultLookAtAxis;
    }
}

void FAnimNode_ACFLookAtTarget::EvaluateComponentSpaceInternal(FComponentSpacePoseContext& Context)
{
    Super::EvaluateComponentSpaceInternal(Context);
}

void FAnimNode_ACFLookAtTarget::EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms)
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(EvaluateSkeletalControl_AnyThread)
    ANIM_MT_SCOPE_CYCLE_COUNTER_VERBOSE(LookAt, !IsInGameThread());

    if (!bCachedHasOwner) {
        return;
    }

    FVector LookAtLocation;
    if (bCachedHasLookAtTarget) {
        LookAtLocation = CachedWorldLookAtLocation;
    } else {
        LookAtLocation = CachedNoTargetLookAtLocation + NoTargetOffset;
    }

    check(OutBoneTransforms.Num() == 0);

    const FBoneContainer& BoneContainer = Output.Pose.GetPose().GetBoneContainer();
    const FCompactPoseBoneIndex ModifyBoneIndex = BoneToModify.GetCompactPoseIndex(BoneContainer);
    FTransform ComponentBoneTransform = Output.Pose.GetComponentSpaceTransform(ModifyBoneIndex);

    FVector LookAtVector = LookAt_Axis.GetTransformedAxis(ComponentBoneTransform);
    FVector LookUpVector = LookUp_Axis.GetTransformedAxis(ComponentBoneTransform);
    FTransform TargetTransform = LookAtTarget.GetTargetTransform(LookAtLocation, Output.Pose, Output.AnimInstanceProxy->GetComponentTransform());

    FVector TargetLocationInComponentSpace = TargetTransform.GetLocation();

    PreviousTargetLocation = CurrentLookAtLocation;
    CurrentTargetLocation = TargetLocationInComponentSpace;

    if (InterpolationSpeed > 0.f) {
        CurrentLookAtLocation = FMath::VInterpTo(PreviousTargetLocation, CurrentTargetLocation, DeltaTime, InterpolationSpeed);
    } else {
        CurrentLookAtLocation = CurrentTargetLocation;
    }

    CachedCurrentTargetLocation = CurrentTargetLocation;

    FQuat DeltaRotation = AnimationCore::SolveAim(ComponentBoneTransform, CurrentLookAtLocation, LookAtVector, bUseLookUpAxis, LookUpVector, LookAtClamp);

    ModifyPoseFromDeltaRotation(Output, OutBoneTransforms, ComponentBoneTransform, DeltaRotation);

    OutBoneTransforms.Sort(FCompareBoneTransformIndex());

    TRACE_ANIM_NODE_VALUE(Output, TEXT("Bone"), BoneToModify.BoneName);
    TRACE_ANIM_NODE_VALUE(Output, TEXT("Look At Target"), LookAtTarget.HasValidSetup() ? LookAtTarget.GetTargetSetup() : NAME_None);
    TRACE_ANIM_NODE_VALUE(Output, TEXT("Look At Location"), LookAtLocation);
    TRACE_ANIM_NODE_VALUE(Output, TEXT("Target Location"), CachedCurrentTargetLocation);
}

bool FAnimNode_ACFLookAtTarget::IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones)
{
    // if both bones are valid
    return BoneToModify.IsValidToEvaluate(RequiredBones);
}

void FAnimNode_ACFLookAtTarget::InitializeBoneReferences(const FBoneContainer& RequiredBones)
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(InitializeBoneReferences)
    BoneToModify.Initialize(RequiredBones);
}

void FAnimNode_ACFLookAtTarget::ModifyPoseFromDeltaRotation(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms, FTransform& InOutBoneToModifyTransform, const FQuat& DeltaRotation)
{
    InOutBoneToModifyTransform.SetRotation(DeltaRotation * InOutBoneToModifyTransform.GetRotation());
    const FCompactPoseBoneIndex BoneToModifyIndex = BoneToModify.GetCompactPoseIndex(Output.Pose.GetPose().GetBoneContainer());
    OutBoneTransforms.Add(FBoneTransform(BoneToModifyIndex, InOutBoneToModifyTransform));
}
