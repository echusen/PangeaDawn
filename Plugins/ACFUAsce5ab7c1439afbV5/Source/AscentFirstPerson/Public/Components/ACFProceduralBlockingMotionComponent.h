// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ACFProceduralMotionComponent.h"
#include "Engine/HitResult.h"
#include "ACFProceduralBlockingMotionComponent.generated.h"

class ACharacter;
class UACFProceduralPresetData;
class USkeletalMeshComponent;

/**
 * Pushes the weapon aside when the muzzle is close to geometry.
 *
 * The trace has to run on the game thread but needs the pose before blocking is applied, so the pivot is
 * captured in BeforeEvaluate, resolved to world space in OnPoseFinalized, and swept in ComputeOutput.
 */
UCLASS(ClassGroup=(ACFFirstPerson), meta=(BlueprintSpawnableComponent, DisplayName="Blocking Component"))
class ASCENTFIRSTPERSON_API UACFProceduralBlockingMotionComponent : public UACFProceduralMotionComponent
{
	GENERATED_BODY()

public:
	UACFProceduralBlockingMotionComponent(const FObjectInitializer& ObjectInitializer);

	//~ Begin UACFProceduralMotionComponent
	virtual TSubclassOf<UACFProceduralPreset> GetPresetClass() const override;
	//~ End of UACFProceduralMotionComponent

	//~ Begin UActorComponent
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End UActorComponent

	UFUNCTION(BlueprintPure, Category="Blocking")
	const FTransform& GetBlockingOutput() const { return BlockingOutput; }

	UFUNCTION(BlueprintPure, Category="Blocking")
	FORCEINLINE float GetBlockingAlpha() const { return BlockingAlpha; }

	UFUNCTION(BlueprintPure, Category="Blocking")
	FORCEINLINE bool IsBlocking() const { return BlockingAlpha > BlockingActiveThreshold; }

	/**
	 * Where the held item ends, usually the muzzle, relative to the holder bone. The sweep then reaches
	 * CollisionPointDistance past it rather than past the holder, so a long barrel blocks before it clips.
	 */
	UFUNCTION(BlueprintCallable, Category="Blocking")
	void SetCollisionPoint(const FTransform& InCollisionPoint);

	UFUNCTION(BlueprintCallable, Category="Blocking")
	void ClearCollisionPoint();

protected:
	//~ Begin UACFProceduralMotionComponent
	virtual void BeforeEvaluate(float DeltaSeconds, FComponentSpacePoseContext& Context, FCompactPoseBoneIndex& BoneIndex) override;
	virtual bool ComputeOutput(float DeltaSeconds, const UAnimInstance* AnimInstance, const FACFProceduralMotionInput& Input, FACFProceduralMotionOutput& Out) override;
	//~ End UACFProceduralMotionComponent

	void RefreshBlocking(float DeltaTime);

	UFUNCTION()
	void OnPoseFinalized();

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings|Blocking")
	uint8 bBlockingEnabled : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings|Blocking")
	uint8 bAutoFindMeshComponent : 1;

	/** Sweep length ahead of the collision point, or of the holder when none is set. Also the distance over which the alpha ramps to 1. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings|Blocking")
	float CollisionPointDistance = 15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings|Blocking")
	float TraceRadius = 10.0f;

	/** Negative disables the override. Zero or above forces the alpha and skips the distance ramp. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings|Blocking")
	float OverrideBlockingAlpha = -1.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadWrite, Category="State|Blocking")
	TObjectPtr<USkeletalMeshComponent> TargetMeshComponent;

protected:
	/** Transform currently being applied, scaled by the alpha. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="State|Blocking")
	FTransform BlockingOutput = FTransform::Identity;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="State|Blocking")
	float BlockingAlpha = 0.0f;

	UPROPERTY(Transient)
	FHitResult BlockingHitResult;

	UPROPERTY(Transient)
	FVector BlockingTraceStartLocation = FVector::ZeroVector;

	UPROPERTY(Transient)
	FVector BlockingTraceEndLocation = FVector::ZeroVector;

	UPROPERTY(Transient)
	FName ParentBoneName = FName(TEXT("head"));

	/** Pivot in world space before blocking is applied. Written in OnPoseFinalized, read by ComputeOutput. */
	UPROPERTY(Transient)
	FTransform BeforeBlockingPose = FTransform::Identity;

	/** Pivot relative to its parent bone, captured off the incoming pose. */
	UPROPERTY(Transient)
	FTransform StableRelativeTransform = FTransform::Identity;

	/** Held item end relative to the holder bone. Only used while bHasCollisionPoint. */
	UPROPERTY(Transient)
	FTransform CollisionPoint = FTransform::Identity;

	bool bHasCollisionPoint = false;

	FDelegateHandle OnPoseFinalizedHandle;

private:
	static constexpr float BlockingActiveThreshold = 0.1f;
	static constexpr float BlockingInterpSpeed = 10.0f;
};
