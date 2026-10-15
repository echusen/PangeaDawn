// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "Animation/ACFBlendMaskOverlayLayer.h"

#include "ACFActionTypes.h"
#include "Animation/ACFAnimInstance.h"
#include "Components/SkeletalMeshComponent.h"

namespace ACFBlendMaskOverlayLayer
{
	/** Entry for the stance, else Stances.Idle, else the root "Stances" tag. outStanceTag is only set on an exact match. */
	template <typename TData>
	const TData* FindStanceEntry(const TMap<FGameplayTag, TData>& inMap, const FGameplayTag& inStance, FGameplayTag& outStanceTag)
	{
		if (const TData* Entry = inMap.Find(inStance)) {
			outStanceTag = inStance;
			return Entry;
		}

		// No specific stance matched (no loc stance active, or an active stance with no entry
		// configured): fall back to Stances.Idle first, then to the root "Stances" tag - either
		// one can be used as the Idle/default overlay.
		static const FGameplayTag IdleStanceTag = FGameplayTag::RequestGameplayTag(ACF::IdleTag, false);
		static const FGameplayTag RootStanceTag = FGameplayTag::RequestGameplayTag(ACF::StancesTag, false);
		outStanceTag = FGameplayTag();

		if (const TData* Entry = inMap.Find(IdleStanceTag)) {
			return Entry;
		}
		return inMap.Find(RootStanceTag);
	}
}

UACFBlendMaskOverlayLayer::UACFBlendMaskOverlayLayer()
{
}

void UACFBlendMaskOverlayLayer::NativeInitializeAnimation()
{
	// Before Super, which resolves the first stance: a layer linked while the owner is already in first
	// person has to start on the first person map, with no blend.
	ResolveOwnerAnimInstance();
	bIsInFirstPerson = OwnerAnimInstance && OwnerAnimInstance->IsInFirstPerson();
	bBlendingAimOffset = false;
	bPerspectiveChangePending = false;
	bSnapAimOffset = true;

	Super::NativeInitializeAnimation();
}

void UACFBlendMaskOverlayLayer::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	// Safety net in case the main anim instance did not exist yet at initialization. Game Thread only.
	if (!OwnerAnimInstance) {
		ResolveOwnerAnimInstance();
	}
}

void UACFBlendMaskOverlayLayer::NativeThreadSafeUpdateAnimation(float DeltaSeconds)
{
	// Linked layers update after the main anim instance cached this on the Game Thread, so it is this frame's value.
	const bool bNowInFirstPerson = OwnerAnimInstance && OwnerAnimInstance->IsInFirstPerson();
	if (bNowInFirstPerson != bIsInFirstPerson) {
		bIsInFirstPerson = bNowInFirstPerson;

		// Read by the re-resolve Super does next, so the aim offset blends over FirstPersonAimOffsetBlendTime.
		bPerspectiveChangePending = true;
		bForceStanceRefresh = true;
	}

	Super::NativeThreadSafeUpdateAnimation(DeltaSeconds);

	UpdateAimOffsetBlend(DeltaSeconds);
}

void UACFBlendMaskOverlayLayer::SetMovStance(const FGameplayTag& inOverlay)
{
	if (bIsInFirstPerson) {
		currentOverlay = FACFOverlayAnimationData();
		if (const FACFFirstPersonOverlayAnimationData* Entry = ACFBlendMaskOverlayLayer::FindStanceEntry(FirstPersonLocomotionStancesOverlays, inOverlay, currentOverlayTag)) {
			currentOverlay.Overlay = Entry->Overlay;
			currentOverlay.BlendMask = Entry->BlendMask;
			currentOverlay.BlendMode = Entry->BlendMode;
			currentOverlay.CameraAimRig = Entry->CameraAimRig;
		}

		// The animation based aim offset is always off in first person. The last blendspace is kept only so
		// AimOffsetAlpha has something to blend out of.
		currentOverlay.AimOffset = AimOffset;
		currentOverlay.AimOffsetAlpha = 0.f;
	} else {
		const FACFOverlayAnimationData* Entry = ACFBlendMaskOverlayLayer::FindStanceEntry(LocomotionStancesOverlays, inOverlay, currentOverlayTag);
		currentOverlay = Entry ? *Entry : FACFOverlayAnimationData();
	}

	CurrentBlendMask = currentOverlay.BlendMask;
	CurrentBlendMode = currentOverlay.BlendMode;

	// An entry without an aim offset blends the previous blendspace out, instead of dropping it mid pose.
	if (currentOverlay.AimOffset) {
		AimOffset = currentOverlay.AimOffset;
	}
	const float AimOffsetTarget = currentOverlay.AimOffset ? currentOverlay.AimOffsetAlpha : 0.f;

	if (bSnapAimOffset) {
		TargetAimOffsetAlpha = AimOffsetTarget;
		AimOffsetAlpha = AimOffsetTarget;
		bBlendingAimOffset = false;
	} else {
		SetTargetAimOffsetAlpha(AimOffsetTarget, bPerspectiveChangePending ? FirstPersonAimOffsetBlendTime : currentOverlay.Overlay.BlendTime);
	}
	bSnapAimOffset = false;
	bPerspectiveChangePending = false;

	CurrentCameraAimRig = currentOverlay.CameraAimRig;

	// No overlay animation -> no mask, so ACFBlendByMask leaves the base pose untouched
	// instead of blending in the (ref/T-pose) sampled from an empty sequence.
	if (!currentOverlay.Overlay.OverlayAnim) {
		CurrentBlendMask = nullptr;
	}

	targetBlendAlpha = currentOverlay.Overlay.BlendAlpha;
	targetBlendTime = currentOverlay.Overlay.BlendTime;
	bHasActiveStance = currentOverlayTag != FGameplayTag();
	RefreshGraphVariables();
}

void UACFBlendMaskOverlayLayer::RefreshGraphVariables()
{
	CurrentOverlayAnim = currentOverlay.Overlay.OverlayAnim;
	CurrentOverlayBlendTime = currentOverlay.Overlay.BlendTime;
}

void UACFBlendMaskOverlayLayer::ResolveOwnerAnimInstance()
{
	const USkeletalMeshComponent* Mesh = GetOwningComponent();
	OwnerAnimInstance = Mesh ? Cast<UACFAnimInstance>(Mesh->GetAnimInstance()) : nullptr;
}

void UACFBlendMaskOverlayLayer::SetTargetAimOffsetAlpha(float inTarget, float inBlendTime)
{
	// A stance with no entry is re-resolved every frame, and restarting on each of those would stall the blend.
	if (FMath::IsNearlyEqual(inTarget, TargetAimOffsetAlpha)) {
		return;
	}

	TargetAimOffsetAlpha = inTarget;
	AimOffsetBlendFrom = AimOffsetAlpha;
	AimOffsetBlendElapsed = 0.f;
	AimOffsetBlendDuration = inBlendTime;
	bBlendingAimOffset = true;
}

void UACFBlendMaskOverlayLayer::UpdateAimOffsetBlend(float DeltaSeconds)
{
	if (!bBlendingAimOffset) {
		return;
	}

	AimOffsetBlendElapsed += DeltaSeconds;
	const float BlendProgress = AimOffsetBlendDuration > KINDA_SMALL_NUMBER
		? FMath::Clamp(AimOffsetBlendElapsed / AimOffsetBlendDuration, 0.f, 1.f)
		: 1.f;

	AimOffsetAlpha = FMath::Lerp(AimOffsetBlendFrom, TargetAimOffsetAlpha, BlendProgress);

	if (BlendProgress >= 1.f) {
		bBlendingAimOffset = false;
	}
}
