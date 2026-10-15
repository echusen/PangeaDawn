// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "ACFAnimLayer.h"
#include "CoreMinimal.h"
#include <GameplayTagContainer.h>

#include "ACFBaseOverlayLayer.generated.h"

/**
 * Shared runtime plumbing for overlay anim layers: every frame polls the owning
 * UACFCharacterMovementComponent's locomotion stance, defers to SetMovStance() to resolve the
 * stance-specific config, and blends OverlayBlendAlfa linearly towards the resolved target over
 * the resolved entry's blend time, so it lands together with the pose and mask blends.
 *
 * Concrete overlay layers (UACFOverlayLayer, UACFBlendMaskOverlayLayer) configure their own data
 * directly on themselves - one data-only Blueprint child per overlay, same pattern as Movesets -
 * because they use different config struct types (FOverlayConfig vs FACFOverlayAnimationData) and
 * can't share a single config table here.
 */
UCLASS()
class CHARACTERCONTROLLER_API UACFBaseOverlayLayer : public UACFAnimLayer {
    GENERATED_BODY()

public:
    UACFBaseOverlayLayer();

protected:
    UPROPERTY(BlueprintReadOnly, Category = ACF)
    FGameplayTag currentOverlayTag;

    UPROPERTY(BlueprintReadOnly, Category = ACF)
    bool bHasActiveStance = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF|Overlay")
    float OverlayBlendAlfa = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = ACF)
    TObjectPtr<class UACFCharacterMovementComponent> MovementComp;

    virtual void NativeInitializeAnimation() override;
    virtual void NativeThreadSafeUpdateAnimation(float DeltaSeconds) override;
    virtual void OnActivated_Implementation() override;

    /** Resolves the config for the given locomotion stance tag: overrides must update
     * currentOverlayTag, bHasActiveStance, targetBlendAlpha and targetBlendTime (plus whatever
     * Current* data the AnimGraph of that concrete layer exposes). Base has no config to resolve. */
    UFUNCTION(BlueprintCallable, Category = ACF)
    virtual void SetMovStance(const FGameplayTag& inOverlay);

    void SetReferences();

    UPROPERTY(BlueprintReadOnly, Category = ACF)
    float targetBlendAlpha = 0.f;

    /** Seconds OverlayBlendAlfa takes to reach a new targetBlendAlpha: the resolved entry's BlendTime. 0 snaps. */
    UPROPERTY(BlueprintReadOnly, Category = ACF)
    float targetBlendTime = 0.3f;

    bool bForceStanceRefresh = false;

private:
    /** Moves OverlayBlendAlfa towards targetBlendAlpha, restarting the blend whenever the target changes. */
    void UpdateOverlayBlendAlpha(float DeltaSeconds);

    /** Target the running blend heads for. Negative until the first one, so it always starts on initialization. */
    float BlendAlphaTarget = -1.f;

    /** Alpha per second, fixed when the blend starts so it covers the distance in exactly targetBlendTime. */
    float BlendAlphaRate = 0.f;
};
