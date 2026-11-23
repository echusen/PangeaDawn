// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ACFSurfaceRaycastTypes.generated.h"

USTRUCT()
struct FACFSurfaceRaycastTriangleMeta
{
	GENERATED_BODY()

	UPROPERTY()
	int32 DominantBoneIndex = INDEX_NONE;

	UPROPERTY()
	int32 RegionIndex = INDEX_NONE;

	bool IsValid() const
	{
		return DominantBoneIndex != INDEX_NONE && RegionIndex != INDEX_NONE;
	}
};

USTRUCT()
struct FACFSurfaceRaycastAABBPiece
{
	GENERATED_BODY()

	UPROPERTY()
	int32 BoneIndex = INDEX_NONE;

	UPROPERTY()
	FBox RefBoneLocalAABB = FBox(ForceInit);

	bool IsValid() const
	{
		return BoneIndex != INDEX_NONE && RefBoneLocalAABB.IsValid;
	}
};

USTRUCT()
struct FACFSurfaceRaycastBVHNode
{
	GENERATED_BODY()

	UPROPERTY()
	int32 LeftChild = INDEX_NONE;

	UPROPERTY()
	int32 RightChild = INDEX_NONE;

	UPROPERTY()
	int32 FirstTriangle = 0;

	UPROPERTY()
	int32 TriangleCount = 0;

	UPROPERTY()
	FBox RefAABB = FBox(ForceInit);

	UPROPERTY()
	TArray<FACFSurfaceRaycastAABBPiece> AABBPieces;

	bool IsLeaf() const
	{
		return LeftChild == INDEX_NONE && RightChild == INDEX_NONE;
	}

	bool HasValidTriangleRange() const
	{
		return FirstTriangle >= 0 && TriangleCount > 0;
	}
};

USTRUCT()
struct FACFSurfaceRaycastBVH
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FACFSurfaceRaycastBVHNode> Nodes;

	UPROPERTY()
	TArray<int32> OrderedTriangleIndices;

	UPROPERTY()
	int32 RootNodeIndex = INDEX_NONE;

	void Reset()
	{
		Nodes.Reset();
		OrderedTriangleIndices.Reset();
		RootNodeIndex = INDEX_NONE;
	}

	bool IsValid() const
	{
		return RootNodeIndex != INDEX_NONE &&
			Nodes.IsValidIndex(RootNodeIndex) &&
			OrderedTriangleIndices.Num() > 0;
	}
};

USTRUCT()
struct FACFSurfaceRaycastRegion
{
	GENERATED_BODY()

	UPROPERTY()
	int32 RegionIndex = INDEX_NONE;

	UPROPERTY()
	int32 RepresentativeBoneIndex = INDEX_NONE;

	UPROPERTY()
	FName RepresentativeBoneName = NAME_None;

	UPROPERTY()
	TArray<int32> TriangleIndices;

	UPROPERTY()
	TArray<FACFSurfaceRaycastAABBPiece> AABBPieces;

	UPROPERTY()
	FACFSurfaceRaycastBVH TriangleBVH;

	UPROPERTY()
	TArray<int32> AdjacentRegionIndices;

	bool IsValid() const
	{
		return RegionIndex != INDEX_NONE &&
			RepresentativeBoneIndex != INDEX_NONE &&
			RepresentativeBoneName != NAME_None;
	}
};

USTRUCT()
struct FACFSurfaceRaycastData
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FACFSurfaceRaycastTriangleMeta> TriangleMetas;

	UPROPERTY()
	TArray<FTransform> RefPoseBoneCS;

	UPROPERTY()
	TArray<FName> BoneNames;

	UPROPERTY()
	TArray<FACFSurfaceRaycastRegion> Regions;

	UPROPERTY()
	TArray<int32> BoneToRegionIndex;

	void Reset()
	{
		TriangleMetas.Reset();
		RefPoseBoneCS.Reset();
		BoneNames.Reset();
		Regions.Reset();
		BoneToRegionIndex.Reset();
	}

	bool HasValidData(int32 SurfaceTriangleCount) const
	{
		return SurfaceTriangleCount > 0 &&
			TriangleMetas.Num() == SurfaceTriangleCount &&
			RefPoseBoneCS.Num() > 0 &&
			BoneNames.Num() == RefPoseBoneCS.Num() &&
			BoneToRegionIndex.Num() == RefPoseBoneCS.Num() &&
			Regions.Num() > 0;
	}
};

struct FACFSurfaceRaycastRuntimeCache
{
	bool bInitialized = false;
	
	int32 PoseVersion = INDEX_NONE;
	uint32 BoneTransformRevision = MAX_uint32;
	
	uint64 CacheFrame = 0;
	int32 CacheVersion = 0;

	TArray<FMatrix44f> RefToLocalMatrices;
	FTransform ComponentTransform = FTransform::Identity;

	TArray<FBox> RegionAABBsWS;
	TArray<int32> RegionAABBVersions;

	TArray<TArray<FBox>> NodeAABBsWS;
	TArray<TArray<int32>> NodeAABBVersions;

	TArray<FVector> MicroSkinnedPositionsWS;
	TArray<FVector> MicroSkinnedNormalsWS;
	TArray<int32> MicroSkinnedVertexVersions;

	void Reset()
	{
		bInitialized = false;
		
		PoseVersion = INDEX_NONE;
		BoneTransformRevision = MAX_uint32;
		CacheFrame = 0;
		CacheVersion = 0;
		
		RefToLocalMatrices.Reset();
		ComponentTransform = FTransform::Identity;

		RegionAABBsWS.Reset();
		RegionAABBVersions.Reset();

		NodeAABBsWS.Reset();
		NodeAABBVersions.Reset();

		MicroSkinnedPositionsWS.Reset();
		MicroSkinnedNormalsWS.Reset();
		MicroSkinnedVertexVersions.Reset();
	}
};

struct FACFCandidateRegion
{
	int32 RegionIndex = INDEX_NONE;
	float EntryDistance = 0.0;
};
