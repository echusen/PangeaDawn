// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "Animation/ACFOverlayLayer.h"

UACFOverlayLayer::UACFOverlayLayer()
{
}

void UACFOverlayLayer::SetMovStance(const FGameplayTag& inOverlay)
{
    if (LocomotionStancesOverlays.Contains(inOverlay)) {
        currentOverlayTag = inOverlay;
        currentOverlay = *(LocomotionStancesOverlays.Find(currentOverlayTag));
    } else {
        currentOverlayTag = FGameplayTag();
        currentOverlay = IdleOverlay;
    }
    targetBlendAlpha = currentOverlay.BlendAlpha;
    targetBlendTime = currentOverlay.BlendTime;
    bHasActiveStance = currentOverlayTag != FGameplayTag();
}
