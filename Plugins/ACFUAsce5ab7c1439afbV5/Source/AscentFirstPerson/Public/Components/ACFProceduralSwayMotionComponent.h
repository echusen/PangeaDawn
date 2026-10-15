// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ACFProceduralMotionComponent.h"
#include "Presets/ACFProceduralPresetSwayMotion.h"
#include "ACFProceduralSwayMotionComponent.generated.h"

class APawn;
class UACFProceduralPresetSwayMotion;

/** Spring-damped weapon lag driven by look and movement input. */
UCLASS(ClassGroup=(ACFFirstPerson), meta=(BlueprintSpawnableComponent, DisplayName="Sway Component"))
class ASCENTFIRSTPERSON_API UACFProceduralSwayMotionComponent : public UACFProceduralMotionComponent
{
	GENERATED_BODY()

public:
	UACFProceduralSwayMotionComponent(const FObjectInitializer& ObjectInitializer);

	//~ Begin UACFProceduralMotionComponent
	virtual TSubclassOf<UACFProceduralPreset> GetPresetClass() const override;
	//~ End of UACFProceduralMotionComponent

	/** Master multiplier over both sway sources. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings|Sway")
	float SwayScale = 1.0f;

protected:
	//~ Begin UACFProceduralMotionComponent
	virtual void ComputeOutputs(float DeltaSeconds, const UAnimInstance* AnimInstance, const FACFProceduralMotionInput& Input, TArray<FACFProceduralMotionOutput>& Outs) override;
	//~ End UACFProceduralMotionComponent

	const UACFProceduralPresetSwayMotion* GetSwayPreset() const;

	void RefreshSwayTargets(float DeltaSeconds, const FACFSwayData& SwayData);
	void RefreshAimSway(float DeltaSeconds, const FACFSwayData& SwayData);
	void RefreshMoveSway(float DeltaSeconds, const FACFSwayData& SwayData);

	/** Fills MouseDeltaInput and MoveInput from the animated pawn. No-op unless bAutoReadFromOwner. */
	void AcquireInputsFromOwner(float DeltaSeconds, const UAnimInstance* AnimInstance, const UACFProceduralPresetSwayMotion* Preset);
	void RefreshLookInput(float DeltaSeconds, const APawn* Pawn, const UACFProceduralPresetSwayMotion* Preset);
	void RefreshMoveInput(const APawn* Pawn, const UACFProceduralPresetSwayMotion* Preset);

protected:
	/** Look delta in degrees. X = yaw, Y = pitch. */
	FVector2D MouseDeltaInput = FVector2D::ZeroVector;

	/** Movement in [-1..1]. X = strafe/right, Y = forward. */
	FVector2D MoveInput = FVector2D::ZeroVector;

	FTransform AimSwayTarget = FTransform::Identity;
	FTransform MoveSwayTarget = FTransform::Identity;
	FTransform AimSwayOutput = FTransform::Identity;
	FTransform MoveSwayOutput = FTransform::Identity;

	FACFSwaySpringState AimSpring;
	FACFSwaySpringState MoveSpring;

	float AimingState = 0.0f;

	/** Previous view rotation, for the look delta. */
	FRotator LastViewRotation = FRotator::ZeroRotator;

	/** Smoothed view rotation used for pawns we do not control locally. */
	FRotator SmoothedViewRotation = FRotator::ZeroRotator;

	/** Pawn LastViewRotation was sampled from. Switching pawns resets the delta. */
	TWeakObjectPtr<const APawn> LastViewSource;

	bool bHasLastViewRotation = false;
};
