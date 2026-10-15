// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "ACFSurfaceRaycastRuntime.h"

#include "ACFFreeClimbTypes.h"
#include "ACFSurfaceMath.h"
#include "ACFSurfaceRaycastTypes.h"
#include "Components/SkeletalMeshComponent.h"
#include "ACFSurfaceAsset.h"

namespace
{
	void ACFGetBoxCorner(const FBox& Box, int32 CornerIndex, FVector& OutCorner)
	{
		OutCorner.X = (CornerIndex & 1) ? Box.Max.X : Box.Min.X;
		OutCorner.Y = (CornerIndex & 2) ? Box.Max.Y : Box.Min.Y;
		OutCorner.Z = (CornerIndex & 4) ? Box.Max.Z : Box.Min.Z;
	}

    bool MatchesQueryFlags(
        const FACFTriangle& Triangle,
        const uint8 RequiredQueryFlags)
    {
        return (Triangle.QueryFlags & RequiredQueryFlags) == RequiredQueryFlags;
    }
}

bool FACFSurfaceRaycastRuntime::InitializeCache(const UACFSurfaceAsset* SurfaceAsset, FACFSurfaceRaycastRuntimeCache& RuntimeCache)
{
	if (!SurfaceAsset) return false;
	
	const FACFSurfaceRaycastData& RaycastData = SurfaceAsset->RaycastData;
	if (!RaycastData.HasValidData(SurfaceAsset->Triangles.Num())) { return false; }
	
	const int32 RegionCount = RaycastData.Regions.Num();
	const int32 VertexCount = SurfaceAsset->Vertices.Num();
	
	RuntimeCache.RegionAABBsWS.SetNum(RegionCount);
	RuntimeCache.RegionAABBVersions.Init(INDEX_NONE, RegionCount);
	
	RuntimeCache.NodeAABBsWS.SetNum(RegionCount);
	RuntimeCache.NodeAABBVersions.SetNum(RegionCount);
	
	for (int32 RegionIndex = 0; RegionIndex < RegionCount; ++RegionIndex)
	{
		const int32 NodeCount = RaycastData.Regions[RegionIndex].TriangleBVH.Nodes.Num();
		RuntimeCache.NodeAABBsWS[RegionIndex].SetNum(NodeCount);
		RuntimeCache.NodeAABBVersions[RegionIndex].Init(INDEX_NONE, NodeCount);
	}
	
	RuntimeCache.MicroSkinnedPositionsWS.SetNum(VertexCount);
	RuntimeCache.MicroSkinnedNormalsWS.SetNum(VertexCount);
	RuntimeCache.MicroSkinnedVertexVersions.Init(INDEX_NONE, VertexCount);
	RuntimeCache.bInitialized = true;
	
	return true;
}

bool FACFSurfaceRaycastRuntime::PrepareCacheForPose(const UACFSurfaceAsset* SurfaceAsset, const USkeletalMeshComponent* MeshComponent, FACFSurfaceRaycastRuntimeCache& RuntimeCache)
{
	if (!SurfaceAsset || !MeshComponent) return false;

	const FACFSurfaceRaycastData& RaycastData = SurfaceAsset->RaycastData;
	if (!RaycastData.HasValidData(SurfaceAsset->Triangles.Num())) return false;
	
	if (!RuntimeCache.bInitialized)
	{
		if (!InitializeCache(SurfaceAsset, RuntimeCache)) return false;
	}
	
	RuntimeCache.CacheFrame = GFrameCounter;
	
	const uint32 BoneTransformRevision = MeshComponent->GetBoneTransformRevisionNumber();
	const FTransform ComponentTransform = MeshComponent->GetComponentTransform();
	const bool bBoneTransformChanged = RuntimeCache.BoneTransformRevision != BoneTransformRevision;
	const bool bComponentTransformChanged = !RuntimeCache.ComponentTransform.Equals(ComponentTransform, 0.0);
	if (RuntimeCache.PoseVersion != INDEX_NONE && !bBoneTransformChanged && !bComponentTransformChanged)
	{
		RuntimeCache.CacheFrame = GFrameCounter;
		return true;
	}
		
	MeshComponent->GetCurrentRefToLocalMatrices(RuntimeCache.RefToLocalMatrices, 0);

	if (RuntimeCache.RefToLocalMatrices.Num() < RaycastData.RefPoseBoneCS.Num()) return false;
	
	++RuntimeCache.CacheVersion;
	
	RuntimeCache.BoneTransformRevision = BoneTransformRevision;
	RuntimeCache.ComponentTransform = ComponentTransform;
	RuntimeCache.PoseVersion = RuntimeCache.CacheVersion;
	
	RuntimeCache.CacheFrame = GFrameCounter;
	
	return true;
}

void FACFSurfaceRaycastRuntime::CollectCandidateRegions(const UACFSurfaceAsset* SurfaceAsset, FACFSurfaceRaycastRuntimeCache& RuntimeCache,
                                                          const FACFSurfaceBVHQuery& Query, TArray<::FACFCandidateRegion>& OutRegions)
{
	OutRegions.Reset();

	if (!SurfaceAsset) return;
	if (!SurfaceAsset->RaycastData.HasValidData(SurfaceAsset->Triangles.Num())) return;
	
	const TArray<FACFSurfaceRaycastRegion>& Regions = SurfaceAsset->RaycastData.Regions;
	OutRegions.Reserve(Regions.Num());
	
	for (int32 RegionIndex = 0; RegionIndex < Regions.Num(); ++RegionIndex)
	{
		FBox RegionAABBWs(ForceInit);
		if (!EnsureRegionAABBUpdated(SurfaceAsset, RegionIndex, RuntimeCache, RegionAABBWs)) continue;
		
		if (Query.BoundsExpandRadius > 0.0f) RegionAABBWs = RegionAABBWs.ExpandBy(Query.BoundsExpandRadius);
		
		float EntryDistance = 0.0f;
		if (!ACFSurfaceMath::SegmentIntersectsAABB(Query.Start, Query.End, RegionAABBWs, EntryDistance)) continue;
		
		const float SegmentLength = FVector::Distance(Query.Start, Query.End);
		if (SegmentLength <= KINDA_SMALL_NUMBER) return;
		
		FACFCandidateRegion CandidateRegion;
		CandidateRegion.RegionIndex = RegionIndex;
		CandidateRegion.EntryDistance = EntryDistance;
		
		OutRegions.Add(CandidateRegion);
	}
	
	OutRegions.Sort([](const FACFCandidateRegion& A, const FACFCandidateRegion& B)
	{
		if (FMath::IsNearlyEqual(A.EntryDistance, B.EntryDistance))
		{
			return A.RegionIndex < B.RegionIndex;
		}
		return A.EntryDistance < B.EntryDistance;
	});
}

void FACFSurfaceRaycastRuntime::CollectCandidateRegions(
    const UACFSurfaceAsset* SurfaceAsset,
    FACFSurfaceRaycastRuntimeCache& RuntimeCache,
    const FACFSurfaceCapsuleBVHQuery& Query,
    TArray<FACFCandidateRegion>& OutRegions)
{
    OutRegions.Reset();

    if (!SurfaceAsset) return;
    if (!SurfaceAsset->RaycastData.HasValidData(SurfaceAsset->Triangles.Num())) return;

    const TArray<FACFSurfaceRaycastRegion>& Regions = SurfaceAsset->RaycastData.Regions;
    OutRegions.Reserve(Regions.Num());

    for (int32 RegionIndex = 0; RegionIndex < Regions.Num(); ++RegionIndex)
    {
        FBox RegionAABBWS(ForceInit);
        if (!EnsureRegionAABBUpdated(SurfaceAsset, RegionIndex, RuntimeCache, RegionAABBWS)) continue;

        FBox CapsuleSweepAABB(ForceInit);
        CapsuleSweepAABB += Query.StartA;
        CapsuleSweepAABB += Query.StartB;
        CapsuleSweepAABB += Query.EndA;
        CapsuleSweepAABB += Query.EndB;
        CapsuleSweepAABB = CapsuleSweepAABB.ExpandBy(Query.Radius);

        if (!CapsuleSweepAABB.Intersect(RegionAABBWS)) continue;

        FACFCandidateRegion Candidate;
        Candidate.RegionIndex = RegionIndex;
        Candidate.EntryDistance = FVector::DistSquared(CapsuleSweepAABB.GetCenter(), RegionAABBWS.GetCenter());

        OutRegions.Add(Candidate);
    }

    OutRegions.Sort([](const FACFCandidateRegion& A, const FACFCandidateRegion& B)
    {
        if (FMath::IsNearlyEqual(A.EntryDistance, B.EntryDistance))
        {
            return A.RegionIndex < B.RegionIndex;
        }
        return A.EntryDistance < B.EntryDistance;
    });
}

void FACFSurfaceRaycastRuntime::RaycastRegionBVHNearFirst(const UACFSurfaceAsset* SurfaceAsset, int32 RegionIndex, FACFSurfaceRaycastRuntimeCache& RuntimeCache, 
	const FACFSurfaceQuery& Query, float& InOutBestDistance, FACFSurfaceHitResult& InOutBestHit)
{
	if (!SurfaceAsset) return;
	if (!SurfaceAsset->RaycastData.Regions.IsValidIndex(RegionIndex)) return;

	const FACFSurfaceRaycastRegion& Region = SurfaceAsset->RaycastData.Regions[RegionIndex];
	const FACFSurfaceRaycastBVH& BVH = Region.TriangleBVH;
	if (!BVH.IsValid()) return;
	
	TArray<int32> Stack;
	Stack.Reserve(64);
	Stack.Add(BVH.RootNodeIndex);
	const FACFSurfaceBVHQuery BVHQuery{ Query.Start, Query.End };

	while (!Stack.IsEmpty())
	{
		const int32 NodeIndex = Stack.Pop(EAllowShrinking::No);

		float NodeEntryDistance = 0.0f;
		if (!GetNodeEntryDistance(
			SurfaceAsset,
			RegionIndex,
			NodeIndex,
			RuntimeCache,
			BVHQuery,
			NodeEntryDistance))
		{
			continue;
		}

		if (NodeEntryDistance > InOutBestDistance) continue;

		const FACFSurfaceRaycastBVHNode& Node = BVH.Nodes[NodeIndex];

		if (Node.IsLeaf())
		{
			for (int32 LocalIndex = 0; LocalIndex < Node.TriangleCount; ++LocalIndex)
			{
				const int32 OrderedIndex = Node.FirstTriangle + LocalIndex;
				if (!BVH.OrderedTriangleIndices.IsValidIndex(OrderedIndex)) continue;

				RaycastTriangle(
					SurfaceAsset,
					BVH.OrderedTriangleIndices[OrderedIndex],
					RuntimeCache,
					Query,
					InOutBestDistance,
					InOutBestHit);
			}

			continue;
		}

		float LeftEntryDistance = TNumericLimits<float>::Max();
		float RightEntryDistance = TNumericLimits<float>::Max();

		const bool bHasLeft = Node.LeftChild != INDEX_NONE &&
			GetNodeEntryDistance(
				SurfaceAsset,
				RegionIndex,
				Node.LeftChild,
				RuntimeCache,
				BVHQuery,
				LeftEntryDistance) &&
			LeftEntryDistance <= InOutBestDistance;

		const bool bHasRight = Node.RightChild != INDEX_NONE &&
			GetNodeEntryDistance(
				SurfaceAsset,
				RegionIndex,
				Node.RightChild,
				RuntimeCache,
				BVHQuery,
				RightEntryDistance) &&
			RightEntryDistance <= InOutBestDistance;

		if (bHasLeft && bHasRight)
		{
			if (LeftEntryDistance <= RightEntryDistance)
			{
				Stack.Add(Node.RightChild);
				Stack.Add(Node.LeftChild);
			}
			else
			{
				Stack.Add(Node.LeftChild);
				Stack.Add(Node.RightChild);
			}
		}
		else if (bHasLeft) Stack.Add(Node.LeftChild);
		else if (bHasRight) Stack.Add(Node.RightChild);
	}
}

void FACFSurfaceRaycastRuntime::SweepSphereRegionBVHNearFirst(
	const UACFSurfaceAsset* SurfaceAsset,
	int32 RegionIndex, FACFSurfaceRaycastRuntimeCache& RuntimeCache,
	const FACFSurfaceSphereSweepQuery& Query,
	float& InOutBestDistance,
	FACFSurfaceHitResult& InOutBestHit)
{
	if (!SurfaceAsset) return;
	if (!SurfaceAsset->RaycastData.Regions.IsValidIndex(RegionIndex)) return;
	
	const FACFSurfaceRaycastRegion& Region = SurfaceAsset->RaycastData.Regions[RegionIndex];
	const FACFSurfaceRaycastBVH& BVH = Region.TriangleBVH;
	if (!BVH.IsValid()) return;
	
	const FACFSurfaceBVHQuery BVHQuery{ Query.Start, Query.End, Query.Radius };

	TArray<int32> Stack;
	Stack.Reserve(64);
	Stack.Add(BVH.RootNodeIndex);
	
	while (!Stack.IsEmpty())
    {
        const int32 NodeIndex = Stack.Pop(EAllowShrinking::No);

        float NodeEntryDistance = 0.0f;
        if (!GetNodeEntryDistance(SurfaceAsset, RegionIndex, NodeIndex, RuntimeCache, BVHQuery, NodeEntryDistance))
        {
            continue;
        }

        if (NodeEntryDistance > InOutBestDistance) continue;

        const FACFSurfaceRaycastBVHNode& Node = BVH.Nodes[NodeIndex];

        if (Node.IsLeaf())
        {
            for (int32 LocalIndex = 0; LocalIndex < Node.TriangleCount; ++LocalIndex)
            {
                const int32 OrderedIndex = Node.FirstTriangle + LocalIndex;
                if (!BVH.OrderedTriangleIndices.IsValidIndex(OrderedIndex)) continue;

                SweepSphereTriangle(SurfaceAsset, BVH.OrderedTriangleIndices[OrderedIndex], RuntimeCache, Query, InOutBestDistance, InOutBestHit);
            }

            continue;
        }

        float LeftEntryDistance = TNumericLimits<float>::Max();
        float RightEntryDistance = TNumericLimits<float>::Max();

        const bool bHasLeft = Node.LeftChild != INDEX_NONE &&
            GetNodeEntryDistance(
                SurfaceAsset,
                RegionIndex,
                Node.LeftChild,
                RuntimeCache,
                BVHQuery,
                LeftEntryDistance) &&
            LeftEntryDistance <= InOutBestDistance;

        const bool bHasRight = Node.RightChild != INDEX_NONE &&
            GetNodeEntryDistance(
                SurfaceAsset,
                RegionIndex,
                Node.RightChild,
                RuntimeCache,
                BVHQuery,
                RightEntryDistance) &&
            RightEntryDistance <= InOutBestDistance;

        if (bHasLeft && bHasRight)
        {
            if (LeftEntryDistance <= RightEntryDistance)
            {
                Stack.Add(Node.RightChild);
                Stack.Add(Node.LeftChild);
            }
            else
            {
                Stack.Add(Node.LeftChild);
                Stack.Add(Node.RightChild);
            }
        }
        else if (bHasLeft) Stack.Add(Node.LeftChild);
        else if (bHasRight) Stack.Add(Node.RightChild);
    }
}

void FACFSurfaceRaycastRuntime::SweepSphereRegionBVHAll(
	const UACFSurfaceAsset* SurfaceAsset,
	int32 RegionIndex,
	FACFSurfaceRaycastRuntimeCache& RuntimeCache,
	const FACFSurfaceSphereSweepQuery& Query,
	TArray<FACFSurfaceHitResult>& OutHits)
{
	if (!SurfaceAsset || !SurfaceAsset->RaycastData.Regions.IsValidIndex(RegionIndex)) return;

	const FACFSurfaceRaycastRegion& Region = SurfaceAsset->RaycastData.Regions[RegionIndex];
	const FACFSurfaceRaycastBVH& BVH = Region.TriangleBVH;
	if (!BVH.IsValid()) return;

	const FACFSurfaceBVHQuery BVHQuery{ Query.Start, Query.End, FMath::Max(Query.Radius, 0.0f) };
	TArray<int32> Stack;
	Stack.Reserve(64);
	Stack.Add(BVH.RootNodeIndex);

	while (!Stack.IsEmpty())
	{
		const int32 NodeIndex = Stack.Pop(EAllowShrinking::No);
		float NodeEntryDistance = 0.0f;
		if (!GetNodeEntryDistance(SurfaceAsset, RegionIndex, NodeIndex, RuntimeCache, BVHQuery, NodeEntryDistance)) continue;

		const FACFSurfaceRaycastBVHNode& Node = BVH.Nodes[NodeIndex];
		if (Node.IsLeaf())
		{
			for (int32 LocalIndex = 0; LocalIndex < Node.TriangleCount; ++LocalIndex)
			{
				const int32 OrderedIndex = Node.FirstTriangle + LocalIndex;
				if (!BVH.OrderedTriangleIndices.IsValidIndex(OrderedIndex)) continue;
				SweepSphereTriangleAll(SurfaceAsset, BVH.OrderedTriangleIndices[OrderedIndex], RuntimeCache, Query, OutHits);
			}
			continue;
		}

		if (Node.LeftChild != INDEX_NONE) Stack.Add(Node.LeftChild);
		if (Node.RightChild != INDEX_NONE) Stack.Add(Node.RightChild);
	}
}

void FACFSurfaceRaycastRuntime::SweepCapsuleRegionBVHNearFirst(
    const UACFSurfaceAsset* SurfaceAsset,
    int32 RegionIndex,
    FACFSurfaceRaycastRuntimeCache& RuntimeCache,
    const FACFSurfaceCapsuleSweepQuery& Query,
    float& InOutBestDistance,
    FACFSurfaceCapsuleHitResult& InOutBestHit)
{
    if (!SurfaceAsset) return;
    if (!SurfaceAsset->RaycastData.Regions.IsValidIndex(RegionIndex)) return;

    const FACFSurfaceRaycastRegion& Region = SurfaceAsset->RaycastData.Regions[RegionIndex];
    const FACFSurfaceRaycastBVH& BVH = Region.TriangleBVH;
    if (!BVH.IsValid()) return;

    FACFSurfaceCapsuleBVHQuery BVHQuery = {};
    BVHQuery.StartA = Query.StartA;
    BVHQuery.StartB = Query.StartB;
    BVHQuery.EndA = Query.EndA;
    BVHQuery.EndB = Query.EndB;
    BVHQuery.Radius = Query.Radius;

    TArray<int32> Stack;
    Stack.Reserve(64);
    Stack.Add(BVH.RootNodeIndex);

    while (!Stack.IsEmpty())
    {
        const int32 NodeIndex = Stack.Pop(EAllowShrinking::No);

        float NodeEntryDistance = 0.0f;
        if (!GetCapsuleNodeEntryDistance(SurfaceAsset, RegionIndex, NodeIndex, RuntimeCache, BVHQuery, NodeEntryDistance))
        {
            continue;
        }

        const FACFSurfaceRaycastBVHNode& Node = BVH.Nodes[NodeIndex];

        if (Node.IsLeaf())
        {
            for (int32 LocalIndex = 0; LocalIndex < Node.TriangleCount; ++LocalIndex)
            {
                const int32 OrderedIndex = Node.FirstTriangle + LocalIndex;
                if (!BVH.OrderedTriangleIndices.IsValidIndex(OrderedIndex)) continue;

                SweepCapsuleTriangle(SurfaceAsset, BVH.OrderedTriangleIndices[OrderedIndex], RuntimeCache, Query, InOutBestDistance, InOutBestHit);
            }

            continue;
        }

        float LeftEntryDistance = TNumericLimits<float>::Max();
        float RightEntryDistance = TNumericLimits<float>::Max();

        const bool bHasLeft = Node.LeftChild != INDEX_NONE &&
            GetCapsuleNodeEntryDistance(
                SurfaceAsset,
                RegionIndex,
                Node.LeftChild,
                RuntimeCache,
                BVHQuery,
                LeftEntryDistance);

        const bool bHasRight = Node.RightChild != INDEX_NONE &&
            GetCapsuleNodeEntryDistance(
                SurfaceAsset,
                RegionIndex,
                Node.RightChild,
                RuntimeCache,
                BVHQuery,
                RightEntryDistance);

        if (bHasLeft && bHasRight)
        {
            if (LeftEntryDistance <= RightEntryDistance)
            {
                Stack.Add(Node.RightChild);
                Stack.Add(Node.LeftChild);
            }
            else
            {
                Stack.Add(Node.LeftChild);
                Stack.Add(Node.RightChild);
            }
        }
        else if (bHasLeft) Stack.Add(Node.LeftChild);
        else if (bHasRight) Stack.Add(Node.RightChild);
    }
}

void FACFSurfaceRaycastRuntime::SweepCapsuleRegionBVHAll(
    const UACFSurfaceAsset* SurfaceAsset,
    int32 RegionIndex,
    FACFSurfaceRaycastRuntimeCache& RuntimeCache,
    const FACFSurfaceCapsuleSweepQuery& Query,
    TArray<FACFSurfaceCapsuleHitResult>& OutHits)
{
    if (!SurfaceAsset || !SurfaceAsset->RaycastData.Regions.IsValidIndex(RegionIndex)) return;

    const FACFSurfaceRaycastRegion& Region = SurfaceAsset->RaycastData.Regions[RegionIndex];
    const FACFSurfaceRaycastBVH& BVH = Region.TriangleBVH;
    if (!BVH.IsValid()) return;

    FACFSurfaceCapsuleBVHQuery BVHQuery;
    BVHQuery.StartA = Query.StartA;
    BVHQuery.StartB = Query.StartB;
    BVHQuery.EndA = Query.EndA;
    BVHQuery.EndB = Query.EndB;
    BVHQuery.Radius = Query.Radius;

    TArray<int32> Stack;
    Stack.Reserve(64);
    Stack.Add(BVH.RootNodeIndex);

    while (!Stack.IsEmpty())
    {
        const int32 NodeIndex = Stack.Pop(EAllowShrinking::No);

        float NodeEntryDistance = 0.0f;
        if (!GetCapsuleNodeEntryDistance(SurfaceAsset, RegionIndex, NodeIndex, RuntimeCache, BVHQuery, NodeEntryDistance))
        {
            continue;
        }

        const FACFSurfaceRaycastBVHNode& Node = BVH.Nodes[NodeIndex];
        if (Node.IsLeaf())
        {
            for (int32 LocalIndex = 0; LocalIndex < Node.TriangleCount; ++LocalIndex)
            {
                const int32 OrderedIndex = Node.FirstTriangle + LocalIndex;
                if (!BVH.OrderedTriangleIndices.IsValidIndex(OrderedIndex)) continue;

                SweepCapsuleTriangleAll(SurfaceAsset, BVH.OrderedTriangleIndices[OrderedIndex], RuntimeCache, Query, OutHits);
            }

            continue;
        }

        if (Node.LeftChild != INDEX_NONE) Stack.Add(Node.LeftChild);
        if (Node.RightChild != INDEX_NONE) Stack.Add(Node.RightChild);
    }
}

bool FACFSurfaceRaycastRuntime::SkinVertexWS(
	const UACFSurfaceAsset* SurfaceAsset,
	int32 VertexIndex,
	FACFSurfaceRaycastRuntimeCache& RuntimeCache,
	FVector& OutPositionWS,
	FVector& OutNormalWS)
{
	OutPositionWS = FVector::ZeroVector;
	OutNormalWS = FVector::ZeroVector;

	if (!SurfaceAsset) return false;
	if (!SurfaceAsset->Vertices.IsValidIndex(VertexIndex)) return false;

	if (RuntimeCache.MicroSkinnedVertexVersions.IsValidIndex(VertexIndex) &&
		RuntimeCache.MicroSkinnedVertexVersions[VertexIndex] == RuntimeCache.PoseVersion)
	{
		OutPositionWS = RuntimeCache.MicroSkinnedPositionsWS[VertexIndex];
		OutNormalWS = RuntimeCache.MicroSkinnedNormalsWS[VertexIndex];
		return true;
	}

	if (!RuntimeCache.MicroSkinnedPositionsWS.IsValidIndex(VertexIndex) ||
		!RuntimeCache.MicroSkinnedNormalsWS.IsValidIndex(VertexIndex) ||
		!RuntimeCache.MicroSkinnedVertexVersions.IsValidIndex(VertexIndex))
	{
		return false;
	}

	const FACFTriangleVertex& Vertex = SurfaceAsset->Vertices[VertexIndex];

	const int32 NumInfluences = FMath::Min(Vertex.BoneIndices.Num(), Vertex.BoneWeights.Num());
	if (NumInfluences <= 0) return false;

	float WeightSum = 0.0f;

	for (int32 InfluenceIndex = 0; InfluenceIndex < NumInfluences; ++InfluenceIndex)
	{
		const int32 BoneIndex = Vertex.BoneIndices[InfluenceIndex];
		const float Weight = Vertex.BoneWeights[InfluenceIndex];

		if (BoneIndex == INDEX_NONE || Weight <= 0.0f) continue;
		if (!RuntimeCache.RefToLocalMatrices.IsValidIndex(BoneIndex)) continue;

		const FMatrix44f& RefToLocal = RuntimeCache.RefToLocalMatrices[BoneIndex];

		const FVector CurrentCS = FVector(RefToLocal.TransformPosition(FVector3f(Vertex.Position)));
		const FVector CurrentNormalCS = FVector(RefToLocal.TransformVector(FVector3f(Vertex.Normal)));

		OutPositionWS += RuntimeCache.ComponentTransform.TransformPosition(CurrentCS) * Weight;
		OutNormalWS += RuntimeCache.ComponentTransform.TransformVectorNoScale(CurrentNormalCS) * Weight;

		WeightSum += Weight;
	}

	if (WeightSum <= UE_KINDA_SMALL_NUMBER) return false;

	if (!FMath::IsNearlyEqual(WeightSum, 1.0f))
	{
		const float InvWeightSum = 1.0f / WeightSum;
		OutPositionWS *= InvWeightSum;
		OutNormalWS *= InvWeightSum;
	}

	OutNormalWS.Normalize();

	RuntimeCache.MicroSkinnedPositionsWS[VertexIndex] = OutPositionWS;
	RuntimeCache.MicroSkinnedNormalsWS[VertexIndex] = OutNormalWS;
	RuntimeCache.MicroSkinnedVertexVersions[VertexIndex] = RuntimeCache.PoseVersion;

	return true;
}

bool FACFSurfaceRaycastRuntime::EnsureRegionAABBUpdated(const UACFSurfaceAsset* SurfaceAsset, int32 RegionIndex, FACFSurfaceRaycastRuntimeCache& RuntimeCache, FBox& OutAABBWS)
{
	if (!SurfaceAsset->RaycastData.Regions.IsValidIndex(RegionIndex)) return false;
	if (!RuntimeCache.RegionAABBsWS.IsValidIndex(RegionIndex)) return false;
	if (!RuntimeCache.RegionAABBVersions.IsValidIndex(RegionIndex)) return false;

	if (RuntimeCache.RegionAABBVersions[RegionIndex] == RuntimeCache.PoseVersion)
	{
		OutAABBWS = RuntimeCache.RegionAABBsWS[RegionIndex];
		return OutAABBWS.IsValid != 0;
	}

	const FACFSurfaceRaycastRegion& Region = SurfaceAsset->RaycastData.Regions[RegionIndex];

	FBox NewAABBWS(ForceInit);
	if (!BuildAABBFromPiecesWS(SurfaceAsset, Region.AABBPieces, RuntimeCache, NewAABBWS))
	{
		return false;
	}

	RuntimeCache.RegionAABBsWS[RegionIndex] = NewAABBWS;
	RuntimeCache.RegionAABBVersions[RegionIndex] = RuntimeCache.PoseVersion;

	OutAABBWS = NewAABBWS;
	return OutAABBWS.IsValid != 0;
}

bool FACFSurfaceRaycastRuntime::EnsureNodeAABBUpdated(const UACFSurfaceAsset* SurfaceAsset, int32 RegionIndex, int32 NodeIndex, FACFSurfaceRaycastRuntimeCache& RuntimeCache, FBox& OutAABBWS)
{
	if (!SurfaceAsset->RaycastData.Regions.IsValidIndex(RegionIndex)) return false;
	if (!RuntimeCache.NodeAABBsWS.IsValidIndex(RegionIndex)) return false;
	if (!RuntimeCache.NodeAABBVersions.IsValidIndex(RegionIndex)) return false;
	if (!RuntimeCache.NodeAABBsWS[RegionIndex].IsValidIndex(NodeIndex)) return false;
	if (!RuntimeCache.NodeAABBVersions[RegionIndex].IsValidIndex(NodeIndex)) return false;

	if (RuntimeCache.NodeAABBVersions[RegionIndex][NodeIndex] == RuntimeCache.PoseVersion)
	{
		OutAABBWS = RuntimeCache.NodeAABBsWS[RegionIndex][NodeIndex];
		return OutAABBWS.IsValid != 0;
	}

	const FACFSurfaceRaycastRegion& Region = SurfaceAsset->RaycastData.Regions[RegionIndex];
	if (!Region.TriangleBVH.Nodes.IsValidIndex(NodeIndex)) return false;

	const FACFSurfaceRaycastBVHNode& Node = Region.TriangleBVH.Nodes[NodeIndex];

	FBox NewAABBWS(ForceInit);
	if (!BuildAABBFromPiecesWS(SurfaceAsset, Node.AABBPieces, RuntimeCache, NewAABBWS))
	{
		return false;
	}

	RuntimeCache.NodeAABBsWS[RegionIndex][NodeIndex] = NewAABBWS;
	RuntimeCache.NodeAABBVersions[RegionIndex][NodeIndex] = RuntimeCache.PoseVersion;

	OutAABBWS = NewAABBWS;
	return OutAABBWS.IsValid != 0;
}

bool FACFSurfaceRaycastRuntime::BuildAABBFromPiecesWS(const UACFSurfaceAsset* SurfaceAsset, const TArray<FACFSurfaceRaycastAABBPiece>& Pieces, const FACFSurfaceRaycastRuntimeCache& RuntimeCache, FBox& OutAABBWS)
{
	OutAABBWS = FBox(ForceInit);

	const FACFSurfaceRaycastData& RaycastData = SurfaceAsset->RaycastData;

	for (const FACFSurfaceRaycastAABBPiece& Piece : Pieces)
	{
		if (Piece.BoneIndex == INDEX_NONE) continue;
		if (!Piece.RefBoneLocalAABB.IsValid) continue;
		if (!RaycastData.RefPoseBoneCS.IsValidIndex(Piece.BoneIndex)) continue;
		if (!RuntimeCache.RefToLocalMatrices.IsValidIndex(Piece.BoneIndex)) continue;

		const FTransform& RefBoneCS = RaycastData.RefPoseBoneCS[Piece.BoneIndex];
		const FMatrix44f& RefToLocal = RuntimeCache.RefToLocalMatrices[Piece.BoneIndex];

		for (int32 CornerIndex = 0; CornerIndex < 8; ++CornerIndex)
		{
			FVector RefBoneLocalCorner;
			ACFGetBoxCorner(Piece.RefBoneLocalAABB, CornerIndex, RefBoneLocalCorner);

			// Piece skinning
			const FVector RefCS = RefBoneCS.TransformPosition(RefBoneLocalCorner);
			const FVector CurrentCS = FVector(RefToLocal.TransformPosition(FVector3f(RefCS)));
			const FVector CurrentWS = RuntimeCache.ComponentTransform.TransformPosition(CurrentCS);

			OutAABBWS += CurrentWS;
		}
	}

	return OutAABBWS.IsValid != 0;
}

bool FACFSurfaceRaycastRuntime::GetNodeEntryDistance(const UACFSurfaceAsset* SurfaceAsset, int32 RegionIndex, int32 NodeIndex, FACFSurfaceRaycastRuntimeCache& RuntimeCache,
	const FACFSurfaceBVHQuery& Query, float& OutEntryDistance)
{
	OutEntryDistance = TNumericLimits<float>::Max();

	if (!SurfaceAsset) return false;
	if (!SurfaceAsset->RaycastData.Regions.IsValidIndex(RegionIndex)) return false;

	const FACFSurfaceRaycastRegion& Region = SurfaceAsset->RaycastData.Regions[RegionIndex];
	const FACFSurfaceRaycastBVH& BVH = Region.TriangleBVH;
	if (!BVH.Nodes.IsValidIndex(NodeIndex)) return false;

	FBox NodeAABBWS(ForceInit);
	if (!EnsureNodeAABBUpdated(SurfaceAsset, RegionIndex, NodeIndex, RuntimeCache, NodeAABBWS))
	{
		return false;
	}

	if (Query.BoundsExpandRadius > 0.0f) NodeAABBWS = NodeAABBWS.ExpandBy(Query.BoundsExpandRadius);

	return ACFSurfaceMath::SegmentIntersectsAABB(Query.Start, Query.End, NodeAABBWS, OutEntryDistance);
}

bool FACFSurfaceRaycastRuntime::GetCapsuleNodeEntryDistance(
    const UACFSurfaceAsset* SurfaceAsset,
    int32 RegionIndex,
    int32 NodeIndex,
    FACFSurfaceRaycastRuntimeCache& RuntimeCache,
    const FACFSurfaceCapsuleBVHQuery& Query,
    float& OutEntryDistance)
{
    OutEntryDistance = TNumericLimits<float>::Max();

    if (!SurfaceAsset) return false;
    if (!SurfaceAsset->RaycastData.Regions.IsValidIndex(RegionIndex)) return false;

    const FACFSurfaceRaycastRegion& Region = SurfaceAsset->RaycastData.Regions[RegionIndex];
    const FACFSurfaceRaycastBVH& BVH = Region.TriangleBVH;
    if (!BVH.Nodes.IsValidIndex(NodeIndex)) return false;

    FBox NodeAABBWS(ForceInit);
    if (!EnsureNodeAABBUpdated(SurfaceAsset, RegionIndex, NodeIndex, RuntimeCache, NodeAABBWS))
    {
        return false;
    }

    FBox CapsuleSweepAABB(ForceInit);
    CapsuleSweepAABB += Query.StartA;
    CapsuleSweepAABB += Query.StartB;
    CapsuleSweepAABB += Query.EndA;
    CapsuleSweepAABB += Query.EndB;
    CapsuleSweepAABB = CapsuleSweepAABB.ExpandBy(Query.Radius);

    if (!CapsuleSweepAABB.Intersect(NodeAABBWS))
    {
        return false;
    }

    OutEntryDistance = 0.0f;
    return true;
}

void FACFSurfaceRaycastRuntime::RaycastTriangle(const UACFSurfaceAsset* SurfaceAsset, int32 TriangleIndex, FACFSurfaceRaycastRuntimeCache& RuntimeCache,
	const FACFSurfaceQuery& Query, float& InOutBestDistance, FACFSurfaceHitResult& InOutBestHit)
{
	if (!SurfaceAsset) return;
	if (!SurfaceAsset->Triangles.IsValidIndex(TriangleIndex)) return;

	const FACFTriangle& Triangle = SurfaceAsset->Triangles[TriangleIndex];
	if (!MatchesQueryFlags(Triangle, Query.RequiredQueryFlags)) return;

	const int32 I0 = Triangle.VertexIndices[0];
	const int32 I1 = Triangle.VertexIndices[1];
	const int32 I2 = Triangle.VertexIndices[2];

	if (!SurfaceAsset->Vertices.IsValidIndex(I0) ||
		!SurfaceAsset->Vertices.IsValidIndex(I1) ||
		!SurfaceAsset->Vertices.IsValidIndex(I2))
	{
		return;
	}

	FVector P0, P1, P2;
	FVector N0, N1, N2;

	if (!SkinVertexWS(SurfaceAsset, I0, RuntimeCache, P0, N0) ||
		!SkinVertexWS(SurfaceAsset, I1, RuntimeCache, P1, N1) ||
		!SkinVertexWS(SurfaceAsset, I2, RuntimeCache, P2, N2))
	{
		return;
	}

	FVector IntersectPoint;
	FVector IntersectNormal;
	if (!FMath::SegmentTriangleIntersection(Query.Start, Query.End, P0, P1, P2, IntersectPoint, IntersectNormal))
	{
		return;
	}
	
	if (Query.bCullBackfaces)
	{
		const FVector FaceNormal = FVector::CrossProduct(P2 - P0, P1 - P0).GetSafeNormal();
		if (FaceNormal.IsNearlyZero()) return;
		if (FVector::DotProduct(Query.End - Query.Start, FaceNormal) >= 0.0f) return;
	}

	const float HitDistance = FVector::Distance(Query.Start, IntersectPoint);
	if (HitDistance >= InOutBestDistance)
	{
		return;
	}

	const FVector Bary = FMath::ComputeBaryCentric2D(IntersectPoint, P0, P1, P2);
	const FVector BaryNormal = (Bary.X * N0 + Bary.Y * N1 + Bary.Z * N2).GetSafeNormal();

	InOutBestDistance = HitDistance;

	InOutBestHit.Position = IntersectPoint;
	InOutBestHit.Normal = BaryNormal;
	InOutBestHit.Distance = HitDistance;
	InOutBestHit.TriangleIndex = TriangleIndex;
	InOutBestHit.Barycentric = Bary;
}

void FACFSurfaceRaycastRuntime::SweepSphereTriangle(const UACFSurfaceAsset* SurfaceAsset, int32 TriangleIndex, FACFSurfaceRaycastRuntimeCache& RuntimeCache, const FACFSurfaceSphereSweepQuery& Query, float& InOutBestDistance, FACFSurfaceHitResult& InOutBestHit)
{
	if (!SurfaceAsset || !SurfaceAsset->Triangles.IsValidIndex(TriangleIndex)) return;

	const FACFTriangle& Triangle = SurfaceAsset->Triangles[TriangleIndex];
	if (!MatchesQueryFlags(Triangle, Query.RequiredQueryFlags)) return;

	const int32 I0 = Triangle.VertexIndices[0];
	const int32 I1 = Triangle.VertexIndices[1];
	const int32 I2 = Triangle.VertexIndices[2];

	FVector P0, P1, P2;
	FVector N0, N1, N2;

	if (!SkinVertexWS(SurfaceAsset, I0, RuntimeCache, P0, N0) ||
		!SkinVertexWS(SurfaceAsset, I1, RuntimeCache, P1, N1) ||
		!SkinVertexWS(SurfaceAsset, I2, RuntimeCache, P2, N2))
	{
		return;
	}

	const FVector Segment = Query.End - Query.Start;
	const float SegmentLength = Segment.Size();
	if (SegmentLength <= KINDA_SMALL_NUMBER) return;
	
	float HitT = 0.0f;
	FVector SphereCenter;
	FVector ImpactPoint;
	if (!ACFSurfaceMath::SweepSphereTriangleTOI(Query.Start, Query.End, Query.Radius, P0, P1, P2, HitT, SphereCenter, ImpactPoint)) return;
	
	if (Query.bCullBackfaces)
	{
		const FVector FaceNormal = FVector::CrossProduct(P2 - P0, P1 - P0).GetSafeNormal();
		if (FaceNormal.IsNearlyZero()) return;

		if (Query.Radius <= KINDA_SMALL_NUMBER)
		{
			if (FVector::DotProduct(Segment, FaceNormal) >= 0.0f) return;
		}
		else
		{
			const FVector ContactOffset = SphereCenter - ImpactPoint;
			if (FVector::DotProduct(ContactOffset, FaceNormal) <= KINDA_SMALL_NUMBER) return;
		}
	}

	const float HitDistance = HitT * SegmentLength;
	if (HitDistance >= InOutBestDistance) return;

	const FVector Bary = FMath::ComputeBaryCentric2D(ImpactPoint, P0, P1, P2);
	const FVector BaryNormal = (Bary.X * N0 + Bary.Y * N1 + Bary.Z * N2).GetSafeNormal();

	InOutBestDistance = HitDistance;

	InOutBestHit.Position = ImpactPoint;
	InOutBestHit.Normal = BaryNormal;
	InOutBestHit.Distance = HitDistance;
	InOutBestHit.TriangleIndex = TriangleIndex;
	InOutBestHit.Barycentric = Bary;
}

void FACFSurfaceRaycastRuntime::SweepSphereTriangleAll(
	const UACFSurfaceAsset* SurfaceAsset,
	int32 TriangleIndex,
	FACFSurfaceRaycastRuntimeCache& RuntimeCache,
	const FACFSurfaceSphereSweepQuery& Query,
	TArray<FACFSurfaceHitResult>& OutHits)
{
	if (!SurfaceAsset || !SurfaceAsset->Triangles.IsValidIndex(TriangleIndex)) return;

	const FACFTriangle& Triangle = SurfaceAsset->Triangles[TriangleIndex];
	if (!MatchesQueryFlags(Triangle, Query.RequiredQueryFlags)) return;
	const int32 I0 = Triangle.VertexIndices[0];
	const int32 I1 = Triangle.VertexIndices[1];
	const int32 I2 = Triangle.VertexIndices[2];

	FVector P0, P1, P2;
	FVector N0, N1, N2;
	if (!SkinVertexWS(SurfaceAsset, I0, RuntimeCache, P0, N0) ||
		!SkinVertexWS(SurfaceAsset, I1, RuntimeCache, P1, N1) ||
		!SkinVertexWS(SurfaceAsset, I2, RuntimeCache, P2, N2)) return;

	const FVector Segment = Query.End - Query.Start;
	const float SegmentLength = Segment.Size();
	if (SegmentLength <= KINDA_SMALL_NUMBER) return;
	
	float HitT = 0.0f;
	FVector SphereCenter;
	FVector ImpactPoint;
	if (!ACFSurfaceMath::SweepSphereTriangleTOI(Query.Start, Query.End, Query.Radius, P0, P1, P2, HitT, SphereCenter, ImpactPoint)) return;
	
	if (Query.bCullBackfaces)
	{
		const FVector FaceNormal = FVector::CrossProduct(P2 - P0, P1 - P0).GetSafeNormal();
		if (FaceNormal.IsNearlyZero()) return;

		if (Query.Radius <= KINDA_SMALL_NUMBER)
		{
			if (FVector::DotProduct(Segment, FaceNormal) >= 0.0f) return;
		}
		else
		{
			const FVector ContactOffset = SphereCenter - ImpactPoint;
			if (FVector::DotProduct(ContactOffset, FaceNormal) <= KINDA_SMALL_NUMBER) return;
		}
	}

	const FVector Bary = FMath::ComputeBaryCentric2D(ImpactPoint, P0, P1, P2);
	FACFSurfaceHitResult& Hit = OutHits.AddDefaulted_GetRef();
	Hit.Position = ImpactPoint;
	Hit.Normal = (Bary.X * N0 + Bary.Y * N1 + Bary.Z * N2).GetSafeNormal();
	Hit.Distance = HitT * SegmentLength;
	Hit.TriangleIndex = TriangleIndex;
	Hit.Barycentric = Bary;
}

void FACFSurfaceRaycastRuntime::SweepCapsuleTriangleAll(
    const UACFSurfaceAsset* SurfaceAsset,
    int32 TriangleIndex,
    FACFSurfaceRaycastRuntimeCache& RuntimeCache,
    const FACFSurfaceCapsuleSweepQuery& Query,
    TArray<FACFSurfaceCapsuleHitResult>& OutHits)
{
    if (!SurfaceAsset || !SurfaceAsset->Triangles.IsValidIndex(TriangleIndex)) return;

    const FACFTriangle& Triangle = SurfaceAsset->Triangles[TriangleIndex];
    if (!MatchesQueryFlags(Triangle, Query.RequiredQueryFlags)) return;
    const int32 I0 = Triangle.VertexIndices[0];
    const int32 I1 = Triangle.VertexIndices[1];
    const int32 I2 = Triangle.VertexIndices[2];

    FVector P0, P1, P2;
    FVector N0, N1, N2;
    if (!SkinVertexWS(SurfaceAsset, I0, RuntimeCache, P0, N0) ||
        !SkinVertexWS(SurfaceAsset, I1, RuntimeCache, P1, N1) ||
        !SkinVertexWS(SurfaceAsset, I2, RuntimeCache, P2, N2))
    {
        return;
    }

    FVector ClosestOnCapsule;
    FVector ClosestOnTriangle;
    float CapsuleT = 0.0f;
    const float DistSq = ACFSurfaceMath::ClosestSegmentTriangle(
        Query.EndB,
        Query.EndA,
        P0,
        P1,
        P2,
        ClosestOnCapsule,
        ClosestOnTriangle,
        CapsuleT);
	
	if (Query.bCullBackfaces)
	{
		const FVector FaceNormal = FVector::CrossProduct(P2 - P0, P1 - P0).GetSafeNormal();
		if (FaceNormal.IsNearlyZero()) return;

		const FVector ContactOffset = ClosestOnCapsule - ClosestOnTriangle;
		if (FVector::DotProduct(ContactOffset, FaceNormal) <= KINDA_SMALL_NUMBER) return;
	}

    const float RadiusSq = FMath::Square(Query.Radius);
    if (DistSq > RadiusSq) return;

    const float Dist = FMath::Sqrt(FMath::Max(DistSq, 0.0f));
    const float PenetrationDepth = Query.Radius - Dist;
    const FVector Bary = FMath::ComputeBaryCentric2D(ClosestOnTriangle, P0, P1, P2);
    const FVector SurfaceNormal = (Bary.X * N0 + Bary.Y * N1 + Bary.Z * N2).GetSafeNormal();
    const FVector PushNormal = Dist > KINDA_SMALL_NUMBER
        ? (ClosestOnCapsule - ClosestOnTriangle) / Dist
        : SurfaceNormal;

    FACFSurfaceCapsuleHitResult& Hit = OutHits.AddDefaulted_GetRef();
    Hit.SurfaceHit.Position = ClosestOnTriangle;
    Hit.SurfaceHit.Normal = SurfaceNormal;
    Hit.SurfaceHit.Distance = Dist;
    Hit.SurfaceHit.TriangleIndex = TriangleIndex;
    Hit.SurfaceHit.Barycentric = Bary;
    Hit.CapsulePoint = ClosestOnCapsule;
    Hit.PushNormal = PushNormal;
    Hit.PenetrationDepth = PenetrationDepth;
}

void FACFSurfaceRaycastRuntime::SweepCapsuleTriangle(
    const UACFSurfaceAsset* SurfaceAsset,
    int32 TriangleIndex,
    FACFSurfaceRaycastRuntimeCache& RuntimeCache,
    const FACFSurfaceCapsuleSweepQuery& Query,
    float& InOutBestPenetration,
    FACFSurfaceCapsuleHitResult& InOutBestHit)
{
    if (!SurfaceAsset) return;
    if (!SurfaceAsset->Triangles.IsValidIndex(TriangleIndex)) return;

    const FACFTriangle& Triangle = SurfaceAsset->Triangles[TriangleIndex];
    if (!MatchesQueryFlags(Triangle, Query.RequiredQueryFlags)) return;

    const int32 I0 = Triangle.VertexIndices[0];
    const int32 I1 = Triangle.VertexIndices[1];
    const int32 I2 = Triangle.VertexIndices[2];

    FVector P0, P1, P2;
    FVector N0, N1, N2;

    if (!SkinVertexWS(SurfaceAsset, I0, RuntimeCache, P0, N0) ||
        !SkinVertexWS(SurfaceAsset, I1, RuntimeCache, P1, N1) ||
        !SkinVertexWS(SurfaceAsset, I2, RuntimeCache, P2, N2))
    {
        return;
    }

    FVector ClosestOnCapsule;
    FVector ClosestOnTriangle;
    float CapsuleT = 0.0f;

    const float DistSq = ACFSurfaceMath::ClosestSegmentTriangle(
        Query.EndB,
        Query.EndA,
        P0,
        P1,
        P2,
        ClosestOnCapsule,
        ClosestOnTriangle,
        CapsuleT);
	
	if (Query.bCullBackfaces)
	{
		const FVector FaceNormal = FVector::CrossProduct(P2 - P0, P1 - P0).GetSafeNormal();
		if (FaceNormal.IsNearlyZero()) return;

		const FVector ContactOffset = ClosestOnCapsule - ClosestOnTriangle;
		if (FVector::DotProduct(ContactOffset, FaceNormal) <= KINDA_SMALL_NUMBER) return;
	}

    const float RadiusSq = FMath::Square(Query.Radius);
    if (DistSq > RadiusSq) return;

    const float Dist = FMath::Sqrt(FMath::Max(DistSq, 0.0f));
    const float PenetrationDepth = Query.Radius - Dist;

    const FVector Bary = FMath::ComputeBaryCentric2D(ClosestOnTriangle, P0, P1, P2);

    const FVector SurfaceNormal = (Bary.X * N0 + Bary.Y * N1 + Bary.Z * N2).GetSafeNormal();

    FVector PushNormal;
    if (Dist > KINDA_SMALL_NUMBER)
    {
        PushNormal = (ClosestOnCapsule - ClosestOnTriangle) / Dist;
    }
    else
    {
        PushNormal = SurfaceNormal;
    }

    InOutBestHit.BlendedPushDir += PushNormal * PenetrationDepth;
    InOutBestHit.MaxPenetrationDepth = FMath::Max(InOutBestHit.MaxPenetrationDepth, PenetrationDepth);

    if (PenetrationDepth <= InOutBestPenetration) return;

    InOutBestPenetration = PenetrationDepth;

    InOutBestHit.SurfaceHit.Position = ClosestOnTriangle;
    InOutBestHit.SurfaceHit.Normal = SurfaceNormal;
    InOutBestHit.SurfaceHit.Distance = Dist;
    InOutBestHit.SurfaceHit.TriangleIndex = TriangleIndex;
    InOutBestHit.SurfaceHit.Barycentric = Bary;

    InOutBestHit.CapsulePoint = ClosestOnCapsule;
    InOutBestHit.PushNormal = PushNormal;
    InOutBestHit.PenetrationDepth = PenetrationDepth;
}
