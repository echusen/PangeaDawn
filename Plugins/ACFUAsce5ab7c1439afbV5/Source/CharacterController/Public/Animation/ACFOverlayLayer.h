// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "ACFBaseOverlayLayer.h"
#include "ACFCCTypes.h"
#include "Animation/AimOffsetBlendSpace.h"
#include "CoreMinimal.h"

#include "ACFOverlayLayer.generated.h"

/**
 * Legacy overlay anim layer: plain per-instance additive overlay, one data-only Blueprint child
 * per overlay tag. Configure IdleOverlay / LocomotionStancesOverlays with the animation to apply.
 * For mask-driven overlays (ACFBlendByMask node), see UACFBlendMaskOverlayLayer instead.
 */
class UAnimSequence;
UCLASS()
class CHARACTERCONTROLLER_API UACFOverlayLayer : public UACFBaseOverlayLayer {
    GENERATED_BODY()

public:
    UACFOverlayLayer();

protected:
    /*AimOffset Blendpsace applied to have the character point to the target or the camera direction*/
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF|AimOffset")
    UAimOffsetBlendSpace* AimOffset;

    /*Alpha of the AimOffset Blendpsace*/
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF|AimOffset")
    float AimOffsetAlpha = 1.0f;

    /*Overlay to be applied when in Idle*/
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF|Overlay")
    FOverlayConfig IdleOverlay;

    /*List of all the overlays for every Locomotion Stance*/
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (Categories = "Stances"), Category = "ACF|Overlay")
    TMap<FGameplayTag, FOverlayConfig> LocomotionStancesOverlays;

    UPROPERTY(BlueprintReadOnly, Category = ACF)
    FOverlayConfig currentOverlay;

    virtual void SetMovStance(const FGameplayTag& inOverlay) override;
};
