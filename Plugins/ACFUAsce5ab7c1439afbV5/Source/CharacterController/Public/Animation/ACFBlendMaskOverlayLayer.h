// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "ACFBaseOverlayLayer.h"
#include "ACFAnimTypes.h"
#include "ACFCCTypes.h"
#include "Animation/AimOffsetBlendSpace.h"
#include "Animation/AnimNode_ACFBlendByMask.h"
#include "CoreMinimal.h"

#include "ACFBlendMaskOverlayLayer.generated.h"

class UACFAnimInstance;
class UACFBoneBlendMask;
class UAnimSequence;

/**
 * Overlay anim layer driven by the ACFBlendByMask AnimGraph node. One data-only Blueprint child
 * per overlay tag (same pattern as Movesets and the legacy UACFOverlayLayer): configure
 * LocomotionStancesOverlays with the animation, blend mask and blend mode for each locomotion
 * stance. When no more specific stance tag matches (no loc stance active, or an active stance
 * with no entry configured), falls back to Stances.Idle (ACF::IdleTag) first, then to the root
 * "Stances" tag (ACF::StancesTag) - deeper stance tags (Stances.Aiming, Stances.Blocking, ...)
 * override both, the same inheritance model UACFBoneBlendMask uses for its branch points.
 * Resolves the active entry every frame and exposes it as Current* read-only variables for the
 * AnimGraph (ACFBlendByMask node).
 *
 * While the owner is in first person, FirstPersonLocomotionStancesOverlays is used instead, with the
 * same fallback rules. The two maps never fall back on each other, and AimOffsetAlpha is always 0 in
 * first person, blending over FirstPersonAimOffsetBlendTime when the perspective changes.
 *
 * Everything a stance change touches blends over the new entry's Overlay.BlendTime, exposed as
 * CurrentOverlayBlendTime: OverlayBlendAlfa, AimOffsetAlpha, and in the AnimGraph the pose and mask
 * blends that pin should feed.
 */
UCLASS()
class CHARACTERCONTROLLER_API UACFBlendMaskOverlayLayer : public UACFBaseOverlayLayer {
    GENERATED_BODY()

public:
    UACFBlendMaskOverlayLayer();

protected:
    /*Overlay applied per locomotion stance. Falls back to the Stances.Idle entry, then to the
      root "Stances" entry, when no more specific stance tag is active or configured.
      Locked to the Stances tag tree so no other root tags show up in the picker.*/
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (Categories = "Stances"), Category = "ACF|Overlay")
    TMap<FGameplayTag, FACFOverlayAnimationData> LocomotionStancesOverlays;

    /*First person counterpart of LocomotionStancesOverlays, used instead of it while the owner is in
      first person, with the same Stances.Idle and root "Stances" fallbacks. It never falls back to the
      third person map: a stance with no entry here has no overlay in first person.*/
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (Categories = "Stances"), Category = "ACF|Overlay|First Person")
    TMap<FGameplayTag, FACFFirstPersonOverlayAnimationData> FirstPersonLocomotionStancesOverlays;

    /** Seconds AimOffsetAlpha takes to blend out on entering first person, and back in on leaving it. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = 0.0, ForceUnits = "s"), Category = "ACF|Overlay|First Person")
    float FirstPersonAimOffsetBlendTime = 0.2f;

    /** True while the owner is in first person, so the first person map drives the overlay. */
    UPROPERTY(BlueprintReadOnly, Category = "ACF|Overlay|First Person")
    bool bIsInFirstPerson = false;

    /** Resolved overlay animation for the current locomotion stance. */
    UPROPERTY(BlueprintReadOnly, Category = "ACF|Overlay")
    TObjectPtr<UAnimSequence> CurrentOverlayAnim;

    /** Blend time for the current entry: feed it into the ACFBlendByMask node's BlendTime pin and into whatever
     * blends the overlay animation swap (e.g. Set Sequence With Inertial Blending), so every blend of a stance
     * change takes the same time as OverlayBlendAlfa and AimOffsetAlpha. */
    UPROPERTY(BlueprintReadOnly, Category = "ACF|Overlay")
    float CurrentOverlayBlendTime = 0.3f;

    /** Blend mask of the current entry (idle or active stance). Null when the entry has no animation. */
    UPROPERTY(BlueprintReadOnly, Category = "ACF|Overlay")
    TObjectPtr<UACFBoneBlendMask> CurrentBlendMask;

    UPROPERTY(BlueprintReadOnly, Category = "ACF|Overlay")
    EACFMaskBlendMode CurrentBlendMode = EACFMaskBlendMode::DynamicAdditive;

    UPROPERTY(BlueprintReadOnly, Category = "ACF|AimOffset")
    TObjectPtr<UAimOffsetBlendSpace> AimOffset;

    /** Always 0 in first person, and 0 for an entry without an aim offset, whose AimOffset then keeps the previous
     * blendspace so it can blend out. Blends over the entry's blend time between stances, and over
     * FirstPersonAimOffsetBlendTime when the perspective changes. */
    UPROPERTY(BlueprintReadOnly, Category = "ACF|AimOffset")
    float AimOffsetAlpha = 1.f;

    /** Rig of the current entry: feed it into the ACFCameraAim AnimGraph node's Rig pin, so each
     * weapon hold gets its own chain, socket offset and lock mode. */
    UPROPERTY(BlueprintReadOnly, Category = "ACF|AimOffset")
    TObjectPtr<class UACFCameraAimRig> CurrentCameraAimRig;

    UPROPERTY(BlueprintReadOnly, Category = "ACF|Overlay")
    FACFOverlayAnimationData currentOverlay;

    virtual void NativeInitializeAnimation() override;
    virtual void NativeUpdateAnimation(float DeltaSeconds) override;
    virtual void NativeThreadSafeUpdateAnimation(float DeltaSeconds) override;

    virtual void SetMovStance(const FGameplayTag& inOverlay) override;

private:
    void RefreshGraphVariables();

    /** Game Thread. The first person state is read off the owning mesh's main anim instance. */
    void ResolveOwnerAnimInstance();

    /** Starts blending AimOffsetAlpha to a new target over inBlendTime, from wherever it is. Same target: no restart. */
    void SetTargetAimOffsetAlpha(float inTarget, float inBlendTime);

    void UpdateAimOffsetBlend(float DeltaSeconds);

    UPROPERTY(Transient)
    TObjectPtr<UACFAnimInstance> OwnerAnimInstance;

    float TargetAimOffsetAlpha = 1.f;
    float AimOffsetBlendFrom = 1.f;
    float AimOffsetBlendElapsed = 0.f;
    float AimOffsetBlendDuration = 0.f;
    bool bBlendingAimOffset = false;

    /** The next stance resolve came from a perspective change, so the aim offset uses FirstPersonAimOffsetBlendTime. */
    bool bPerspectiveChangePending = false;

    /** The next stance resolve is the first one, so the aim offset starts where it belongs instead of blending. */
    bool bSnapAimOffset = false;
};
  