// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.
//
// Simple proportional-rescale root motion warp built on top of Epic's stock Motion Warping
// plugin (URootMotionModifier_Warp), useful for warping mounted/vehicle entry-exit montages
// onto a precise dismount/mount target. Only requires the "MotionWarping" plugin enabled.

#pragma once

#include "CoreMinimal.h"
#include "RootMotionModifier.h"
#include "ACFRootMotionModifier_SimpleWarp.generated.h"

class UMotionWarpingComponent;

/**
 * Proportional-rescale warp for translation (bWarpTranslation), a lerp-only translation mode
 * (bLerpTranslation), per-axis ignore flags (bIgnoreXYAxes / bIgnoreYaw), and a Z-tolerance
 * lerp fallback (WarpZTolerance) on top of Epic's stock URootMotionModifier_Warp.
 */
UCLASS(meta = (DisplayName = "ACF Simple Warp"))
class VEHICLESYSTEM_API UACFRootMotionModifier_SimpleWarp : public URootMotionModifier_Warp
{
	GENERATED_BODY()

public:

	/** When true, translation on the horizontal plane will remain intact */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config", meta = (DisplayAfter = "bWarpTranslation", EditCondition = "bWarpTranslation"))
	bool bIgnoreXYAxes = false;

	/** When total translation on the Z axis is below this value we will linearly interpolate it to reach the target. Only relevant when bIgnoreZAxis is false */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config", meta = (DisplayAfter = "WarpRotationTimeMultiplier", EditCondition = "bAllowAdditiveTranslationZAxis"))
	float WarpZTolerance = 1.f;

	/** When true, translation will be linearly interpolated instead of warped */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
	bool bLerpTranslation = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config", meta = (DisplayAfter = "bWarpRotation", EditCondition = "bWarpRotation"))
	bool bIgnoreYaw = false;

	UACFRootMotionModifier_SimpleWarp(const FObjectInitializer& ObjectInitializer);

	virtual FTransform ProcessRootMotion(const FTransform& InRootMotion, float DeltaSeconds) override;
	virtual void OnTargetTransformChanged() override;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	void PrintLog(const FTransform& OriginalRootMotion, const FTransform& WarpedRootMotion) const;
#endif

protected:

	UPROPERTY()
	FTransform CachedRootMotion;

public:

	/** Wrapper function to add warping targets to reduce friction during merges while using the old motion warping API */
	UFUNCTION(BlueprintCallable, Category = "Motion Warping", meta = (DisplayName = "Add Or Update Warp Target (ACF)"))
	static void BP_AddOrUpdateWarpTarget(UMotionWarpingComponent* MotionWarpingComp, FName Name, FVector Location, FRotator Rotation);
};
