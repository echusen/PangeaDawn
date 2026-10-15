// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "ACFSurfaceMemory.h"

#include "ACFSurfaceAsset.h"
#include "ACFSurfaceRaycastTypes.h"

DEFINE_STAT(STAT_ClimbingSystemClimbableCount);
DEFINE_STAT(STAT_ClimbingSystemTotalMemory);
DEFINE_STAT(STAT_ClimbingSystemMeshDataMemory);
DEFINE_STAT(STAT_ClimbingSystemCollisionDataMemory);
DEFINE_STAT(STAT_ClimbingSystemRuntimeCacheMemory);

namespace
{
	SIZE_T GetRaycastBVHAllocatedSize(const FACFSurfaceRaycastBVH& BVH)
	{
		SIZE_T Bytes = 0;

		Bytes += BVH.Nodes.GetAllocatedSize();
		Bytes += BVH.OrderedTriangleIndices.GetAllocatedSize();

		for (const FACFSurfaceRaycastBVHNode& Node : BVH.Nodes)
		{
			Bytes += Node.AABBPieces.GetAllocatedSize();
		}

		return Bytes;
	}

	SIZE_T GetRaycastDataHeapAllocatedSize(const FACFSurfaceRaycastData& RaycastData)
	{
		SIZE_T Bytes = 0;

		Bytes += RaycastData.TriangleMetas.GetAllocatedSize();
		Bytes += RaycastData.RefPoseBoneCS.GetAllocatedSize();
		Bytes += RaycastData.BoneNames.GetAllocatedSize();
		Bytes += RaycastData.BoneToRegionIndex.GetAllocatedSize();
		Bytes += RaycastData.Regions.GetAllocatedSize();

		for (const FACFSurfaceRaycastRegion& Region : RaycastData.Regions)
		{
			Bytes += Region.TriangleIndices.GetAllocatedSize();
			Bytes += Region.AABBPieces.GetAllocatedSize();
			Bytes += Region.AdjacentRegionIndices.GetAllocatedSize();
			Bytes += GetRaycastBVHAllocatedSize(Region.TriangleBVH);
		}

		return Bytes;
	}
}

SIZE_T ACFSurfaceMemory::GetSurfaceAssetAllocatedSize(const UACFSurfaceAsset* SurfaceAsset)
{
	if (!SurfaceAsset) return 0;

	SIZE_T Bytes = sizeof(UACFSurfaceAsset);

	Bytes += SurfaceAsset->Triangles.GetAllocatedSize();
	Bytes += SurfaceAsset->Vertices.GetAllocatedSize();

	for (const FACFTriangleVertex& Vertex : SurfaceAsset->Vertices)
	{
		Bytes += Vertex.BoneIndices.GetAllocatedSize();
		Bytes += Vertex.BoneWeights.GetAllocatedSize();
	}

	Bytes += GetRaycastDataHeapAllocatedSize(SurfaceAsset->RaycastData);

	return Bytes;
}

SIZE_T ACFSurfaceMemory::GetRaycastDataAllocatedSize(const UACFSurfaceAsset* SurfaceAsset)
{
	if (!SurfaceAsset) return 0;

	return sizeof(FACFSurfaceRaycastData) + GetRaycastDataHeapAllocatedSize(SurfaceAsset->RaycastData);
}

SIZE_T ACFSurfaceMemory::GetRuntimeCacheAllocatedSize(const FACFSurfaceRaycastRuntimeCache& RuntimeCache)
{
	SIZE_T Bytes = sizeof(FACFSurfaceRaycastRuntimeCache);

	Bytes += RuntimeCache.RefToLocalMatrices.GetAllocatedSize();
	Bytes += RuntimeCache.RegionAABBsWS.GetAllocatedSize();
	Bytes += RuntimeCache.RegionAABBVersions.GetAllocatedSize();

	Bytes += RuntimeCache.NodeAABBsWS.GetAllocatedSize();
	for (const TArray<FBox>& NodeAABBs : RuntimeCache.NodeAABBsWS)
	{
		Bytes += NodeAABBs.GetAllocatedSize();
	}

	Bytes += RuntimeCache.NodeAABBVersions.GetAllocatedSize();
	for (const TArray<int32>& NodeVersions : RuntimeCache.NodeAABBVersions)
	{
		Bytes += NodeVersions.GetAllocatedSize();
	}

	Bytes += RuntimeCache.MicroSkinnedPositionsWS.GetAllocatedSize();
	Bytes += RuntimeCache.MicroSkinnedNormalsWS.GetAllocatedSize();
	Bytes += RuntimeCache.MicroSkinnedVertexVersions.GetAllocatedSize();

	return Bytes;
}
