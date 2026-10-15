// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "ACFSurfaceBVHBuilder.h"

#include "ReferenceSkeleton.h"
#include "Engine/SkeletalMesh.h"

DEFINE_LOG_CATEGORY_STATIC(LogACFSurfaceBVHBuilder, Log, All);

namespace 
{
	constexpr int32 ACFSurfaceBVHLeafTriangleCount = 8;
	constexpr double ACFSurfaceBVHMinSplitExtent = 0.01;
	
	double ACFSurfaceGetAxisValue(const FVector& Value, int32 Axis)
	{
		if (Axis == 0) return Value.X;
		if (Axis == 1) return Value.Y;
		return Value.Z;
	}
	
	int32 ACFSurfaceGetLongestAxis(const FVector& Extent)
	{
		if (Extent.Y >= Extent.X && Extent.Y >= Extent.Z) return 1;
		if (Extent.Z >= Extent.X && Extent.Z >= Extent.Y) return 2;
		return 0;
	}
	
	bool HasAnyBoneInfluence(const FACFTriangleVertex& Vertex)
	{
		const int32 NumInfluences = FMath::Min(Vertex.BoneIndices.Num(), Vertex.BoneWeights.Num());
		
		for (int32 InfluenceIndex = 0; InfluenceIndex < NumInfluences; ++InfluenceIndex)
		{
			if (Vertex.BoneIndices[InfluenceIndex] != INDEX_NONE && Vertex.BoneWeights[InfluenceIndex] > 0.0f)
			{
				return true;
			}
		}
		return false;
	}
}

bool FACFSurfaceBVHBuilder::Build(UACFSurfaceAsset* InAsset)
{
	FString Error;

	if (!InAsset)
	{
		UE_LOG(LogACFSurfaceBVHBuilder, Warning, TEXT("[ACFSurfaceBVHBuilder] Build failed. Asset is null."));
		return false;
	}

	if (!InAsset->SourceMesh)
	{
		UE_LOG(LogACFSurfaceBVHBuilder, Warning, TEXT("[ACFSurfaceBVHBuilder] Build failed. SourceMesh is null."));
		return false;
	}

	if (InAsset->Vertices.Num() <= 0)
	{
		UE_LOG(LogACFSurfaceBVHBuilder, Warning, TEXT("[ACFSurfaceBVHBuilder] Build failed. No surface vertices."));
		return false;
	}

	if (InAsset->Triangles.Num() <= 0)
	{
		UE_LOG(LogACFSurfaceBVHBuilder, Warning, TEXT("[ACFSurfaceBVHBuilder] Build failed. No surface triangles."));
		return false;
	}

	InAsset->RaycastData.Reset();

	if (!BuildRefPoseBoneCS(InAsset, Error) ||
		!BuildRegions(InAsset, Error) ||
		!BuildRegionGraph(InAsset, Error) ||
		!BuildRegionAABBPieces(InAsset, Error) ||
		!BuildRegionTriangleBVHs(InAsset, Error))
	{
		InAsset->RaycastData.Reset();

		UE_LOG(LogACFSurfaceBVHBuilder, Warning,
			TEXT("[ACFSurfaceBVHBuilder] Build failed. Error=%s"),
			*Error);

		return false;
	}

	if (!InAsset->RaycastData.HasValidData(InAsset->Triangles.Num()))
	{
		InAsset->RaycastData.Reset();

		UE_LOG(LogACFSurfaceBVHBuilder, Warning,
			TEXT("[ACFSurfaceBVHBuilder] Build failed. Final RaycastData is invalid."));

		return false;
	}

	int32 RegionPieceCount = 0;
	int32 NodeCount = 0;
	int32 LeafCount = 0;
	int32 NodePieceCount = 0;
	int32 MaxLeafTriangleCount = 0;

	for (const FACFSurfaceRaycastRegion& Region : InAsset->RaycastData.Regions)
	{
		RegionPieceCount += Region.AABBPieces.Num();

		for (const FACFSurfaceRaycastBVHNode& Node : Region.TriangleBVH.Nodes)
		{
			++NodeCount;
			NodePieceCount += Node.AABBPieces.Num();

			if (Node.IsLeaf())
			{
				++LeafCount;
				MaxLeafTriangleCount = FMath::Max(MaxLeafTriangleCount, Node.TriangleCount);
			}
		}
	}

	UE_LOG(LogACFSurfaceBVHBuilder, Log, TEXT("[ACFSurfaceBVHBuilder] Built. Vertices=%d, Triangles=%d, Bones=%d, Regions=%d, RegionPieces=%d, BVHNodes=%d, BVHLeaves=%d, MaxLeafTris=%d, NodePieces=%d"),
		InAsset->Vertices.Num(), InAsset->Triangles.Num(), InAsset->RaycastData.RefPoseBoneCS.Num(), InAsset->RaycastData.Regions.Num(), RegionPieceCount,
		NodeCount, LeafCount, MaxLeafTriangleCount, NodePieceCount);

	return true;
}

// Returns the bone transform in component space.
bool FACFSurfaceBVHBuilder::BuildRefPoseBoneCS(UACFSurfaceAsset* InAsset, FString& OutError)
{
	const FReferenceSkeleton& RefSkeleton = InAsset->SourceMesh->GetRefSkeleton();
	const TArray<FTransform>& LocalRefPose = RefSkeleton.GetRefBonePose();

	// Surface vertices can only reference raw, skinnable bones. GetNum() also
	// includes virtual bones, which do not have entries in RefBasesInvMatrix.
	const int32 BoneCount = RefSkeleton.GetRawBoneNum();
	const int32 SkinningMatrixCount = InAsset->SourceMesh->GetRefBasesInvMatrix().Num();
	if (BoneCount <= 0 || LocalRefPose.Num() < BoneCount || SkinningMatrixCount != BoneCount)
	{
		OutError = FString::Printf(
			TEXT("BuildRefPoseBoneCS failed: invalid skinning skeleton. RawBones=%d, FinalRefPose=%d, RefBasesInvMatrices=%d"),
			BoneCount,
			LocalRefPose.Num(),
			SkinningMatrixCount);
		return false;
	}
	
	FACFSurfaceRaycastData& RaycastData = InAsset->RaycastData;
	
	RaycastData.RefPoseBoneCS.SetNum(BoneCount);
	RaycastData.BoneNames.SetNum(BoneCount);
	RaycastData.BoneToRegionIndex.Init(INDEX_NONE, BoneCount);
	
	for (int32 BoneIndex = 0; BoneIndex < BoneCount; ++BoneIndex)
	{
		RaycastData.BoneNames[BoneIndex] = RefSkeleton.GetBoneName(BoneIndex);
		
		const int32 ParentIndex = RefSkeleton.GetParentIndex(BoneIndex);
		if (ParentIndex == INDEX_NONE)
		{
			RaycastData.RefPoseBoneCS[BoneIndex] = LocalRefPose[BoneIndex];
			continue;
		}
		
		if (!RaycastData.RefPoseBoneCS.IsValidIndex(ParentIndex))
		{
			OutError = FString::Printf(TEXT("BuildRefPoseBoneCS failed: invalid parent. Bone=%d, Parent=%d"),
				BoneIndex, ParentIndex);
			return false;
		}
		
		RaycastData.RefPoseBoneCS[BoneIndex] = LocalRefPose[BoneIndex] * RaycastData.RefPoseBoneCS[ParentIndex];
	}
	
	return true;
}

bool FACFSurfaceBVHBuilder::BuildRegions(UACFSurfaceAsset* InAsset, FString& OutError)
{
	FACFSurfaceRaycastData& RaycastData = InAsset->RaycastData;
	
	if (RaycastData.RefPoseBoneCS.Num() <= 0)
	{
		OutError = TEXT("BuildRegions failed: no ref pose bones.");
		return false;
	}
	
	RaycastData.TriangleMetas.SetNum(InAsset->Triangles.Num());
	
	TMap<int32, TArray<int32>> BoneToTriangleIndices;
	
	for (int32 TriangleIndex = 0; TriangleIndex < InAsset->Triangles.Num(); ++TriangleIndex)
	{
		const FACFTriangle& Triangle = InAsset->Triangles[TriangleIndex];
		
		const int32 DominantBoneIndex = ComputeDominantBoneForTriangle(InAsset, Triangle);
		if (!RaycastData.RefPoseBoneCS.IsValidIndex(DominantBoneIndex)) continue;
		
		RaycastData.TriangleMetas[TriangleIndex].DominantBoneIndex = DominantBoneIndex;
		BoneToTriangleIndices.FindOrAdd(DominantBoneIndex).Add(TriangleIndex);
	}
	
	if (BoneToTriangleIndices.Num() <= 0)
	{
		OutError = TEXT("BuildRegions failed: no bone received triangles.");
		return false;
	}
	
	TArray<int32> RegionBoneIndices;
	BoneToTriangleIndices.GetKeys(RegionBoneIndices);
	RegionBoneIndices.Sort();
	
	const FReferenceSkeleton& RefSkeleton = InAsset->SourceMesh->GetRefSkeleton();
	
	RaycastData.Regions.Reset();
	RaycastData.Regions.Reserve(RegionBoneIndices.Num());
	
	for (const int32 BoneIndex : RegionBoneIndices)
	{
		const TArray<int32>* TriangleIndices = BoneToTriangleIndices.Find(BoneIndex);
		if (!TriangleIndices || TriangleIndices->Num() <= 0) continue;
		
		const int32 RegionIndex = RaycastData.Regions.Num();
		
		FACFSurfaceRaycastRegion Region;
		Region.RegionIndex = RegionIndex;
		Region.RepresentativeBoneIndex = BoneIndex;
		Region.RepresentativeBoneName = RefSkeleton.GetBoneName(BoneIndex);
		Region.TriangleIndices = *TriangleIndices;

		RaycastData.Regions.Add(MoveTemp(Region));
		RaycastData.BoneToRegionIndex[BoneIndex] = RegionIndex;
	}
	
	for (int32 TriangleIndex = 0; TriangleIndex < RaycastData.TriangleMetas.Num(); ++TriangleIndex)
	{
		FACFSurfaceRaycastTriangleMeta& TriangleMeta = RaycastData.TriangleMetas[TriangleIndex];
		
		if (!RaycastData.BoneToRegionIndex.IsValidIndex(TriangleMeta.DominantBoneIndex)) continue;
		
		TriangleMeta.RegionIndex = RaycastData.BoneToRegionIndex[TriangleMeta.DominantBoneIndex];
	}
	
	if (RaycastData.Regions.Num() <= 0)
	{
		OutError = TEXT("BuildRegions failed: no regions built.");
		return false;
	}
	
	return true;
}

int32 FACFSurfaceBVHBuilder::ComputeDominantBoneForTriangle(const UACFSurfaceAsset* InAsset, const FACFTriangle& Triangle)
{
	if (!InAsset) return INDEX_NONE;
	
	TMap<int32, float> BoneWeightSums;
	
	for (int32 Corner = 0; Corner < 3; ++Corner)
	{
		const int32 VertexIndex = Triangle.VertexIndices[Corner];
		if (!InAsset->Vertices.IsValidIndex(VertexIndex)) return INDEX_NONE;
		
		const FACFTriangleVertex& Vertex = InAsset->Vertices[VertexIndex];
		const int32 NumInfluences = FMath::Min(Vertex.BoneIndices.Num(), Vertex.BoneWeights.Num());
		
		for (int32 InfluenceIndex = 0; InfluenceIndex < NumInfluences; ++InfluenceIndex)
		{
			const int32 BoneIndex = Vertex.BoneIndices[InfluenceIndex];
			const float Weight = Vertex.BoneWeights[InfluenceIndex];
			
			if (BoneIndex == INDEX_NONE || Weight <= 0.0f) continue;
			
			BoneWeightSums.FindOrAdd(BoneIndex) += Weight;
		}
	}
	
	int32 BestBoneIndex = INDEX_NONE;
	float BestWeightSum = -1.0f;
	
	for (const TPair<int32, float>& Pair : BoneWeightSums)
	{
		if (Pair.Value > BestWeightSum)
		{
			BestBoneIndex = Pair.Key;
			BestWeightSum = Pair.Value;
			continue;
		}
		
		if (FMath::IsNearlyEqual(Pair.Value, BestWeightSum) && (BestBoneIndex == INDEX_NONE || Pair.Key < BestBoneIndex))
		{
			BestBoneIndex = Pair.Key;
			BestWeightSum = Pair.Value;
		}
	}
	
	return BestBoneIndex;
}

bool FACFSurfaceBVHBuilder::BuildRegionGraph(UACFSurfaceAsset* InAsset, FString& OutError)
{
	FACFSurfaceRaycastData& RaycastData = InAsset->RaycastData;
	const FReferenceSkeleton& RefSkeleton = InAsset->SourceMesh->GetRefSkeleton();
	
	for (FACFSurfaceRaycastRegion& Region : RaycastData.Regions)
	{
		Region.AdjacentRegionIndices.Reset();
	}
	
	for (FACFSurfaceRaycastRegion& Region : RaycastData.Regions)
	{
		const int32 CurrentRegionIndex = Region.RegionIndex;
		const int32 CurrentBoneIndex = Region.RepresentativeBoneIndex;
		
		const int32 AncestorRegionIndex = FindNearestAncestorRegion(RefSkeleton, InAsset, CurrentBoneIndex);
		if (AncestorRegionIndex == INDEX_NONE || AncestorRegionIndex == CurrentRegionIndex) continue;
		
		Region.AdjacentRegionIndices.AddUnique(AncestorRegionIndex);
		RaycastData.Regions[AncestorRegionIndex].AdjacentRegionIndices.AddUnique(CurrentRegionIndex);
	}
	
	return true;
}

int32 FACFSurfaceBVHBuilder::FindNearestAncestorRegion(const FReferenceSkeleton& RefSkeleton, const UACFSurfaceAsset* InAsset, int32 BoneIndex)
{
	const FACFSurfaceRaycastData& RaycastData = InAsset->RaycastData;
	
	int32 ParentBoneIndex = RefSkeleton.GetParentIndex(BoneIndex);
	while (ParentBoneIndex != INDEX_NONE)
	{
		if (RaycastData.BoneToRegionIndex.IsValidIndex(ParentBoneIndex))
		{
			const int32 RegionIndex = RaycastData.BoneToRegionIndex[ParentBoneIndex];
			if (RegionIndex != INDEX_NONE) return RegionIndex;
		}
		
		ParentBoneIndex = RefSkeleton.GetParentIndex(ParentBoneIndex);
	}
	
	return INDEX_NONE;
}

bool FACFSurfaceBVHBuilder::BuildRegionAABBPieces(UACFSurfaceAsset* InAsset, FString& OutError)
{
	for (FACFSurfaceRaycastRegion& Region : InAsset->RaycastData.Regions)
	{
		if (!BuildRegionAABBPiecesForRegion(InAsset, Region, OutError)) return false;
	}
	
	return true;
}


bool FACFSurfaceBVHBuilder::BuildRegionAABBPiecesForRegion(UACFSurfaceAsset* InAsset, FACFSurfaceRaycastRegion& Region, FString& OutError)
{
	Region.AABBPieces.Reset();
	
	if (Region.TriangleIndices.Num() <= 0)
	{
		OutError = FString::Printf(TEXT("BuildRegionAABBPiecesForRegion failed: invalid region. Region=%d"), Region.RegionIndex);
		return false;
	}
	
	TMap<int32, FBox> BoneToLocalBoxes;
	
	for (const int32 TriangleIndex : Region.TriangleIndices)
	{
		const FACFTriangle& Triangle = InAsset->Triangles[TriangleIndex];
		
		for (int32 Corner = 0; Corner < 3; ++Corner)
		{
			if (!AddVertexToBoneLocalBoxes(InAsset, Triangle.VertexIndices[Corner], BoneToLocalBoxes, OutError))
			{
				return false;
			}
		}
	}
	
	if (BoneToLocalBoxes.Num() <= 0)
	{
		OutError = FString::Printf(TEXT("BuildRegionAABBPiecesForRegion failed: no AABB pieces. Region=%d"), Region.RegionIndex);
		return false;
	}
	
	TArray<int32> BoneIndices;
	BoneToLocalBoxes.GetKeys(BoneIndices);
	BoneIndices.Sort();
	
	Region.AABBPieces.Reserve(BoneIndices.Num());
	
	for (const int32 BoneIndex : BoneIndices)
	{
		FACFSurfaceRaycastAABBPiece Piece;
		Piece.BoneIndex = BoneIndex;
		Piece.RefBoneLocalAABB = BoneToLocalBoxes[BoneIndex];

		Region.AABBPieces.Add(Piece);
	}
	
	return true;
}

// Expands each affected bone's local box by the vertex.
bool FACFSurfaceBVHBuilder::AddVertexToBoneLocalBoxes(const UACFSurfaceAsset* InAsset, int32 VertexIndex, TMap<int32, FBox>& BoneToLocalBoxes, FString& OutError)
{
	if (!InAsset->Vertices.IsValidIndex(VertexIndex))
	{
		OutError = FString::Printf(TEXT("AddVertexToBoneLocalBoxes failed: invalid vertex. Vertex=%d"), VertexIndex);
		return false;
	}
	
	const FACFTriangleVertex& Vertex = InAsset->Vertices[VertexIndex];
	if (!HasAnyBoneInfluence(Vertex))
	{
		OutError = FString::Printf(TEXT("AddVertexToBoneLocalBoxes failed: vertex has no valid influence. Vertex=%d"), VertexIndex);
		return false;
	}
	
	const FACFSurfaceRaycastData& RaycastData = InAsset->RaycastData;
	const int32 NumInfluences = FMath::Min(Vertex.BoneIndices.Num(), Vertex.BoneWeights.Num());
	
	for (int32 InfluenceIndex = 0; InfluenceIndex < NumInfluences; ++InfluenceIndex)
	{
		const int32 BoneIndex = Vertex.BoneIndices[InfluenceIndex];
		const float Weight = Vertex.BoneWeights[InfluenceIndex];
		
		if (BoneIndex == INDEX_NONE || Weight <= 0.0f) continue;
		
		const FTransform& RefBoneCS = RaycastData.RefPoseBoneCS[BoneIndex];
		const FVector RefBoneLocalPosition = RefBoneCS.InverseTransformPosition(Vertex.Position);
		
		FBox* LocalBox = BoneToLocalBoxes.Find(BoneIndex);
		if (!LocalBox) LocalBox = &BoneToLocalBoxes.Add(BoneIndex, FBox(ForceInit));
		
		*LocalBox += RefBoneLocalPosition;
	}
	
	return true;
}

bool FACFSurfaceBVHBuilder::BuildRegionTriangleBVHs(UACFSurfaceAsset* InAsset, FString& OutError)
{
	if (InAsset->RaycastData.Regions.Num() <= 0)
	{
		OutError = TEXT("BuildRegionTriangleBVHs failed: no regions.");
		return false;
	}
	
	for (FACFSurfaceRaycastRegion& Region : InAsset->RaycastData.Regions)
	{
		int32 RegionMaxDepth = 0;
		if (!BuildRegionTriangleBVH(InAsset, Region, ACFSurfaceBVHLeafTriangleCount, RegionMaxDepth, OutError))
		{
			return false;
		}

		if (!Region.TriangleBVH.IsValid())
		{
			OutError = FString::Printf(TEXT("BuildRegionTriangleBVHs failed: invalid BVH. Region=%d"), Region.RegionIndex);
			return false;
		}
	}

	return true;
}

bool FACFSurfaceBVHBuilder::BuildRegionTriangleBVH(UACFSurfaceAsset* InAsset, FACFSurfaceRaycastRegion& Region, int32 LeafTriangleCount, int32& OutMaxDepth, FString& OutError)
{
	OutMaxDepth = 0;
	Region.TriangleBVH.Reset();

	if (Region.TriangleIndices.Num() <= 0)
	{
		OutError = FString::Printf(TEXT("BuildRegionTriangleBVH failed: invalid region. Region=%d"), Region.RegionIndex);
		return false;
	}
	
	const int32 SafeLeafTriangleCount = FMath::Max(1, LeafTriangleCount);
	
	Region.TriangleBVH.OrderedTriangleIndices = Region.TriangleIndices;
	Region.TriangleBVH.Nodes.Reserve(Region.TriangleIndices.Num() * 2);
	
	int32 MaxDepth = 0;
	const int32 RootNodeIndex = BuildRegionTriangleBVHNode(InAsset, Region.TriangleBVH, 0, Region.TriangleBVH.OrderedTriangleIndices.Num(),
		SafeLeafTriangleCount, 0, MaxDepth, OutError);
	
	if (RootNodeIndex == INDEX_NONE)
	{
		if (OutError.IsEmpty())
		{
			OutError = FString::Printf(TEXT("BuildRegionTriangleBVH failed: root build failed. Region=%d"), Region.RegionIndex);
		}

		return false;
	}
	
	Region.TriangleBVH.RootNodeIndex = RootNodeIndex;
	OutMaxDepth = MaxDepth;

	for (int32 NodeIndex = 0; NodeIndex < Region.TriangleBVH.Nodes.Num(); ++NodeIndex)
	{
		FACFSurfaceRaycastBVHNode& Node = Region.TriangleBVH.Nodes[NodeIndex];

		if (!BuildNodeAABBPieces(InAsset, Region.TriangleBVH, Node, OutError))
		{
			OutError = FString::Printf(TEXT("BuildRegionTriangleBVH failed: node AABB piece failed. Region=%d, Node=%d, Error=%s"),
				Region.RegionIndex, NodeIndex, *OutError);
			return false;
		}
	}

	return true;
}

int32 FACFSurfaceBVHBuilder::BuildRegionTriangleBVHNode(UACFSurfaceAsset* InAsset, FACFSurfaceRaycastBVH& BVH, int32 FirstTriangle, int32 TriangleCount, int32 LeafTriangleCount, int32 Depth, int32& InOutMaxDepth, FString& OutError)
{
	if (FirstTriangle < 0 || TriangleCount <= 0 || FirstTriangle + TriangleCount > BVH.OrderedTriangleIndices.Num())
	{
		OutError = FString::Printf(TEXT("BuildRegionTriangleBVHNode failed: invalid range. First=%d, Count=%d, Ordered=%d"),
			FirstTriangle, TriangleCount, BVH.OrderedTriangleIndices.Num());
		return INDEX_NONE;
	}

	const FBox RangeAABB = ComputeOrderedTriangleRangeRefAABB(InAsset, BVH.OrderedTriangleIndices, FirstTriangle, TriangleCount);
	if (!RangeAABB.IsValid)
	{
		OutError = FString::Printf(TEXT("BuildRegionTriangleBVHNode failed: invalid range AABB. First=%d, Count=%d"),
			FirstTriangle, TriangleCount);
		return INDEX_NONE;
	}

	const int32 NodeIndex = BVH.Nodes.AddDefaulted();
	FACFSurfaceRaycastBVHNode& NewNode = BVH.Nodes[NodeIndex];

	NewNode.FirstTriangle = FirstTriangle;
	NewNode.TriangleCount = TriangleCount;
	NewNode.RefAABB = RangeAABB;
	NewNode.LeftChild = INDEX_NONE;
	NewNode.RightChild = INDEX_NONE;

	InOutMaxDepth = FMath::Max(InOutMaxDepth, Depth);

	const FVector Extent = RangeAABB.GetExtent();
	if (TriangleCount <= LeafTriangleCount || Extent.GetMax() <= ACFSurfaceBVHMinSplitExtent) return NodeIndex;

	const int32 SplitAxis = ACFSurfaceGetLongestAxis(Extent);

	TArray<int32> SortedTriangleIndices;
	SortedTriangleIndices.Reserve(TriangleCount);

	for (int32 LocalIndex = 0; LocalIndex < TriangleCount; ++LocalIndex)
	{
		SortedTriangleIndices.Add(BVH.OrderedTriangleIndices[FirstTriangle + LocalIndex]);
	}
	
	// Splits the AABB along its longest axis.
	SortedTriangleIndices.Sort([InAsset, SplitAxis](const int32 A, const int32 B)
{
	if (A == B) return false;

	const FVector CentroidA = FACFSurfaceBVHBuilder::ComputeTriangleRefCentroid(InAsset, InAsset->Triangles[A]);
	const FVector CentroidB = FACFSurfaceBVHBuilder::ComputeTriangleRefCentroid(InAsset, InAsset->Triangles[B]);

	const double ValueA = ACFSurfaceGetAxisValue(CentroidA, SplitAxis);
	const double ValueB = ACFSurfaceGetAxisValue(CentroidB, SplitAxis);

	if (FMath::IsNearlyEqual(ValueA, ValueB)) return A < B;

	return ValueA < ValueB;
});

	for (int32 LocalIndex = 0; LocalIndex < TriangleCount; ++LocalIndex)
	{
		BVH.OrderedTriangleIndices[FirstTriangle + LocalIndex] = SortedTriangleIndices[LocalIndex];
	}

	const int32 LeftCount = TriangleCount / 2;
	const int32 RightCount = TriangleCount - LeftCount;

	if (LeftCount <= 0 || RightCount <= 0) return NodeIndex;

	// Recursively builds the nodes.
	const int32 LeftChild = BuildRegionTriangleBVHNode(InAsset, BVH, FirstTriangle, LeftCount,
		LeafTriangleCount, Depth + 1, InOutMaxDepth, OutError);

	if (LeftChild == INDEX_NONE) return INDEX_NONE;

	const int32 RightChild = BuildRegionTriangleBVHNode(InAsset, BVH, FirstTriangle + LeftCount, RightCount,
		LeafTriangleCount, Depth + 1, InOutMaxDepth, OutError);

	if (RightChild == INDEX_NONE) return INDEX_NONE;

	BVH.Nodes[NodeIndex].LeftChild = LeftChild;
	BVH.Nodes[NodeIndex].RightChild = RightChild;

	return NodeIndex;
}

bool FACFSurfaceBVHBuilder::BuildNodeAABBPieces(const UACFSurfaceAsset* InAsset, const FACFSurfaceRaycastBVH& BVH, FACFSurfaceRaycastBVHNode& Node, FString& OutError)
{
	Node.AABBPieces.Reset();

	TMap<int32, FBox> BoneToLocalBoxes;

	for (int32 LocalIndex = 0; LocalIndex < Node.TriangleCount; ++LocalIndex)
	{
		const int32 OrderedIndex = Node.FirstTriangle + LocalIndex;
		const int32 TriangleIndex = BVH.OrderedTriangleIndices[OrderedIndex];

		const FACFTriangle& Triangle = InAsset->Triangles[TriangleIndex];

		for (int32 Corner = 0; Corner < 3; ++Corner)
		{
			if (!AddVertexToBoneLocalBoxes(InAsset, Triangle.VertexIndices[Corner], BoneToLocalBoxes, OutError))
			{
				return false;
			}
		}
	}

	if (BoneToLocalBoxes.Num() <= 0)
	{
		OutError = TEXT("BuildNodeAABBPieces failed: no AABB pieces.");
		return false;
	}

	TArray<int32> BoneIndices;
	BoneToLocalBoxes.GetKeys(BoneIndices);
	BoneIndices.Sort();

	Node.AABBPieces.Reserve(BoneIndices.Num());

	for (const int32 BoneIndex : BoneIndices)
	{
		FACFSurfaceRaycastAABBPiece Piece;
		Piece.BoneIndex = BoneIndex;
		Piece.RefBoneLocalAABB = BoneToLocalBoxes[BoneIndex];

		Node.AABBPieces.Add(Piece);
	}
	
	return true;
}

FVector FACFSurfaceBVHBuilder::ComputeTriangleRefCentroid(const UACFSurfaceAsset* InAsset, const FACFTriangle& Triangle)
{
	return (InAsset->Vertices[Triangle.VertexIndices[0]].Position +
		InAsset->Vertices[Triangle.VertexIndices[1]].Position +
		InAsset->Vertices[Triangle.VertexIndices[2]].Position) / 3.0;
}

FBox FACFSurfaceBVHBuilder::ComputeTriangleRefAABB(const UACFSurfaceAsset* InAsset, const FACFTriangle& Triangle)
{
	FBox Box(ForceInit);

	Box += InAsset->Vertices[Triangle.VertexIndices[0]].Position;
	Box += InAsset->Vertices[Triangle.VertexIndices[1]].Position;
	Box += InAsset->Vertices[Triangle.VertexIndices[2]].Position;

	return Box;
}

FBox FACFSurfaceBVHBuilder::ComputeOrderedTriangleRangeRefAABB(const UACFSurfaceAsset* InAsset, const TArray<int32>& OrderedTriangleIndices, int32 FirstTriangle, int32 TriangleCount)
{
	FBox Box(ForceInit);

	for (int32 LocalIndex = 0; LocalIndex < TriangleCount; ++LocalIndex)
	{
		const int32 TriangleIndex = OrderedTriangleIndices[FirstTriangle + LocalIndex];
		Box += ComputeTriangleRefAABB(InAsset, InAsset->Triangles[TriangleIndex]);
	}

	return Box;
}
