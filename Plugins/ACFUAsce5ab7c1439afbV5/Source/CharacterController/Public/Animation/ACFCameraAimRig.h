// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/BoneReference.h"
#include "Animation/BoneSocketReference.h"
#include "Engine/DataAsset.h"
#include "Interfaces/Interface_BoneReferenceSkeletonProvider.h"
#include "ACFCameraAimRig.generated.h"

class USkeleton;

/** One bone of the aim chain and how much of the view rotation it takes on each axis. */
USTRUCT()
struct FACFAimChainBone {
    GENERATED_BODY()

    /** Bone of the chain. Order is irrelevant: the chain is sorted root to tip at runtime. */
    UPROPERTY(EditAnywhere, Category = ACF)
    FBoneReference Bone;

    /** Share of the total pitch taken by this bone. Weights are normalized, so only their ratio matters. */
    UPROPERTY(EditAnywhere, Category = ACF, meta = (ClampMin = 0.0, UIMin = 0.0))
    float PitchWeight = 1.f;

    /** Share of the total yaw taken by this bone. Weights are normalized, so only their ratio matters. */
    UPROPERTY(EditAnywhere, Category = ACF, meta = (ClampMin = 0.0, UIMin = 0.0))
    float YawWeight = 1.f;

    /** Share of the total roll taken by this bone. Usually 0: the view frame has no meaningful roll. */
    UPROPERTY(EditAnywhere, Category = ACF, meta = (ClampMin = 0.0, UIMin = 0.0))
    float RollWeight = 0.f;
};

/**
 * Bones the ACF Camera Aim node drives, and where they have to end up relative to the view frame.
 * One asset per weapon hold: a bow, a two handed sword and a torch all want a different socket
 * offset and usually a different lock bone, and the node takes this as a pin so the overlay layer
 * can swap it at runtime the same way UACFBoneBlendMask is swapped on the blend mask node.
 */
UCLASS(BlueprintType)
class CHARACTERCONTROLLER_API UACFCameraAimRig : public UDataAsset, public IBoneReferenceSkeletonProvider {
    GENERATED_BODY()

public:
    /** Skeleton used to populate the bone pickers below. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = ACF)
    TObjectPtr<USkeleton> Skeleton;

    /**
     * Bones the view rotation is spread over, from the root of the spine to the chest. Weights are
     * normalized, so { spine_01 = 0.2, spine_02 = 0.3, spine_03 = 0.5 } and { 2, 3, 5 } are the same
     * chain. The accumulated rotation at the tip is always the full view delta, whatever the weights.
     */
    UPROPERTY(EditAnywhere, Category = "ACF|Aim Chain")
    TArray<FACFAimChainBone> AimChain;

    /** Frame that has to sit in front of the camera: the weapon hold socket, or the hand bone itself. */
    UPROPERTY(EditAnywhere, Category = "ACF|Socket Lock")
    FBoneSocketTarget AimSocket;

    /**
     * Bone actually corrected to place the socket. It must be an ancestor of the aim socket and must
     * not sit above the tip of the aim chain - usually it is the chain tip itself (spine_03).
     * Everything under it (arms, hands, weapon) follows rigidly, which is what makes the hold stable.
     */
    UPROPERTY(EditAnywhere, Category = "ACF|Socket Lock")
    FBoneReference LockBone;

    /**
     * How the socket has to be oriented relative to the view direction. Zero means the socket points
     * exactly where the view does; a bow held across the body wants a few degrees of yaw here.
     * Only the orientation is solved: the node follows the control rotation, which carries no
     * position, so the bone is never displaced and the body keeps its animated placement.
     */
    UPROPERTY(EditAnywhere, Category = "ACF|Socket Lock")
    FRotator SocketRotationOffset = FRotator::ZeroRotator;

    /** Bone the camera conceptually sits on (head, or a dedicated anchor bone). Leave empty to skip stabilization. */
    UPROPERTY(EditAnywhere, Category = "ACF|Anchor Stabilization")
    FBoneReference ViewAnchorBone;

    /** How the anchor bone has to be oriented relative to the view direction. */
    UPROPERTY(EditAnywhere, Category = "ACF|Anchor Stabilization")
    FRotator AnchorRotationOffset = FRotator::ZeroRotator;

    // IBoneReferenceSkeletonProvider
    virtual USkeleton* GetSkeleton(bool& bInvalidSkeletonIsError, const IPropertyHandle* PropertyHandle) override;
};
