// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ACFProceduralMotionComponent.h"
#include "Animation/AnimSequence.h"
#include "ACFProceduralAimMotionComponent.generated.h"

class UCameraComponent;
class UACFProceduralPresetAimMotion;

/** Aligns the weapon sight to the camera, and optionally bleeds a fraction of the animation back in. */
UCLASS(ClassGroup=(ACFFirstPerson), meta=(BlueprintSpawnableComponent, DisplayName="Aim Component"))
class ASCENTFIRSTPERSON_API UACFProceduralAimMotionComponent : public UACFProceduralMotionComponent
{
	GENERATED_BODY()

public:
	UACFProceduralAimMotionComponent(const FObjectInitializer& ObjectInitializer);

	//~ Begin UACFProceduralMotionComponent
	virtual TSubclassOf<UACFProceduralPreset> GetPresetClass() const override;
	//~ End of UACFProceduralMotionComponent

	/** Legacy solver only. Sight alignment relative to the holder bone. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadWrite, Category="State")
	FTransform AimOffset = FTransform::Identity;

	/**
	 * Aim Point solver only. The transform, in holder bone space, that aiming brings onto the camera.
	 *
	 * Whatever owns this component fills it in — the plugin makes no assumption about where it comes from.
	 * For an equipment actor it is typically its sight socket taken relative to the holder bone, which you
	 * can get with UACFFirstPersonFunctionLibrary::GetSocketTransformRelativeTo. Its rotation carries
	 * the equipment's orientation, so no forward-axis convention is required.
	 */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadWrite, Category="State")
	FTransform AimPointTransform = FTransform::Identity;

	/** Sets AimPointTransform, and copies it into ADSOffset on every recoil component on the same actor. */
	UFUNCTION(BlueprintCallable, Category="State")
	void SetAimPointTransform(const FTransform& InAimPoint);

	//~ Begin UACFProceduralMotionComponent
	virtual bool NeedsReferencePose() const override;
	//~ End UACFProceduralMotionComponent

	/** Overrides the hip-fire FOV the aiming blend returns to. */
	UFUNCTION(BlueprintCallable, Category="State")
	void SetDefaultFOV(float InValue);

protected:
	//~ Begin UACFProceduralMotionComponent
	virtual void BeforeEvaluate(float DeltaSeconds, FComponentSpacePoseContext& Context, FCompactPoseBoneIndex& BoneIndex) override;
	virtual void ComputeOutputs(float DeltaSeconds, const UAnimInstance* AnimInstance, const FACFProceduralMotionInput& Input, TArray<FACFProceduralMotionOutput>& Outs) override;
	virtual void ComputePoseOutputs(float DeltaSeconds, const FACFProceduralMotionPoseContext& Context, TArray<FACFProceduralMotionOutput>& Outs) override;
	//~ End UACFProceduralMotionComponent

	void ComputeLegacyOutputs(const UAnimInstance* AnimInstance, const FACFProceduralMotionInput& Input, float AimAlpha, const UACFProceduralPresetAimMotion* Preset, TArray<FACFProceduralMotionOutput>& Outs);

	FTransform ExtractBoneTransform(const UAnimSequence* Animation, USkeleton* Skeleton, FName BoneName) const;

	void RefreshFOV(float DeltaTime, float AimState, const UACFProceduralPresetAimMotion* Preset);

protected:
	/** Aiming alpha resolved on the game thread, consumed by the Aim Point solver during evaluation. */
	float CachedAimAlpha = 0.0f;

	/** How much of the live animation delta survives ADS. Resolved on the game thread. */
	float CachedAdditiveAlpha = 0.0f;

	FVector CachedPivotOffset = FVector::ZeroVector;

	/** BeforeEvaluate runs before the first ComputeOutputs, so caching waits for the pivot offset. */
	bool bCanCacheRef = false;

	UPROPERTY()
	TObjectPtr<UCameraComponent> Camera;

	/** Holder bone relative to its parent in the reference animation. Resolved once. */
	FTransform CachedRefRelativeTransform;
	bool bIsRefCached = false;

	/** Holder bone relative to its parent in the live pose. Captured every BeforeEvaluate. */
	FTransform CurrentRelativeTransform;
	bool bHasValidIncomingPose = false;

	float DefaultFOV = 90.f;
};
