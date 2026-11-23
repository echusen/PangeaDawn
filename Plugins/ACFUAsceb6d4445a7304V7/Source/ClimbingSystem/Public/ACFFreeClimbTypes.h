// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ACFFreeClimbStateTypes.h"
#include "ACFFreeClimbTypes.generated.h"

class UACFFreeClimbableComponent;

UENUM()
enum class EACFSurfaceSource { Skeletal, Static };

UENUM()
enum class EACFSurfaceQueryFlags : uint8
{
    None = 0,
    ClimbingMove = 1 << 0,
    IKProbe = 1 << 1
};

struct FACFSurfaceQuery
{
    FVector Start;
    FVector End;
    
    bool bCullBackfaces = false;

    uint8 RequiredQueryFlags = static_cast<uint8>(EACFSurfaceQueryFlags::ClimbingMove);
};

struct FACFSurfaceSphereSweepQuery
{
    FVector Start = FVector::ZeroVector;
    FVector End = FVector::ZeroVector;
    float Radius = 5.0f;
    
    bool bCullBackfaces = false;

    uint8 RequiredQueryFlags = static_cast<uint8>(EACFSurfaceQueryFlags::ClimbingMove);
};

struct FACFSurfaceCapsuleSweepQuery
{
    FVector StartA = FVector::ZeroVector;
    FVector StartB = FVector::ZeroVector;

    FVector EndA = FVector::ZeroVector;
    FVector EndB = FVector::ZeroVector;

    float Radius = 0.0f;
    
    bool bCullBackfaces = false;

    uint8 RequiredQueryFlags = static_cast<uint8>(EACFSurfaceQueryFlags::ClimbingMove);
};

struct FACFSurfaceHitResult
{
    EACFSurfaceSource Source = EACFSurfaceSource::Skeletal;

    FVector Position;
    FVector Normal;
    float Distance = 0.0f;

    int32   TriangleIndex = INDEX_NONE;
    FVector Barycentric = FVector::ZeroVector;

    UACFFreeClimbableComponent* Climbable = nullptr;
    UPrimitiveComponent* HitComponent = nullptr;

    bool IsValidSurface() const
    {
        return (Source == EACFSurfaceSource::Skeletal)
            ? (Climbable != nullptr && TriangleIndex != INDEX_NONE)
            : (HitComponent != nullptr);
    }
};

struct FACFSurfaceCapsuleHitResult
{
    FACFSurfaceHitResult SurfaceHit;

    FVector CapsulePoint = FVector::ZeroVector;
    FVector PushNormal = FVector::UpVector;
    float PenetrationDepth = 0.0f;

    FVector BlendedPushDir = FVector::ZeroVector;
    float   MaxPenetrationDepth = 0.0f;
};

UENUM(BlueprintType)
enum class EACFFreeClimbStopReason : uint8 { Manual, LostSurface, RuntimeFailure };

USTRUCT()
struct FACFFreeClimbSurface
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, Category = "ACF")
    bool bValid = false;

    UPROPERTY(VisibleAnywhere, Category = "ACF")
    FVector Point = FVector::ZeroVector;

    UPROPERTY(VisibleAnywhere, Category = "ACF")
    FVector RawNormal = FVector::ForwardVector;

    UPROPERTY(VisibleAnywhere, Category = "ACF")
    FVector PoseTargetNormal = FVector::ForwardVector;

    UPROPERTY(VisibleAnywhere, Category = "ACF")
    FVector SmoothedNormal = FVector::ForwardVector;

    UPROPERTY(VisibleAnywhere, Category = "ACF")
    FVector SurfaceUp = FVector::UpVector;

    UPROPERTY(VisibleAnywhere, Category = "ACF")
    FVector SurfaceRight = FVector::RightVector;
};

USTRUCT(BlueprintType)
struct FACFFreeClimbAnimData
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "ACF")
    EACFFreeClimbState ClimbState = EACFFreeClimbState::Idle;

    UPROPERTY(BlueprintReadOnly, Category = "ACF")
    FVector2D ClimbInput = FVector2D::ZeroVector;

    UPROPERTY(BlueprintReadOnly, Category = "ACF")
    float ClimbInputSize = 0.f;

    UPROPERTY(BlueprintReadOnly, Category = "ACF")
    FVector SurfaceNormal = FVector::ForwardVector;

    UPROPERTY(BlueprintReadOnly, Category = "ACF")
    FVector SurfacePoint = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly, Category = "ACF")
    float ClimbAnimPlayRate = 1.0f;

    UPROPERTY(BlueprintReadOnly, Category = "ACF")
    float JumpProgress = 0.0f;
};

USTRUCT()
struct FACFClingState
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, Category = "ACF")
    FVector Pivot = FVector::ZeroVector;
    UPROPERTY(VisibleAnywhere, Category = "ACF")
    FVector PrePivot = FVector::ZeroVector;

    UPROPERTY(VisibleAnywhere, Category = "ACF")
    FVector HangPos = FVector::ZeroVector;

    UPROPERTY(VisibleAnywhere, Category = "ACF")
    FVector HangVel = FVector::ZeroVector;

    UPROPERTY(VisibleAnywhere, Category = "ACF")
    float ArmLength = 0.f;

    UPROPERTY(VisibleAnywhere, Category = "ACF")
    FVector PrevAnchorVel = FVector::ZeroVector;
};

UENUM(BlueprintType)
enum class EACFFreeClimbLimb : uint8
{
    LeftHand,
    RightHand,
    LeftFoot,
    RightFoot,
    Count UMETA(Hidden)
};

USTRUCT(BlueprintType)
struct FACFFreeClimbProbe
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF")
    FName ProbeName = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF")
    EACFFreeClimbLimb Limb = EACFFreeClimbLimb::LeftHand;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF")
    FVector LocalOffset = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF")
    FVector LocalDirection = FVector::ForwardVector;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF")
    FName ReachReferenceBone = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF")
    float Radius = 15.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF")
    float TraceDistance = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF")
    bool bUseFallbackFanSweep = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF")
    TArray<float> FallbackFanPitchDegrees = { 15.0f, 30.0f, 45.0f, 60.0f, 75.0f, -15.0f, -30.0f, -45.0f, -60.0f, -75.0f };

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF")
    float MinReach = 10.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF")
    float MaxReach = 80.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF")
    bool bEnabled = true;
};

struct FACFFreeClimbProbeResult
{
    FName ProbeName = NAME_None;

    TArray<FACFSurfaceHitResult> Hits;

    void Reset()
    {
        Hits.Reset();
    }
};

struct FACFFreeClimbContactCandidate
{
    FACFSurfaceHitResult SurfaceHit;
    float Score = -BIG_NUMBER;
};

struct FACFFreeClimbContact
{
    bool bValid = false;
    FACFSurfaceHitResult SurfaceHit;
    float Score = -BIG_NUMBER;

    void Reset()
    {
        bValid = false;
        SurfaceHit = FACFSurfaceHitResult{};
        Score = -BIG_NUMBER;
    }
};
