// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "ACFFreeClimbRigProfile.h"

#include "Animation/Skeleton.h"
#include "Engine/SkeletalMesh.h"

namespace
{
    void InitializeLimb(
        FACFFreeClimbLimbProfile& Limb,
        const TCHAR* EffectorBone,
        const TCHAR* IKBone,
        const TCHAR* ProbeOriginBone,
        const TCHAR* ContactCurve)
    {
        Limb.EffectorBone = EffectorBone;
        Limb.IKBone = IKBone;
        Limb.ProbeOriginBone = ProbeOriginBone;
        Limb.ContactCurveName = ContactCurve;
    }
}

UACFFreeClimbRigProfile::UACFFreeClimbRigProfile()
{
    InitializeLimb(LeftHand, TEXT("hand_l"), TEXT("ik_hand_l"), TEXT("hand_l"), TEXT("C_LeftHand"));
    InitializeLimb(RightHand, TEXT("hand_r"), TEXT("ik_hand_r"), TEXT("hand_r"), TEXT("C_RightHand"));
    InitializeLimb(LeftFoot, TEXT("foot_l"), TEXT("ik_foot_l"), TEXT("foot_l"), TEXT("C_LeftFoot"));
    InitializeLimb(RightFoot, TEXT("foot_r"), TEXT("ik_foot_r"), TEXT("foot_r"), TEXT("C_RightFoot"));
}

const FACFFreeClimbLimbProfile& UACFFreeClimbRigProfile::GetLimbProfile(EACFFreeClimbLimb Limb) const
{
    switch (Limb)
    {
    case EACFFreeClimbLimb::LeftHand:
        return LeftHand;
    case EACFFreeClimbLimb::RightHand:
        return RightHand;
    case EACFFreeClimbLimb::LeftFoot:
        return LeftFoot;
    case EACFFreeClimbLimb::RightFoot:
        return RightFoot;
    default:
        ensureMsgf(false, TEXT("Invalid free climb limb value."));
        return LeftHand;
    }
}

bool UACFFreeClimbRigProfile::IsCompatibleWith(const USkeletalMesh* SkeletalMesh, FString* OutReason) const
{
    if (!SkeletalMesh)
    {
        if (OutReason) *OutReason = TEXT("Skeletal mesh is missing.");
        return false;
    }

    const USkeleton* MeshSkeleton = SkeletalMesh->GetSkeleton();
    if (!MeshSkeleton)
    {
        if (OutReason) *OutReason = TEXT("Skeletal mesh has no skeleton.");
        return false;
    }

    // TargetSkeleton is authoring metadata (editor preview / bone pickers) only.
    // Compatibility is decided by the bone validation below, so one profile can drive any
    // skeleton that exposes the configured effector / IK / probe bones - which is what lets
    // the same Manny profile serve ACF_UE5Manny, SK_UEFN_Mannequin and the sample skeletons.

    const FReferenceSkeleton& RefSkeleton = SkeletalMesh->GetRefSkeleton();
    const auto ValidateBone = [&RefSkeleton, OutReason](FName BoneName, const TCHAR* Label)
    {
        if (!BoneName.IsNone() && RefSkeleton.FindBoneIndex(BoneName) != INDEX_NONE)
        {
            return true;
        }

        if (OutReason)
        {
            *OutReason = BoneName.IsNone()
                ? FString::Printf(TEXT("%s bone is not configured."), Label)
                : FString::Printf(TEXT("%s bone '%s' was not found."), Label, *BoneName.ToString());
        }
        return false;
    };

    for (uint8 LimbValue = 0; LimbValue < static_cast<uint8>(EACFFreeClimbLimb::Count); ++LimbValue)
    {
        const FACFFreeClimbLimbProfile& Limb = GetLimbProfile(static_cast<EACFFreeClimbLimb>(LimbValue));
        if (!Limb.bEnabled) continue;

        if (!ValidateBone(Limb.EffectorBone, TEXT("Effector")) ||
            !ValidateBone(Limb.IKBone, TEXT("IK")) ||
            !ValidateBone(Limb.ProbeOriginBone, TEXT("Probe origin")))
        {
            return false;
        }

        if (Limb.MinReach > Limb.MaxReach)
        {
            if (OutReason) *OutReason = TEXT("A limb profile has MinReach greater than MaxReach.");
            return false;
        }

        if (Limb.ProbeLocalDirection.IsNearlyZero() || Limb.ContactCurveName.IsNone())
        {
            if (OutReason) *OutReason = TEXT("An enabled limb has no probe direction or contact curve configured.");
            return false;
        }
    }

    if (Curves.TraceThreshold > Curves.LockThreshold)
    {
        if (OutReason) *OutReason = TEXT("TraceThreshold must not be greater than LockThreshold.");
        return false;
    }

    return true;
}
