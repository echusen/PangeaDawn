// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "AnimGraphNode_ACFCameraAim.h"

#include "Animation/ACFCameraAimRig.h"
#include "Animation/Skeleton.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Kismet2/CompilerResultsLog.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimGraphNode_ACFCameraAim)

#define LOCTEXT_NAMESPACE "AnimGraphNode_ACFCameraAim"

namespace {

/** Number of parents between a bone and the root, used to tell which chain bone is the tip. */
int32 GetBoneDepth(const FReferenceSkeleton& inRefSkeleton, int32 inBoneIndex)
{
    int32 depth = 0;
    for (int32 index = inBoneIndex; index != INDEX_NONE; index = inRefSkeleton.GetParentIndex(index)) {
        ++depth;
    }
    return depth;
}

bool IsBoneDescendantOf(const FReferenceSkeleton& inRefSkeleton, int32 inBoneIndex, int32 inAncestorIndex)
{
    for (int32 index = inBoneIndex; index != INDEX_NONE; index = inRefSkeleton.GetParentIndex(index)) {
        if (index == inAncestorIndex) {
            return true;
        }
    }
    return false;
}

} // namespace

UAnimGraphNode_ACFCameraAim::UAnimGraphNode_ACFCameraAim(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
}

FText UAnimGraphNode_ACFCameraAim::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
    return LOCTEXT("NodeTitle", "ACF Camera Aim");
}

FText UAnimGraphNode_ACFCameraAim::GetControllerDescription() const
{
    return LOCTEXT("ControllerDescription", "ACF Camera Aim");
}

FText UAnimGraphNode_ACFCameraAim::GetTooltipText() const
{
    return LOCTEXT("NodeTooltip",
        "Orients the upper body onto a view rotation analytically, normally the control rotation,\n"
        "with no aim offset blendspace involved. Three stages: the rotation is spread over the spine\n"
        "chain, the lock bone is solved so the aim socket points along the view, and the view anchor\n"
        "bone is stabilized. Bones are turned, never displaced.\n"
        "The Rig pin holds the bones and offsets, so each weapon hold can bring its own; bEnabled\n"
        "switches the whole thing on and off at runtime.");
}

FString UAnimGraphNode_ACFCameraAim::GetNodeCategory() const
{
    return TEXT("Animation|ACF");
}

void UAnimGraphNode_ACFCameraAim::ValidateAnimNodeDuringCompilation(USkeleton* ForSkeleton, FCompilerResultsLog& MessageLog)
{
    Super::ValidateAnimNodeDuringCompilation(ForSkeleton, MessageLog);

    // The rig is a pin, so leaving it None at compile time is legitimate: only the default value
    // assigned on the node can be validated here.
    const UACFCameraAimRig* rig = Node.Rig;
    if (!rig || !ForSkeleton) {
        return;
    }

    const FString rigName = rig->GetName();

    if (!rig->Skeleton) {
        MessageLog.Warning(*FText::Format(LOCTEXT("MissingRigSkeleton", "@@ Camera Aim Rig '{0}' has no Skeleton assigned: its bones will use a text field instead of the bone picker."),
            FText::FromString(rigName))
                                .ToString(),
            this);
    } else if (rig->Skeleton != ForSkeleton) {
        MessageLog.Warning(*FText::Format(LOCTEXT("RigSkeletonMismatch", "@@ Camera Aim Rig '{0}' uses skeleton '{1}' but this AnimBP targets '{2}'. Bones are resolved by name at runtime, so this may still work if the hierarchies match."),
            FText::FromString(rigName),
            FText::FromString(rig->Skeleton->GetName()),
            FText::FromString(ForSkeleton->GetName()))
                                .ToString(),
            this);
    }

    const FReferenceSkeleton& refSkeleton = ForSkeleton->GetReferenceSkeleton();

    auto validateBone = [&](const FName& inBoneName, const FText& inRole) -> int32 {
        if (inBoneName == NAME_None) {
            return INDEX_NONE;
        }

        const int32 boneIndex = refSkeleton.FindBoneIndex(inBoneName);
        if (boneIndex == INDEX_NONE) {
            MessageLog.Warning(*FText::Format(LOCTEXT("MissingBone", "@@ {0} '{1}' from Camera Aim Rig '{2}' does not exist on the target skeleton."),
                inRole, FText::FromName(inBoneName), FText::FromString(rigName))
                                    .ToString(),
                this);
        }
        return boneIndex;
    };

    // Deepest chain bone: everything the socket lock and the stabilization touch has to sit at or
    // below it, because this node writes absolute component space transforms and a later stage
    // moving an ancestor would not carry into what an earlier one already wrote.
    int32 chainTipIndex = INDEX_NONE;
    int32 chainTipDepth = -1;
    for (const FACFAimChainBone& chainBone : rig->AimChain) {
        const int32 boneIndex = validateBone(chainBone.Bone.BoneName, LOCTEXT("RoleChainBone", "Aim chain bone"));
        if (boneIndex == INDEX_NONE) {
            continue;
        }

        const int32 depth = GetBoneDepth(refSkeleton, boneIndex);
        if (depth > chainTipDepth) {
            chainTipDepth = depth;
            chainTipIndex = boneIndex;
        }
    }

    const int32 lockBoneIndex = validateBone(rig->LockBone.BoneName, LOCTEXT("RoleLockBone", "Lock bone"));
    validateBone(rig->ViewAnchorBone.BoneName, LOCTEXT("RoleAnchorBone", "View anchor bone"));

    if (lockBoneIndex == INDEX_NONE) {
        return;
    }

    if (chainTipIndex != INDEX_NONE && !IsBoneDescendantOf(refSkeleton, lockBoneIndex, chainTipIndex)) {
        MessageLog.Warning(*FText::Format(LOCTEXT("LockBoneAboveChain", "@@ Lock bone '{0}' of Camera Aim Rig '{1}' sits above the tip of the aim chain ('{2}'). The socket lock would undo part of the chain: make the lock bone the chain tip, or one of its descendants."),
            FText::FromName(rig->LockBone.BoneName),
            FText::FromString(rigName),
            FText::FromName(refSkeleton.GetBoneName(chainTipIndex)))
                                .ToString(),
            this);
    }

    // The lock solve assumes the socket keeps a fixed transform relative to the lock bone. That only
    // holds while the socket hangs off the lock bone, with nothing this node touches in between.
    FName socketOwningBoneName = NAME_None;
    if (rig->AimSocket.bUseSocket) {
        if (const USkeletalMeshSocket* socket = ForSkeleton->FindSocket(rig->AimSocket.SocketReference.SocketName)) {
            socketOwningBoneName = socket->BoneName;
        }
        // A socket authored on the skeletal mesh rather than on the skeleton cannot be resolved
        // here: leave it to the runtime rather than warning about something that is likely fine.
    } else {
        socketOwningBoneName = rig->AimSocket.BoneReference.BoneName;
    }

    if (socketOwningBoneName == NAME_None) {
        return;
    }

    const int32 socketBoneIndex = refSkeleton.FindBoneIndex(socketOwningBoneName);
    if (socketBoneIndex == INDEX_NONE) {
        MessageLog.Warning(*FText::Format(LOCTEXT("MissingSocketBone", "@@ Aim socket of Camera Aim Rig '{0}' resolves to bone '{1}', which does not exist on the target skeleton."),
            FText::FromString(rigName), FText::FromName(socketOwningBoneName))
                                .ToString(),
            this);
        return;
    }

    if (!IsBoneDescendantOf(refSkeleton, socketBoneIndex, lockBoneIndex)) {
        MessageLog.Warning(*FText::Format(LOCTEXT("SocketNotUnderLockBone", "@@ Aim socket of Camera Aim Rig '{0}' hangs off '{1}', which is not a descendant of lock bone '{2}'. The socket lock will drift instead of holding."),
            FText::FromString(rigName),
            FText::FromName(socketOwningBoneName),
            FText::FromName(rig->LockBone.BoneName))
                                .ToString(),
            this);
    }
}

void UAnimGraphNode_ACFCameraAim::PreloadRequiredAssets()
{
    if (Node.Rig) {
        PreloadObject(Node.Rig);
    }

    Super::PreloadRequiredAssets();
}

#undef LOCTEXT_NAMESPACE
