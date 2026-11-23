// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ACFFreeClimbTypes.h"
#include "ACFFreeClimbRigProfile.generated.h"

class USkeletalMesh;
class USkeleton;

USTRUCT(BlueprintType)
struct CLIMBINGSYSTEM_API FACFFreeClimbRigCurveSettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float TraceThreshold = 0.6f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float LockThreshold = 0.95f;

    /** FInterpTo speed for IK alpha when blending from OFF to ON. Higher = snappier (~40-80 for a quick plant). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF", meta = (ClampMin = "0.0"))
    float BlendInSpeed = 15.0f;

    /** FInterpTo speed for IK alpha when blending from ON to OFF. Higher = snappier (~40-80 for a quick release). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF", meta = (ClampMin = "0.0"))
    float BlendOutSpeed = 15.0f;
};

USTRUCT(BlueprintType)
struct CLIMBINGSYSTEM_API FACFFreeClimbLimbProfile
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF")
    bool bEnabled = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF|Rig")
    FName EffectorBone = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF|Rig")
    FName IKBone = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF|Probe")
    FName ProbeOriginBone = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF|Probe")
    FVector ProbeLocalOffset = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF|Probe")
    FVector ProbeLocalDirection = FVector::ForwardVector;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF|Probe", meta = (ClampMin = "0.0"))
    float ProbeRadius = 15.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF|Probe", meta = (ClampMin = "0.0"))
    float TraceDistance = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF|Probe")
    bool bUseFallbackFanSweep = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF|Probe")
    TArray<float> FallbackFanPitchDegrees = { 15.0f, 30.0f, -15.0f, -30.0f };

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF|Probe", meta = (ClampMin = "0.0"))
    float MinReach = 10.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF|Probe", meta = (ClampMin = "0.0"))
    float MaxReach = 80.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF")
    FName ContactCurveName = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF", meta = (Units = "cm"))
    float SurfaceOffset = 10.0f;
};

UCLASS(BlueprintType)
class CLIMBINGSYSTEM_API UACFFreeClimbRigProfile : public UDataAsset
{
    GENERATED_BODY()

public:
    UACFFreeClimbRigProfile();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF")
    TSoftObjectPtr<USkeleton> TargetSkeleton;

#if WITH_EDITORONLY_DATA
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF")
    TSoftObjectPtr<USkeletalMesh> PreviewMesh;
#endif

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF|Limbs")
    FACFFreeClimbLimbProfile LeftHand;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF|Limbs")
    FACFFreeClimbLimbProfile RightHand;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF|Limbs")
    FACFFreeClimbLimbProfile LeftFoot;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF|Limbs")
    FACFFreeClimbLimbProfile RightFoot;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF")
    FACFFreeClimbRigCurveSettings Curves;

    const FACFFreeClimbLimbProfile& GetLimbProfile(EACFFreeClimbLimb Limb) const;

    bool IsCompatibleWith(const USkeletalMesh* SkeletalMesh, FString* OutReason = nullptr) const;
};
