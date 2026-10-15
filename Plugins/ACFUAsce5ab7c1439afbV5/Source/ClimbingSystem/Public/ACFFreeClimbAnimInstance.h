// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "ACFFreeClimbComponent.h"
#include "ACFFreeClimbRigProfile.h"
#include "ACFFreeClimbTypes.h"
#include "ACFFreeClimbAnimInstance.generated.h"

class UAnimMontage;

enum class ELimbIKState
{
    Free,
    Trace,
    Locked,
    Release,
};

USTRUCT(BlueprintType)
struct CLIMBINGSYSTEM_API FACFFreeClimbIKTarget
{
    GENERATED_BODY()

    int32 TriangleIndex = INDEX_NONE;
    FVector Barycentric = FVector::ZeroVector;

    ELimbIKState State = ELimbIKState::Free;

    bool bHasValidPlantTransform = false;

    TObjectPtr<UACFFreeClimbableComponent> Climbable = nullptr;

    EACFSurfaceSource Source = EACFSurfaceSource::Skeletal;
    TWeakObjectPtr<UPrimitiveComponent> Primitive;
    FVector LocalPos = FVector::ZeroVector;
    FVector LocalNormal = FVector::ZeroVector;

    bool bPlanted = false;

    bool bExternallyPinned = false;

    UPROPERTY(BlueprintReadOnly, Category = "ACF|IK")
    FTransform PlantTransform = FTransform::Identity;
    
    UPROPERTY(BlueprintReadOnly, Category = "ACF|IK")
    float Alpha = 0.0f;
};

USTRUCT(BlueprintType)
struct CLIMBINGSYSTEM_API FACFFreeClimbRigTarget
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "ACF|Rig")
    FName EffectorBone = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category = "ACF|Rig")
    FName IKBone = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category = "ACF|Rig")
    FTransform TargetTransform = FTransform::Identity;

    UPROPERTY(BlueprintReadOnly, Category = "ACF|Rig")
    float Alpha = 0.0f;
};

UCLASS()
class CLIMBINGSYSTEM_API UACFFreeClimbAnimInstance : public UAnimInstance
{
    GENERATED_BODY()

public:
    UACFFreeClimbAnimInstance();

    virtual void NativeInitializeAnimation() override;
    virtual void NativeUpdateAnimation(float DeltaSeconds) override;
    virtual void NativeUninitializeAnimation() override;

    void PinLimbToAnchor(EACFFreeClimbLimb Limb, const FACFSurfaceHitResult& Hit);
    void ReleaseAllPins();

    UFUNCTION(BlueprintPure, Category = "ACF")
    UAnimMontage* GetClimbUpMontage() const { return ClimbUpMontage; }

    UFUNCTION(BlueprintPure, Category = "ACF")
    UAnimMontage* GetClimbExitMontage() const { return ClimbExitMontage; }

private:
    // Finds and caches the owner's climbing component.
    bool TryResolveClimbingComponent();

    // IK Update
    void UpdateIKTargets(float DeltaSeconds);

    void UpdateIKTarget(float DeltaSeconds,
        EACFFreeClimbLimb Limb,
        FName ContactCurveName,
        float SurfaceOffset,
        float BlendInSpeed,
        float BlendOutSpeed,
        float TraceThreshold,
        float LockThreshold,
        FACFFreeClimbIKTarget& Target,
        const USkeletalMeshComponent* Mesh,
        const UACFFreeClimbableComponent* Climbable);
    void UpdateIKTargetState(FACFFreeClimbIKTarget& Target, float CurveValue, float TraceThreshold, float LockThreshold);
    void RefreshRigProfile();
    void RefreshRigTargets();

protected:
    UPROPERTY(EditDefaultsOnly, Category = "ACF")
    TObjectPtr<UAnimMontage> ClimbUpMontage = nullptr;

    UPROPERTY(EditDefaultsOnly, Category = "ACF")
    TObjectPtr<UAnimMontage> ClimbExitMontage = nullptr;

    UPROPERTY(BlueprintReadOnly, Category = "ACF")
    EACFFreeClimbState ClimbState = EACFFreeClimbState::Idle;

    UPROPERTY(BlueprintReadOnly, Category = "ACF")
    float BlendX = 50.f;

    UPROPERTY(BlueprintReadOnly, Category = "ACF")
    float BlendY = 50.f;

    UPROPERTY(BlueprintReadOnly, Category = "ACF")
    bool bIsMoving = false;

    UPROPERTY(EditDefaultsOnly, Category = "ACF", meta = (ClampMin = "0.0"))
    float MoveInputDeadzone = 0.1f;

    UPROPERTY(BlueprintReadOnly, Category = "ACF")
    FVector SurfaceNormal = FVector::ForwardVector;

    UPROPERTY(BlueprintReadOnly, Category = "ACF")
    FVector SurfacePoint = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly, Category = "ACF")
    TArray<FACFFreeClimbIKTarget> IKTargets;

    UPROPERTY(BlueprintReadOnly, Category = "ACF")
    float ClimbAnimPlayRate = 1.0f;

    UPROPERTY(BlueprintReadOnly, Category = "ACF")
    float JumpProgress = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "ACF|Rig")
    TArray<FACFFreeClimbRigTarget> RigTargets;

    UPROPERTY(Transient)
    TObjectPtr<UACFFreeClimbRigProfile> RigProfile = nullptr;

    bool bWasClimbing = false;

private:
    UPROPERTY(Transient)
    TWeakObjectPtr<UACFFreeClimbComponent> ClimbingComponent;
};
