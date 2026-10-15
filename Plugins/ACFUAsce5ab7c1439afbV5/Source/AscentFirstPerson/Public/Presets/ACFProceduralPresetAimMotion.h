// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Presets/ACFProceduralPreset.h"
#include "Types/ACFFirstPersonItemTypes.h"
#include "ACFProceduralPresetAimMotion.generated.h"

class UAnimSequence;

UENUM(BlueprintType)
enum class EACFProceduralAimSolver : uint8
{
	/**
	 * Original solver. Requires the equipment to point component +X, compensated by a rotation on the
	 * equipment AnimBP root, PivotOffsetRotation on Bone Rebase and OriginRelativeRotation here.
	 */
	Legacy		UMETA(DisplayName="Legacy (+X Equipment)"),

	/**
	 * Aligns the component's AimPointTransform onto the camera. The aim point carries the equipment's
	 * orientation, so there is no forward-axis requirement and none of the compensating rotations apply.
	 */
	AimPoint	UMETA(DisplayName="Aim Point"),
};

UCLASS(meta=(DisplayName="Preset: Aim Motion"))
class ASCENTFIRSTPERSON_API UACFProceduralPresetAimMotion : public UACFProceduralPreset
{
	GENERATED_BODY()

public:
	UACFProceduralPresetAimMotion();

	/** Defaults to Legacy so existing preset assets keep their behaviour. See Docs/ADS-REDESIGN.md to migrate. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Aim")
	EACFProceduralAimSolver Solver = EACFProceduralAimSolver::Legacy;

	/** Fully masks aim motion. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Aim", meta=(GetOptions="GetCurveNameOptions"))
	FName MaskAimCurve = FName("MaskAim");

	/**
	 * How much of the live animation survives aiming. 0 = weapon locked to the aim point, 1 = full motion.
	 *
	 * Aim Point solver: needs the node's Reference Pose pin connected, and non-zero here is what makes the
	 * node evaluate that branch. Correct at 1.0, since the delta is identity while only the base pose plays.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Aim", meta=(ClampMin="0.0", ClampMax="1.0"))
	float AimRatio = 0.2f;

	/**
	 * Points on the held item that aiming lines up with the camera, such as iron sights and an optic. The first
	 * person component resolves them for the held item and starts on the first one; it can switch between them.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Aim|Aim Point", meta=(EditCondition="Solver == EACFProceduralAimSolver::AimPoint", EditConditionHides))
	TArray<FACFFirstPersonItemPoint> AimPoints;

	/**
	 * Scales AimRatio per frame, so montages can open and close the bleed-through.
	 *
	 * Legacy only. The Aim Point solver measures against a live reference pose, so the delta is already
	 * zero unless something is actually playing and there is nothing to gate.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Aim|Legacy", meta=(EditCondition="Solver == EACFProceduralAimSolver::Legacy", EditConditionHides, GetOptions="GetCurveNameOptions"))
	FName AimAdditiveMotionAlphaCurve = FName("AimAdditiveMotionAlpha");

	// The Idle Animation used to calculate the "Base Pose".
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Aim|Legacy", meta=(EditCondition="Solver == EACFProceduralAimSolver::Legacy", EditConditionHides))
	TObjectPtr<UAnimSequence> ReferenceAnimation;

	// Adjusts the rotation of the Reference Pose.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Aim|Legacy", meta=(EditCondition="Solver == EACFProceduralAimSolver::Legacy", EditConditionHides))
	FRotator OriginRelativeRotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Aim|FOV")
	bool bUseAimingFOV = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Aim|FOV", meta=(EditCondition="bUseAimingFOV", EditConditionHides))
	bool bAimFOVAffectedByCurve = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Aim|FOV", meta=(EditCondition="bUseAimingFOV", EditConditionHides, ClampMin="1.0", ClampMax="360.0", UIMin="20.0", UIMax="120.0"))
	float AimingFOV = 70.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Aim|FOV", meta=(EditCondition="bUseAimingFOV", EditConditionHides, ClampMin="1.0", ClampMax="999.0", UIMin="1.0", UIMax="50.0"))
	float AimInFOVInterpSpeed = 20.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Aim|FOV", meta=(EditCondition="bUseAimingFOV", EditConditionHides, ClampMin="1.0", ClampMax="999.0", UIMin="1.0", UIMax="50.0"))
	float AimOutFOVInterpSpeed = 15.f;
};
