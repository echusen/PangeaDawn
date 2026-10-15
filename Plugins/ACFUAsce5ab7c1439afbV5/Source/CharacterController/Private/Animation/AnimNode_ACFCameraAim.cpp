// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "Animation/AnimNode_ACFCameraAim.h"

#include "Animation/ACFAnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimStats.h"
#include "Animation/AnimTrace.h"
#include "BonePose.h"
#include "Components/SkeletalMeshComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimNode_ACFCameraAim)

FAnimNode_ACFCameraAim::FAnimNode_ACFCameraAim()
{
    // The node exists to be switched on and off at runtime, so it defaults to the bool alpha:
    // that exposes the bEnabled pin and gives a blend in / blend out through AlphaBoolBlend.
    AlphaInputType = EAnimAlphaInputType::Bool;
    bAlphaBoolEnabled = false;

    // Only the locally controlled character needs this, and it is never worth running on a
    // character that is already down to a lower LOD.
    LODThreshold = 0;
}

void FAnimNode_ACFCameraAim::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(Initialize_AnyThread)
    FAnimNode_SkeletalControlBase::Initialize_AnyThread(Context);

    CopyReferencesFromRig(Context.AnimInstanceProxy);

    CachedChain.Reset();
    smoothedAimDelta = FRotator::ZeroRotator;
    bResetInterpolation = true;
    bHasModifiedBone = false;
    lastUpdateCounter = INDEX_NONE;
    cachedBonesSerialNumber = 0;
}

void FAnimNode_ACFCameraAim::CopyReferencesFromRig(const FAnimInstanceProxy* inProxy)
{
    cachedRig = Rig;

    const UACFCameraAimRig* rig = Rig;
    if (!rig) {
        resolvedAimSocket = FBoneSocketTarget();
        resolvedLockBone = FBoneReference();
        resolvedAnchorBone = FBoneReference();
        return;
    }

    resolvedAimSocket = rig->AimSocket;
    resolvedLockBone = rig->LockBone;
    resolvedAnchorBone = rig->ViewAnchorBone;

    // Resolves the socket's local transform and the bone it hangs off. Has to run before
    // InitializeBoneReferences, which turns that mesh bone index into a compact pose one.
    if (inProxy) {
        resolvedAimSocket.Initialize(inProxy);
    }
}

void FAnimNode_ACFCameraAim::InitializeBoneReferences(const FBoneContainer& RequiredBones)
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(InitializeBoneReferences)

    CachedChain.Reset();
    cachedBonesSerialNumber = RequiredBones.GetSerialNumber();
    bResetInterpolation = true;

    resolvedLockBone.Initialize(RequiredBones);
    resolvedAnchorBone.Initialize(RequiredBones);
    resolvedAimSocket.InitializeBoneReferences(RequiredBones);

    const UACFCameraAimRig* rig = Rig;
    if (!rig || rig->AimChain.IsEmpty()) {
        return;
    }

    // Bones are resolved by name against the current bone container instead of through the rig's
    // own FBoneReferences: the rig is shared between characters and LODs, its cached indices are not.
    CachedChain.Reserve(rig->AimChain.Num());
    float totalPitchWeight = 0.f;
    float totalYawWeight = 0.f;
    float totalRollWeight = 0.f;

    for (const FACFAimChainBone& chainBone : rig->AimChain) {
        if (chainBone.Bone.BoneName == NAME_None) {
            continue;
        }

        const int32 meshBoneIndex = RequiredBones.GetPoseBoneIndexForBoneName(chainBone.Bone.BoneName);
        if (meshBoneIndex == INDEX_NONE) {
            continue;
        }

        // Bones dropped by the current LOD simply leave the chain: the weights below are
        // renormalized over whatever is left, so the tip still ends up on the full delta.
        const FCompactPoseBoneIndex compactIndex = RequiredBones.MakeCompactPoseIndex(FMeshPoseBoneIndex(meshBoneIndex));
        if (compactIndex.GetInt() == INDEX_NONE) {
            continue;
        }

        FACFResolvedAimChainBone& resolved = CachedChain.AddDefaulted_GetRef();
        resolved.BoneIndex = compactIndex;
        resolved.CumulativePitch = FMath::Max(chainBone.PitchWeight, 0.f);
        resolved.CumulativeYaw = FMath::Max(chainBone.YawWeight, 0.f);
        resolved.CumulativeRoll = FMath::Max(chainBone.RollWeight, 0.f);

        totalPitchWeight += resolved.CumulativePitch;
        totalYawWeight += resolved.CumulativeYaw;
        totalRollWeight += resolved.CumulativeRoll;
    }

    // Compact pose indices are ordered parents first, so this sorts the chain root to tip
    // whatever order the rig lists the bones in.
    CachedChain.Sort([](const FACFResolvedAimChainBone& lhs, const FACFResolvedAimChainBone& rhs) {
        return lhs.BoneIndex < rhs.BoneIndex;
    });

    // Turn the per bone shares into the fraction accumulated from the root down to each bone.
    float runningPitch = 0.f;
    float runningYaw = 0.f;
    float runningRoll = 0.f;
    for (FACFResolvedAimChainBone& resolved : CachedChain) {
        runningPitch += resolved.CumulativePitch;
        runningYaw += resolved.CumulativeYaw;
        runningRoll += resolved.CumulativeRoll;

        resolved.CumulativePitch = totalPitchWeight > 0.f ? runningPitch / totalPitchWeight : 0.f;
        resolved.CumulativeYaw = totalYawWeight > 0.f ? runningYaw / totalYawWeight : 0.f;
        resolved.CumulativeRoll = totalRollWeight > 0.f ? runningRoll / totalRollWeight : 0.f;
    }
}

void FAnimNode_ACFCameraAim::UpdateInternal(const FAnimationUpdateContext& Context)
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(UpdateInternal)
    FAnimNode_SkeletalControlBase::UpdateInternal(Context);

    lastUpdateDeltaTime = Context.GetDeltaTime();

    // UpdateInternal only runs while the node is relevant, so a gap in the update counter means
    // the node was off: whatever it had smoothed belongs to a different pose and must not be
    // interpolated from.
    const int16 updateCounter = Context.AnimInstanceProxy->GetUpdateCounter().Get();
    if (lastUpdateCounter == INDEX_NONE || updateCounter != lastUpdateCounter + 1) {
        bResetInterpolation = true;
    }
    lastUpdateCounter = updateCounter;

    // The rig is a pin, so it can change on any frame; the bone container changes with LOD.
    const FBoneContainer& requiredBones = Context.AnimInstanceProxy->GetRequiredBones();
    if (Rig != cachedRig.Get()) {
        CopyReferencesFromRig(Context.AnimInstanceProxy);
        InitializeBoneReferences(requiredBones);
    } else if (requiredBones.GetSerialNumber() != cachedBonesSerialNumber) {
        InitializeBoneReferences(requiredBones);
    }
}

bool FAnimNode_ACFCameraAim::IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones)
{
    if (!Rig) {
        return false;
    }

    const bool bChainUsable = bEnableAimChain && CachedChain.Num() > 0;
    const bool bLockUsable = bEnableSocketLock && resolvedLockBone.IsValidToEvaluate(RequiredBones) && resolvedAimSocket.IsValidToEvaluate(RequiredBones);
    const bool bAnchorUsable = bEnableAnchorStabilization && resolvedAnchorBone.IsValidToEvaluate(RequiredBones);

    return bChainUsable || bLockUsable || bAnchorUsable;
}

bool FAnimNode_ACFCameraAim::ResolveViewRotation(const FComponentSpacePoseContext& Output, FRotator& outViewWorld) const
{
    // An unconnected pin leaves the zero rotator behind, which is never a real view rotation.
    if (!ViewRotation.IsNearlyZero()) {
        outViewWorld = ViewRotation;
        return true;
    }

    // Fallback for graphs that don't wire the pin. It has to be the mesh's main anim instance and
    // not the proxy's: inside a linked moveset or overlay layer the proxy belongs to that layer,
    // which is not a UACFAnimInstance. The value itself is cached on the Game Thread.
    const USkeletalMeshComponent* meshComponent = Output.AnimInstanceProxy->GetSkelMeshComponent();
    const UACFAnimInstance* acfAnimInstance = meshComponent ? Cast<UACFAnimInstance>(meshComponent->GetAnimInstance()) : nullptr;
    if (!acfAnimInstance || !acfAnimInstance->IsCameraAimActive()) {
        return false;
    }

    outViewWorld = acfAnimInstance->GetCameraViewRotation();
    return true;
}

FTransform FAnimNode_ACFCameraAim::GetTransformAfterPreviousStages(const FTransform& inBoneOriginalCS) const
{
    if (!bHasModifiedBone) {
        return inBoneOriginalCS;
    }

    // Every bone the stages touch is at or below the previous one, so its transform relative to
    // the last modified bone is untouched and can be rebuilt on top of the new one.
    return inBoneOriginalCS.GetRelativeTransform(lastModifiedOriginalCS) * lastModifiedNewCS;
}

void FAnimNode_ACFCameraAim::AddOrReplaceBoneTransform(TArray<FBoneTransform>& outBoneTransforms, const FCompactPoseBoneIndex& inBoneIndex, const FTransform& inTransform)
{
    // Two entries for the same bone would make FCSPose write it twice and corrupt its bookkeeping.
    for (FBoneTransform& boneTransform : outBoneTransforms) {
        if (boneTransform.BoneIndex == inBoneIndex) {
            boneTransform.Transform = inTransform;
            return;
        }
    }

    outBoneTransforms.Add(FBoneTransform(inBoneIndex, inTransform));
}

void FAnimNode_ACFCameraAim::EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms)
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(EvaluateSkeletalControl_AnyThread)

    check(OutBoneTransforms.Num() == 0);

    bHasModifiedBone = false;

    const UACFCameraAimRig* rig = Rig;
    if (!rig) {
        return;
    }

    FRotator viewWorld;
    if (!ResolveViewRotation(Output, viewWorld)) {
        return;
    }

    const FTransform& componentToWorld = Output.AnimInstanceProxy->GetComponentTransform();
    const FTransform& actorToWorld = Output.AnimInstanceProxy->GetActorTransform();
    const FQuat componentRotation = componentToWorld.GetRotation();
    // A world space rotation expressed in component space. The node is handed a rotation and no
    // position, so this is all there is of the view: no bone is ever displaced by it.
    const FQuat viewRotationCS = (componentRotation.Inverse() * viewWorld.Quaternion()).GetNormalized();
    const FBoneContainer& boneContainer = Output.Pose.GetPose().GetBoneContainer();

    // ---- Aim chain ---- //
    if (bEnableAimChain && CachedChain.Num() > 0 && AimChainAlpha > ZERO_ANIMWEIGHT_THRESH) {
        FRotator aimDelta = (viewWorld - actorToWorld.Rotator()).GetNormalized();
        aimDelta.Pitch = FMath::Clamp(aimDelta.Pitch, PitchLimits.X, PitchLimits.Y);
        aimDelta.Yaw = FMath::Clamp(aimDelta.Yaw, YawLimits.X, YawLimits.Y);

        if (AimInterpSpeed > 0.f && !bResetInterpolation) {
            aimDelta = FMath::RInterpTo(smoothedAimDelta, aimDelta, lastUpdateDeltaTime, AimInterpSpeed);
        }
        smoothedAimDelta = aimDelta;

        aimDelta *= FMath::Clamp(AimChainAlpha, 0.f, 1.f);

        // The rotation is composed down the chain, not summed: rotations around different axes do
        // not commute, so scaling the delta per bone and applying it independently would land the
        // tip somewhere else than the view, more and more the steeper the pitch. Feeding each bone
        // the increment between its cumulative share and the previous one makes the increments
        // telescope, so the tip ends up on exactly the full delta whatever the weights are.
        FQuat previousTargetRotation = FQuat::Identity;
        FTransform previousOriginalCS = FTransform::Identity;
        FTransform previousNewCS = FTransform::Identity;

        for (const FACFResolvedAimChainBone& chainBone : CachedChain) {
            const FQuat targetRotation = FQuat(FRotator(
                aimDelta.Pitch * chainBone.CumulativePitch,
                aimDelta.Yaw * chainBone.CumulativeYaw,
                aimDelta.Roll * chainBone.CumulativeRoll));

            const FQuat incrementWorld = targetRotation * previousTargetRotation.Inverse();
            // A world space rotation acting on a component space transform is that same rotation
            // conjugated by the component's own rotation. Never assume the mesh's yaw offset:
            // it is authored per Blueprint.
            const FQuat incrementComponent = (componentRotation.Inverse() * incrementWorld * componentRotation).GetNormalized();
            previousTargetRotation = targetRotation;

            const FTransform originalCS = Output.Pose.GetComponentSpaceTransform(chainBone.BoneIndex);

            // Rebuilt from the previous chain bone so bones in between, which this node does not
            // touch, still carry their share of the correction down.
            FTransform newCS = originalCS.GetRelativeTransform(previousOriginalCS) * previousNewCS;
            newCS.SetRotation((incrementComponent * newCS.GetRotation()).GetNormalized());

            AddOrReplaceBoneTransform(OutBoneTransforms, chainBone.BoneIndex, newCS);

            previousOriginalCS = originalCS;
            previousNewCS = newCS;
        }

        lastModifiedOriginalCS = previousOriginalCS;
        lastModifiedNewCS = previousNewCS;
        bHasModifiedBone = true;
    }

    bResetInterpolation = false;

    // ---- Socket lock ---- //
    if (bEnableSocketLock && SocketLockAlpha > ZERO_ANIMWEIGHT_THRESH
        && resolvedLockBone.IsValidToEvaluate(boneContainer) && resolvedAimSocket.IsValidToEvaluate(boneContainer)) {

        const FCompactPoseBoneIndex lockBoneIndex = resolvedLockBone.GetCompactPoseIndex(boneContainer);
        const FTransform lockOriginalCS = Output.Pose.GetComponentSpaceTransform(lockBoneIndex);
        const FTransform socketOriginalCS = resolvedAimSocket.GetTargetTransform(FTransform::Identity, Output.Pose, componentToWorld);

        // Nothing between the lock bone and the socket is modified, so this stays constant and the
        // solve below is exact: SocketRotation = LockRotation * lockToSocket.
        const FQuat lockToSocket = lockOriginalCS.GetRotation().Inverse() * socketOriginalCS.GetRotation();
        const FQuat targetSocketRotationCS = viewRotationCS * rig->SocketRotationOffset.Quaternion();

        const FTransform lockAfterChain = GetTransformAfterPreviousStages(lockOriginalCS);
        // Solves LockRotation * lockToSocket == target for LockRotation. Only the orientation is
        // touched: the bone keeps the position the animation and the chain left it at.
        const FQuat lockSolvedRotation = (targetSocketRotationCS * lockToSocket.Inverse()).GetNormalized();

        FTransform lockFinal = lockAfterChain;
        lockFinal.SetRotation(FQuat::Slerp(lockAfterChain.GetRotation(), lockSolvedRotation, FMath::Clamp(SocketLockAlpha, 0.f, 1.f)).GetNormalized());

        AddOrReplaceBoneTransform(OutBoneTransforms, lockBoneIndex, lockFinal);

        lastModifiedOriginalCS = lockOriginalCS;
        lastModifiedNewCS = lockFinal;
        bHasModifiedBone = true;
    }

    // ---- Anchor stabilization ---- //
    if (bEnableAnchorStabilization && resolvedAnchorBone.IsValidToEvaluate(boneContainer)
        && AnchorRotationAlpha > ZERO_ANIMWEIGHT_THRESH) {

        const FCompactPoseBoneIndex anchorBoneIndex = resolvedAnchorBone.GetCompactPoseIndex(boneContainer);
        const FTransform anchorOriginalCS = Output.Pose.GetComponentSpaceTransform(anchorBoneIndex);
        const FTransform anchorAfterPreviousStages = GetTransformAfterPreviousStages(anchorOriginalCS);
        const FQuat anchorTargetRotationCS = (viewRotationCS * rig->AnchorRotationOffset.Quaternion()).GetNormalized();

        // Only the rotation is stabilized: killing the animated head rotation is what stops the view
        // from being shaken, and the animated translation left in reads as life rather than noise.
        FTransform anchorFinal = anchorAfterPreviousStages;
        anchorFinal.SetRotation(FQuat::Slerp(anchorAfterPreviousStages.GetRotation(), anchorTargetRotationCS, FMath::Clamp(AnchorRotationAlpha, 0.f, 1.f)).GetNormalized());

        AddOrReplaceBoneTransform(OutBoneTransforms, anchorBoneIndex, anchorFinal);

        lastModifiedOriginalCS = anchorOriginalCS;
        lastModifiedNewCS = anchorFinal;
        bHasModifiedBone = true;
    }

    // FCSPose requires the bones it is handed to be in increasing index order.
    OutBoneTransforms.Sort(FCompareBoneTransformIndex());

    TRACE_ANIM_NODE_VALUE(Output, TEXT("Rig"), rig);
    TRACE_ANIM_NODE_VALUE(Output, TEXT("Chain Bones"), CachedChain.Num());
    TRACE_ANIM_NODE_VALUE(Output, TEXT("View Rotation"), viewWorld);
}

void FAnimNode_ACFCameraAim::GatherDebugData(FNodeDebugData& DebugData)
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(GatherDebugData)

    FString debugLine = DebugData.GetNodeName(this);
    debugLine += FString::Printf(TEXT("(Rig: %s, Chain: %d bones, Alpha: %.2f)"), *GetNameSafe(Rig), CachedChain.Num(), ActualAlpha);
    DebugData.AddDebugItem(debugLine);

    ComponentPose.GatherDebugData(DebugData.BranchFlow(1.f));
}
