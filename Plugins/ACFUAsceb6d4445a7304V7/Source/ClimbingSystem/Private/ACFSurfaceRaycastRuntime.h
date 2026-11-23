// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FACFSurfaceCapsuleSweepQuery;
struct FACFSurfaceSphereSweepQuery;
struct FACFSurfaceCapsuleHitResult;
struct FACFSurfaceHitResult;
struct FACFSurfaceQuery;
struct FACFCandidateRegion;
struct FACFSurfaceQueryStats;
struct FACFTriangleVertex;
class USkeletalMeshComponent;
class UACFSurfaceAsset;

struct FACFSurfaceRaycastRuntimeCache;
struct FACFSurfaceRaycastAABBPiece;
struct FACFSurfaceRaycastRegion;
struct FACFSurfaceRaycastBVHNode;

struct FACFSurfaceBVHQuery
{
	FVector Start = FVector::ZeroVector;
	FVector End = FVector::ZeroVector;
	float BoundsExpandRadius = 0.0f;
};

struct FACFSurfaceCapsuleBVHQuery
{
    FVector StartA = FVector::ZeroVector;
    FVector StartB = FVector::ZeroVector;

    FVector EndA = FVector::ZeroVector;
    FVector EndB = FVector::ZeroVector;

    float Radius = 0.0f;
};

class FACFSurfaceRaycastRuntime
{
public:
	static bool InitializeCache(
		const UACFSurfaceAsset* SurfaceAsset,
		FACFSurfaceRaycastRuntimeCache& RuntimeCache);
	
	static bool PrepareCacheForPose(
		const UACFSurfaceAsset* SurfaceAsset,
		const USkeletalMeshComponent* MeshComponent,
		FACFSurfaceRaycastRuntimeCache& RuntimeCache);

	static void CollectCandidateRegions(
		const UACFSurfaceAsset* SurfaceAsset,
		FACFSurfaceRaycastRuntimeCache& RuntimeCache,
		const FACFSurfaceBVHQuery& Query,
		TArray<FACFCandidateRegion>& OutRegions);

    static void CollectCandidateRegions(
        const UACFSurfaceAsset* SurfaceAsset,
        FACFSurfaceRaycastRuntimeCache& RuntimeCache,
        const FACFSurfaceCapsuleBVHQuery& Query,
        TArray<FACFCandidateRegion>& OutRegions);
	
	static void RaycastRegionBVHNearFirst(
		const UACFSurfaceAsset* SurfaceAsset,
		int32 RegionIndex,
		FACFSurfaceRaycastRuntimeCache& RuntimeCache,
		const FACFSurfaceQuery& Query,
		float& InOutBestDistance,
		FACFSurfaceHitResult& InOutBestHit);

	static void SweepSphereRegionBVHNearFirst(
		const UACFSurfaceAsset* SurfaceAsset,
		int32 RegionIndex,
		FACFSurfaceRaycastRuntimeCache& RuntimeCache,
		const FACFSurfaceSphereSweepQuery& Query,
		float& InOutBestDistance,
		FACFSurfaceHitResult& InOutBestHit);

	static void SweepSphereRegionBVHAll(
		const UACFSurfaceAsset* SurfaceAsset,
		int32 RegionIndex,
		FACFSurfaceRaycastRuntimeCache& RuntimeCache,
		const FACFSurfaceSphereSweepQuery& Query,
		TArray<FACFSurfaceHitResult>& OutHits);

    static void SweepCapsuleRegionBVHNearFirst(
        const UACFSurfaceAsset* SurfaceAsset,
        int32 RegionIndex,
        FACFSurfaceRaycastRuntimeCache& RuntimeCache,
        const FACFSurfaceCapsuleSweepQuery& Query,
        float& InOutBestDistance,
        FACFSurfaceCapsuleHitResult& InOutBestHit);

    static void SweepCapsuleRegionBVHAll(
        const UACFSurfaceAsset* SurfaceAsset,
        int32 RegionIndex,
        FACFSurfaceRaycastRuntimeCache& RuntimeCache,
        const FACFSurfaceCapsuleSweepQuery& Query,
        TArray<FACFSurfaceCapsuleHitResult>& OutHits);
	
	static bool SkinVertexWS(
		const UACFSurfaceAsset* SurfaceAsset,
		int32 VertexIndex,
		FACFSurfaceRaycastRuntimeCache& RuntimeCache,
		FVector& OutPositionWS,
		FVector& OutNormalWS);
	
private:

	static bool EnsureRegionAABBUpdated(
		const UACFSurfaceAsset* SurfaceAsset,
		int32 RegionIndex,
		FACFSurfaceRaycastRuntimeCache& RuntimeCache,
		FBox& OutAABBWS);

	static bool EnsureNodeAABBUpdated(
		const UACFSurfaceAsset* SurfaceAsset,
		int32 RegionIndex,
		int32 NodeIndex,
		FACFSurfaceRaycastRuntimeCache& RuntimeCache,
		FBox& OutAABBWS);

	static bool BuildAABBFromPiecesWS(
		const UACFSurfaceAsset* SurfaceAsset,
		const TArray<FACFSurfaceRaycastAABBPiece>& Pieces,
		const FACFSurfaceRaycastRuntimeCache& RuntimeCache,
		FBox& OutAABBWS);
	
	static bool GetNodeEntryDistance(
		const UACFSurfaceAsset* SurfaceAsset,
		int32 RegionIndex,
		int32 NodeIndex,
		FACFSurfaceRaycastRuntimeCache& RuntimeCache,
		const FACFSurfaceBVHQuery& Query,
		float& OutEntryDistance);

    static bool GetCapsuleNodeEntryDistance(
        const UACFSurfaceAsset* SurfaceAsset,
        int32 RegionIndex,
        int32 NodeIndex,
        FACFSurfaceRaycastRuntimeCache& RuntimeCache,
        const FACFSurfaceCapsuleBVHQuery& Query,
        float& OutEntryDistance);

	static void RaycastTriangle(
		const UACFSurfaceAsset* SurfaceAsset,
		int32 TriangleIndex,
		FACFSurfaceRaycastRuntimeCache& RuntimeCache,
		const FACFSurfaceQuery& Query,
		float& InOutBestDistance,
		FACFSurfaceHitResult& InOutBestHit);
	
	static void SweepSphereTriangle(
		const UACFSurfaceAsset* SurfaceAsset,
		int32 TriangleIndex,
		FACFSurfaceRaycastRuntimeCache& RuntimeCache,
		const FACFSurfaceSphereSweepQuery& Query,
		float& InOutBestDistance,
		FACFSurfaceHitResult& InOutBestHit);

	static void SweepSphereTriangleAll(
		const UACFSurfaceAsset* SurfaceAsset,
		int32 TriangleIndex,
		FACFSurfaceRaycastRuntimeCache& RuntimeCache,
		const FACFSurfaceSphereSweepQuery& Query,
		TArray<FACFSurfaceHitResult>& OutHits);

    static void SweepCapsuleTriangle(
        const UACFSurfaceAsset* SurfaceAsset,
        int32 TriangleIndex,
        FACFSurfaceRaycastRuntimeCache& RuntimeCache,
        const FACFSurfaceCapsuleSweepQuery& Query,
        float& InOutBestPenetration,
        FACFSurfaceCapsuleHitResult& InOutBestHit);

    static void SweepCapsuleTriangleAll(
        const UACFSurfaceAsset* SurfaceAsset,
        int32 TriangleIndex,
        FACFSurfaceRaycastRuntimeCache& RuntimeCache,
        const FACFSurfaceCapsuleSweepQuery& Query,
        TArray<FACFSurfaceCapsuleHitResult>& OutHits);
};
