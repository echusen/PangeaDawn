// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "Animation/ACFBaseOverlayLayer.h"

#include "Components/ACFCharacterMovementComponent.h"
#include <GameFramework/Pawn.h>

UACFBaseOverlayLayer::UACFBaseOverlayLayer()
{
}

void UACFBaseOverlayLayer::SetReferences()
{
    const APawn* pawn = TryGetPawnOwner();
    if (pawn) {
        MovementComp = Cast<UACFCharacterMovementComponent>(pawn->GetMovementComponent());
        if (!MovementComp) {
            UE_LOG(LogTemp, Error, TEXT("Owner doesn't have ACFCharacterMovement Comp - UACFBaseOverlayLayer::SetReferences!!!!"));
        } else {
            SetMovStance(MovementComp->GetCurrentLocomotionStance());
        }
    }
}

void UACFBaseOverlayLayer::SetMovStance(const FGameplayTag& inOverlay)
{
    // Base has no config to resolve - concrete overlay layers override this.
}

void UACFBaseOverlayLayer::NativeInitializeAnimation()
{
    Super::NativeInitializeAnimation();
    SetReferences();
}

void UACFBaseOverlayLayer::NativeThreadSafeUpdateAnimation(float DeltaSeconds)
{
    Super::NativeThreadSafeUpdateAnimation(DeltaSeconds);

    if (!MovementComp) {
        SetReferences();
    } else {
        const FGameplayTag newOverlay = MovementComp->GetCurrentLocomotionStance();
        if (bForceStanceRefresh || newOverlay != currentOverlayTag) {
            SetMovStance(newOverlay);
            bForceStanceRefresh = false;
        }
    }

    UpdateOverlayBlendAlpha(DeltaSeconds);
}

void UACFBaseOverlayLayer::UpdateOverlayBlendAlpha(float DeltaSeconds)
{
    // A stance with no entry of its own is re-resolved every frame, so only a different target restarts the blend.
    if (!FMath::IsNearlyEqual(targetBlendAlpha, BlendAlphaTarget)) {
        BlendAlphaTarget = targetBlendAlpha;
        BlendAlphaRate = targetBlendTime > KINDA_SMALL_NUMBER ? FMath::Abs(targetBlendAlpha - OverlayBlendAlfa) / targetBlendTime : 0.f;
    }

    // Linear over the entry's blend time, like the mask weights and the pose cross-fade it is paired with.
    OverlayBlendAlfa = BlendAlphaRate > 0.f
        ? FMath::FInterpConstantTo(OverlayBlendAlfa, targetBlendAlpha, DeltaSeconds, BlendAlphaRate)
        : targetBlendAlpha;
}

void UACFBaseOverlayLayer::OnActivated_Implementation()
{
    Super::OnActivated_Implementation();
    bForceStanceRefresh = true;
}
