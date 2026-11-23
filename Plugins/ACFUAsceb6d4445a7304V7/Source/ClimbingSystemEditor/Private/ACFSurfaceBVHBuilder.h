// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ACFSurfaceAsset.h"

struct FReferenceSkeleton;

class FACFSurfaceBVHBuilder
{
public:
	static bool Build(UACFSurfaceAsset* InAsset);

private:
	static bool BuildRefPoseBoneCS(UACFSurfaceAsset* InAsset, FString& OutError);

	static bool BuildRegions(UACFSurfaceAsset* InAsset, FString& OutError);
	static int32 ComputeDominantBoneForTriangle(const UACFSurfaceAsset* InAsset, const FACFTriangle& Triangle);

	static bool BuildRegionGraph(UACFSurfaceAsset* InAsset, FString& OutError);
	static int32 FindNearestAncestorRegion(const FReferenceSkeleton& RefSkeleton, const UACFSurfaceAsset* InAsset, int32 BoneIndex);

	static bool BuildRegionAABBPieces(UACFSurfaceAsset* InAsset, FString& OutError);
	static bool BuildRegionAABBPiecesForRegion(UACFSurfaceAsset* InAsset, FACFSurfaceRaycastRegion& Region, FString& OutError);
	static bool AddVertexToBoneLocalBoxes(const UACFSurfaceAsset* InAsset, int32 VertexIndex, TMap<int32, FBox>& BoneToLocalBoxes, FString& OutError);

	static bool BuildRegionTriangleBVHs(UACFSurfaceAsset* InAsset, FString& OutError);
	static bool BuildRegionTriangleBVH(UACFSurfaceAsset* InAsset, FACFSurfaceRaycastRegion& Region, int32 LeafTriangleCount, int32& OutMaxDepth, FString& OutError);
	static int32 BuildRegionTriangleBVHNode(
		UACFSurfaceAsset* InAsset,
		FACFSurfaceRaycastBVH& BVH,
		int32 FirstTriangle,
		int32 TriangleCount,
		int32 LeafTriangleCount,
		int32 Depth,
		int32& InOutMaxDepth,
		FString& OutError);

	static bool BuildNodeAABBPieces(const UACFSurfaceAsset* InAsset, const FACFSurfaceRaycastBVH& BVH, FACFSurfaceRaycastBVHNode& Node, FString& OutError);

	static FVector ComputeTriangleRefCentroid(const UACFSurfaceAsset* InAsset, const FACFTriangle& Triangle);
	static FBox ComputeTriangleRefAABB(const UACFSurfaceAsset* InAsset, const FACFTriangle& Triangle);
	static FBox ComputeOrderedTriangleRangeRefAABB(
		const UACFSurfaceAsset* InAsset,
		const TArray<int32>& OrderedTriangleIndices,
		int32 FirstTriangle,
		int32 TriangleCount);
};