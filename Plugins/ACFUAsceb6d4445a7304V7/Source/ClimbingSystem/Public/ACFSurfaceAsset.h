// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/SkeletalMesh.h"
#include "UObject/Object.h"
#include "ACFSurfaceRaycastTypes.h"
#include "ACFFreeClimbTypes.h"
#include "ACFSurfaceAsset.generated.h"

USTRUCT()
struct FACFTriangleVertex
{
    GENERATED_BODY()

    UPROPERTY()
    FVector Position = FVector::ZeroVector;

    UPROPERTY()
    FVector Normal = FVector::ZeroVector;

    UPROPERTY()
    TArray<int32> BoneIndices;
    
    UPROPERTY()
    TArray<float> BoneWeights;
};

USTRUCT()
struct FACFTriangle
{
    GENERATED_BODY()

    UPROPERTY()
    int32 VertexIndices[3] = { INDEX_NONE, INDEX_NONE, INDEX_NONE };

    UPROPERTY()
    FVector Normal = FVector::ZeroVector;

    UPROPERTY()
    uint8 QueryFlags =
        static_cast<uint8>(EACFSurfaceQueryFlags::ClimbingMove) |
        static_cast<uint8>(EACFSurfaceQueryFlags::IKProbe);
};

USTRUCT()
struct FACFReductionSettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "ACF", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float NumOfTrianglesPercentage = 0.25f;

    UPROPERTY(EditAnywhere, Category = "ACF", meta = (ClampMin = "0.0"))
    float WeldingThreshold = 0.1f;

    UPROPERTY(EditAnywhere, Category = "ACF")
    bool bLockEdges = false;

    UPROPERTY(EditAnywhere, Category = "ACF", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float VolumeImportance = 1.0f;
};

USTRUCT()
struct FACFLogicalWeldSettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "ACF")
    bool bEnabled = true;

    UPROPERTY(EditAnywhere, Category = "ACF", meta = (ClampMin = "0.0"))
    float WeldDistance = 1.0f;

    UPROPERTY(EditAnywhere, Category = "ACF", meta = (ClampMin = "0.0", ClampMax = "180.0"))
    float MaxNormalAngleDegrees = 45.0f;

    UPROPERTY(EditAnywhere, Category = "ACF")
    bool bRequireSameDominantBone = true;
};

USTRUCT()
struct FACFNormalFlattenSettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "ACF", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float Strength = 0.5f;

    UPROPERTY(EditAnywhere, Category = "ACF", meta = (ClampMin = "0", ClampMax = "180"))
    float MaxAngleDegrees = 45.0f;

    UPROPERTY(EditAnywhere, Category = "ACF", meta = (ClampMin = "1"))
    int32 Iteration = 1;
};

UENUM()
enum class EACFBridgePathType : uint8
{
    OpenSpline,
    ClosedLoop
};

USTRUCT()
struct FACFBridgePoint
{
    GENERATED_BODY()

    UPROPERTY()
    int32 TriangleIndex = INDEX_NONE;

    UPROPERTY()
    FVector Barycentric = FVector::ZeroVector;

    UPROPERTY()
    FVector LocalPosition = FVector::ZeroVector;
};

USTRUCT()
struct FACFBridgeDefinition
{
    GENERATED_BODY()

    UPROPERTY()
    EACFBridgePathType PathType = EACFBridgePathType::OpenSpline;

    UPROPERTY()
    TArray<FACFBridgePoint> FirstPathPoints;

    UPROPERTY()
    TArray<FACFBridgePoint> SecondPathPoints;

    UPROPERTY()
    uint32 BaseSurfaceTopologyHash = 0;

    UPROPERTY()
    bool bReverseWinding = false;
};

UCLASS()
class CLIMBINGSYSTEM_API UACFSurfaceAsset : public UObject
{
	GENERATED_BODY()
	
public:
    UPROPERTY(EditAnywhere, Category = "ACF")
    TObjectPtr<USkeletalMesh> SourceMesh = nullptr;

    UPROPERTY(EditAnywhere, Category = "ACF")
    FACFReductionSettings ReductionSettings;

    UPROPERTY(EditAnywhere, Category = "ACF")
    FACFLogicalWeldSettings LogicalWeldSettings;

    UPROPERTY(EditAnywhere, Category = "ACF")
    FACFNormalFlattenSettings NormalFlattenSettings;

    /** Material slots whose render sections are excluded when generating ACF Free Climb Surface data. */
    UPROPERTY()
    TArray<FName> ExcludedMaterialSlotNames;

    UPROPERTY(VisibleAnywhere, Category = "ACF|Bridge")
    TArray<FACFBridgeDefinition> BridgeDefinitions;

    UPROPERTY()
    int32 BaseSurfaceVertexCount = 0;

    UPROPERTY()
    int32 BaseSurfaceTriangleCount = 0;

    UPROPERTY()
    uint32 BaseSurfaceTopologyHash = 0;
    
    UPROPERTY(EditAnywhere, Category = "Raycast")
    bool bBuildRaycastBVH = true;

    UPROPERTY()
    TArray<FACFTriangle> Triangles;

    UPROPERTY()
    TArray<FACFTriangleVertex> Vertices;
    
    UPROPERTY()
    FACFSurfaceRaycastData RaycastData;
};
