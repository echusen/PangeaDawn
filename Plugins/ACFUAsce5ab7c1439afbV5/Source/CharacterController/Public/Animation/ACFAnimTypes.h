// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved. 

#pragma once


#include "CoreMinimal.h"
#include <GameplayTagContainer.h>
#include "Animation/AnimSequence.h"
#include "ACFCoreTypes.h"
#include "ACFCCTypes.h"
#include "AnimNode_ACFBlendByMask.h"
#include "ACFAnimTypes.generated.h"

class UACFBoneBlendMask;
class UACFBaseOverlayLayer;
class UAimOffsetBlendSpace;

/**
 * 
 */
 USTRUCT(BlueprintType)
struct FBaseLayer {

    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite,  meta = (Categories = "Moveset,Mount"), Category = ACF)
    FGameplayTag TagName;

    FORCEINLINE bool operator!=(const FGameplayTag& Other) const
    {
        return TagName != Other;
    }

    FORCEINLINE bool operator==(const FGameplayTag& Other) const
    {
        return TagName == Other;
    }

    FORCEINLINE bool operator!=(const FACFStruct& Other) const
    {
        return TagName != Other.TagName;
    }

    FORCEINLINE bool operator==(const FACFStruct& Other) const
    {
        return TagName == Other.TagName;
    }
};


USTRUCT(BlueprintType)
struct FMoveset : public FBaseLayer {

	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = ACF)
	TSubclassOf<class UACFMovesetLayer> Moveset;
};


USTRUCT(BlueprintType)
struct FOverlayLayer : public FBaseLayer {

	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = ACF)
	TSubclassOf<class UACFBaseOverlayLayer> Overlay;
};

/** One overlay animation entry with its own blend mask and blend mode. Consumed by
 * UACFBlendMaskOverlayLayer's LocomotionStancesOverlays map and the ACFBlendByMask AnimGraph
 * node; the entry keyed by the root "Stances" tag acts as the Idle/default fallback.
 * Overlay.BlendTime is exposed as CurrentOverlayBlendTime and fed into the AnimGraph node's BlendTime pin. */
USTRUCT(BlueprintType)
struct FACFOverlayAnimationData
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ACF)
	FOverlayConfig Overlay;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ACF)
	TObjectPtr<UACFBoneBlendMask> BlendMask;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ACF)
	EACFMaskBlendMode BlendMode = EACFMaskBlendMode::DynamicAdditive;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ACF)
	TObjectPtr<UAimOffsetBlendSpace> AimOffset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ACF)
	float AimOffsetAlpha = 1.f;

	/** Bones the ACFCameraAim node drives for this hold, and where they sit relative to the view.
	 * Leave empty to keep the aim offset blendspace above doing the work instead. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ACF)
	TObjectPtr<class UACFCameraAimRig> CameraAimRig;
};

/** First person counterpart of FACFOverlayAnimationData, used by UACFBlendMaskOverlayLayer while the owner is in
 * first person. There is no aim offset here: the animation based aim offset is always off in first person. */
USTRUCT(BlueprintType)
struct FACFFirstPersonOverlayAnimationData
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ACF)
	FOverlayConfig Overlay;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ACF)
	TObjectPtr<UACFBoneBlendMask> BlendMask;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ACF)
	EACFMaskBlendMode BlendMode = EACFMaskBlendMode::DynamicAdditive;

	/** Optional, and never taken from the third person entry. Leave empty when the Procedural Aim Offset and
	 * Procedural Motion nodes aim the first person pose: a socket lock here would fight their sway and recoil. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ACF)
	TObjectPtr<class UACFCameraAimRig> CameraAimRig;
};

USTRUCT(BlueprintType)
struct FRiderLayer : public FBaseLayer {

	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = ACF)
	TSubclassOf<class UACFRiderLayer> RiderLayer;
};



