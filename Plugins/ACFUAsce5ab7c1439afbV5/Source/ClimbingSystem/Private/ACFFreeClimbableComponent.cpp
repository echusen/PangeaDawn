// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "ACFFreeClimbableComponent.h"

#include "ACFFreeClimbableWorldSubsystem.h"
#include "ACFSurfaceAsset.h"
#include "ACFSurfaceMath.h"
#include "ACFSurfaceRaycastRuntime.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Rendering/SkeletalMeshRenderData.h"

namespace
{
    bool MatchesTriangleQueryFlagsLinear(const FACFTriangle& Triangle, const uint8 RequiredQueryFlags)
    {
        return (Triangle.QueryFlags & RequiredQueryFlags) == RequiredQueryFlags;
    }
}

UACFFreeClimbableComponent::UACFFreeClimbableComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void UACFFreeClimbableComponent::OnRegister()
{
	Super::OnRegister();
	
	if (AActor* Owner = GetOwner())
	{
		MeshComponent = Owner->FindComponentByClass<USkeletalMeshComponent>();
	}

	if (UWorld* World = GetWorld())
	{
		if (UACFFreeClimbableWorldSubsystem* Subsystem = World->GetSubsystem<UACFFreeClimbableWorldSubsystem>())
		{
			Subsystem->RegisterClimbable(this);
		}
	}
}

void UACFFreeClimbableComponent::OnUnregister()
{
	if (UWorld* World = GetWorld())
	{
		if (UACFFreeClimbableWorldSubsystem* Subsystem = World->GetSubsystem<UACFFreeClimbableWorldSubsystem>())
		{
			Subsystem->UnregisterClimbable(this);
		}
	}
	
	Super::OnUnregister();
}

void UACFFreeClimbableComponent::BeginPlay()
{
	Super::BeginPlay();	

    AActor* Owner = GetOwner();
    MeshComponent = Owner->FindComponentByClass<USkeletalMeshComponent>();

    if (SurfaceAsset)
    {
        SkinnedWorldPositions.SetNum(SurfaceAsset->Vertices.Num());
        SkinnedWorldNormals.SetNum(SurfaceAsset->Vertices.Num());
        FullSkinningCacheFrame = MAX_uint64;
    }
}

void UACFFreeClimbableComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    if (bDebugSurface)
    {
        ProcessSkinning();
        DrawDebugSurface();   
    }
    DrawDebugBVHSkinnedTriangles();

	if (UWorld* World = GetWorld())
	{
		if (UACFFreeClimbableWorldSubsystem* Subsystem = World->GetSubsystem<UACFFreeClimbableWorldSubsystem>())
		{
			Subsystem->UpdateMemoryStats();
		}
	}
}

bool UACFFreeClimbableComponent::GetBroadPhaseBounds(FBox& OutBounds) const
{
	OutBounds = FBox(ForceInit);
	
	if (!MeshComponent) return false;
	
	OutBounds = MeshComponent->Bounds.GetBox();
	
	return OutBounds.IsValid != 0;
}

bool UACFFreeClimbableComponent::QuerySurface(const FACFSurfaceQuery& Query, FACFSurfaceHitResult& OutHit) const
{
	if (!SurfaceAsset || !MeshComponent) return false;
	if (SurfaceAsset->RaycastData.HasValidData(SurfaceAsset->Triangles.Num()))
	{
		return QuerySurfaceBVH(Query, OutHit);
	}
	return QuerySurfaceLinear(Query, OutHit);
}

bool UACFFreeClimbableComponent::QuerySurfaceLinear(const FACFSurfaceQuery& Query, FACFSurfaceHitResult& OutHit) const
{
    if (!SurfaceAsset) return false;

    USkeletalMesh* Mesh = SurfaceAsset->SourceMesh;
    if (!Mesh) return false;
    
    TRACE_CPUPROFILER_EVENT_SCOPE(ACFFreeClimb_QuerySurfaceLinear);
    
    const_cast<UACFFreeClimbableComponent*>(this)->ProcessSkinning();

    TArray<FMatrix44f> RefToLocalMatrices;
    MeshComponent->GetCurrentRefToLocalMatrices(RefToLocalMatrices, 0);

    bool bFound = false;
    float MinDistance = TNumericLimits<float>::Max();

    const int32 NumTriangles = SurfaceAsset->Triangles.Num();
    for (int32 TriIndex = 0; TriIndex < NumTriangles; ++TriIndex)
    {
        const FACFTriangle& Triangle = SurfaceAsset->Triangles[TriIndex];
        if (!MatchesTriangleQueryFlagsLinear(Triangle, Query.RequiredQueryFlags)) continue;

        const FVector Start = Query.Start;
        const FVector End = Query.End;

        const FVector P0 = SkinnedWorldPositions[Triangle.VertexIndices[0]];
        const FVector P1 = SkinnedWorldPositions[Triangle.VertexIndices[1]];
        const FVector P2 = SkinnedWorldPositions[Triangle.VertexIndices[2]];

        const FVector N0 = SkinnedWorldNormals[Triangle.VertexIndices[0]];
        const FVector N1 = SkinnedWorldNormals[Triangle.VertexIndices[1]];
        const FVector N2 = SkinnedWorldNormals[Triangle.VertexIndices[2]];

        FVector IntersectPoint;
        FVector IntersectNormal;
        const bool bHit = FMath::SegmentTriangleIntersection(Start, End, P0, P1, P2, IntersectPoint, IntersectNormal);

        if (!bHit) continue;

        const float Distance = FVector::Distance(Start, IntersectPoint);

        if (bHit && Distance < MinDistance)
        {
            bFound = true;
            MinDistance = Distance;

            const FVector Bary = FMath::ComputeBaryCentric2D(IntersectPoint, P0, P1, P2);
            const FVector BaryNormal = (Bary.X * N0 + Bary.Y * N1 + Bary.Z * N2).GetSafeNormal();

            OutHit.Position = IntersectPoint;
            OutHit.Normal = BaryNormal;
            OutHit.Distance = FVector::Distance(Start, IntersectPoint);

            OutHit.TriangleIndex = TriIndex;
            OutHit.Barycentric = Bary;
            OutHit.Climbable = const_cast<UACFFreeClimbableComponent*>(this);
        }
    }

	return bFound;
}

bool UACFFreeClimbableComponent::QuerySurfaceBVH(const FACFSurfaceQuery& Query, FACFSurfaceHitResult& OutHit) const
{
    if (!SurfaceAsset) return false;
	if (!MeshComponent) return false;

    TRACE_CPUPROFILER_EVENT_SCOPE(ACFFreeClimb_QuerySurfaceBVH);
	
	if (!FACFSurfaceRaycastRuntime::PrepareCacheForPose(SurfaceAsset, MeshComponent, RaycastRuntimeCache))
	{
		return false;
	}
	
	float BestDistance = TNumericLimits<float>::Max();
	FACFSurfaceHitResult BestHit;
	const FACFSurfaceBVHQuery BVHQuery{ Query.Start, Query.End };

	TArray<FACFCandidateRegion> CandidateRegions;
	FACFSurfaceRaycastRuntime::CollectCandidateRegions(SurfaceAsset, RaycastRuntimeCache, BVHQuery, CandidateRegions);
	
	for (const FACFCandidateRegion& Region : CandidateRegions)
	{
		if (Region.EntryDistance > BestDistance) break;
		
		FACFSurfaceRaycastRuntime::RaycastRegionBVHNearFirst(SurfaceAsset, Region.RegionIndex, RaycastRuntimeCache, Query, BestDistance, BestHit);
	}

	if (BestHit.TriangleIndex == INDEX_NONE) return false;
	
	OutHit = BestHit;
	OutHit.Climbable = const_cast<UACFFreeClimbableComponent*>(this);
	return true;
}

bool UACFFreeClimbableComponent::SweepSphereSurface(
    const FACFSurfaceSphereSweepQuery& Query,
    FACFSurfaceHitResult& OutHit) const
{
	if (!SurfaceAsset || !MeshComponent) return false;
    if (SurfaceAsset->RaycastData.HasValidData(SurfaceAsset->Triangles.Num()))
    {
        return SweepSphereSurfaceBVH(Query, OutHit);
    }
    return SweepSphereSurfaceLinear(Query, OutHit);
}

bool UACFFreeClimbableComponent::SweepSphereSurfaceMulti(
    const FACFSurfaceSphereSweepQuery& Query,
    TArray<FACFSurfaceHitResult>& OutHits) const
{
    OutHits.Reset();
	if (!SurfaceAsset || !MeshComponent) return false;

    if (SurfaceAsset->RaycastData.HasValidData(SurfaceAsset->Triangles.Num()))
    {
        return SweepSphereSurfaceMultiBVH(Query, OutHits);
    }

    return SweepSphereSurfaceMultiLinear(Query, OutHits);
}

bool UACFFreeClimbableComponent::SweepSphereSurfaceMultiLinear(
    const FACFSurfaceSphereSweepQuery& Query,
    TArray<FACFSurfaceHitResult>& OutHits) const
{
	if (!SurfaceAsset || !MeshComponent) return false;

    const FVector Segment = Query.End - Query.Start;
    const float SegmentLength = Segment.Size();
    if (SegmentLength <= KINDA_SMALL_NUMBER) return false;

    TRACE_CPUPROFILER_EVENT_SCOPE(ACFFreeClimb_SweepSphereSurfaceMultiLinear);
    const_cast<UACFFreeClimbableComponent*>(this)->ProcessSkinning();

    const float RadiusSq = FMath::Square(FMath::Max(Query.Radius, 0.0f));
    for (int32 TriIndex = 0; TriIndex < SurfaceAsset->Triangles.Num(); ++TriIndex)
    {
        const FACFTriangle& Triangle = SurfaceAsset->Triangles[TriIndex];
        if (!MatchesTriangleQueryFlagsLinear(Triangle, Query.RequiredQueryFlags)) continue;
        const int32 I0 = Triangle.VertexIndices[0];
        const int32 I1 = Triangle.VertexIndices[1];
        const int32 I2 = Triangle.VertexIndices[2];

        if (!SkinnedWorldPositions.IsValidIndex(I0) ||
            !SkinnedWorldPositions.IsValidIndex(I1) ||
            !SkinnedWorldPositions.IsValidIndex(I2) ||
            !SkinnedWorldNormals.IsValidIndex(I0) ||
            !SkinnedWorldNormals.IsValidIndex(I1) ||
            !SkinnedWorldNormals.IsValidIndex(I2)) continue;

        const FVector P0 = SkinnedWorldPositions[I0];
        const FVector P1 = SkinnedWorldPositions[I1];
        const FVector P2 = SkinnedWorldPositions[I2];
        FVector ClosestOnSegment, ClosestOnTriangle;
        float SegmentT = 0.0f;
        const float DistSq = ACFSurfaceMath::ClosestSegmentTriangle(
            Query.Start, Query.End, P0, P1, P2, ClosestOnSegment, ClosestOnTriangle, SegmentT);
        if (DistSq > RadiusSq) continue;

        const FVector Bary = FMath::ComputeBaryCentric2D(ClosestOnTriangle, P0, P1, P2);
        FACFSurfaceHitResult& Hit = OutHits.AddDefaulted_GetRef();
        Hit.Position = ClosestOnTriangle;
        Hit.Normal = (Bary.X * SkinnedWorldNormals[I0] + Bary.Y * SkinnedWorldNormals[I1] + Bary.Z * SkinnedWorldNormals[I2]).GetSafeNormal();
        Hit.Distance = SegmentT * SegmentLength;
        Hit.TriangleIndex = TriIndex;
        Hit.Barycentric = Bary;
        Hit.Climbable = const_cast<UACFFreeClimbableComponent*>(this);
    }

    OutHits.Sort([](const FACFSurfaceHitResult& A, const FACFSurfaceHitResult& B) { return A.Distance < B.Distance; });
    return !OutHits.IsEmpty();
}

bool UACFFreeClimbableComponent::SweepSphereSurfaceMultiBVH(
    const FACFSurfaceSphereSweepQuery& Query,
    TArray<FACFSurfaceHitResult>& OutHits) const
{
    if (!SurfaceAsset || !MeshComponent) return false;
    if ((Query.End - Query.Start).SizeSquared() <= FMath::Square(KINDA_SMALL_NUMBER)) return false;

    if (!FACFSurfaceRaycastRuntime::PrepareCacheForPose(SurfaceAsset, MeshComponent, RaycastRuntimeCache))
    {
    	return false;
    }

    const FACFSurfaceBVHQuery BVHQuery{ Query.Start, Query.End, FMath::Max(Query.Radius, 0.0f) };
    TArray<FACFCandidateRegion> CandidateRegions;
    FACFSurfaceRaycastRuntime::CollectCandidateRegions(SurfaceAsset, RaycastRuntimeCache, BVHQuery, CandidateRegions);
    for (const FACFCandidateRegion& Region : CandidateRegions)
    {
        FACFSurfaceRaycastRuntime::SweepSphereRegionBVHAll(SurfaceAsset, Region.RegionIndex, RaycastRuntimeCache, Query, OutHits);
    }

    for (FACFSurfaceHitResult& Hit : OutHits)
    {
        Hit.Climbable = const_cast<UACFFreeClimbableComponent*>(this);
    }

    OutHits.Sort([](const FACFSurfaceHitResult& A, const FACFSurfaceHitResult& B) { return A.Distance < B.Distance; });
    return !OutHits.IsEmpty();
}

bool UACFFreeClimbableComponent::SweepSphereSurfaceLinear(const FACFSurfaceSphereSweepQuery& Query, FACFSurfaceHitResult& OutHit) const
{
	if (!SurfaceAsset || !MeshComponent) return false;
    if (Query.Radius <= KINDA_SMALL_NUMBER)
    {
        FACFSurfaceQuery RayQuery;
        RayQuery.Start = Query.Start;
        RayQuery.End = Query.End;
        RayQuery.bCullBackfaces = Query.bCullBackfaces;
        RayQuery.RequiredQueryFlags = Query.RequiredQueryFlags;
        return QuerySurface(RayQuery, OutHit);
    }

    const FVector Segment = Query.End - Query.Start;
    const float SegmentLength = Segment.Size();
    if (SegmentLength <= KINDA_SMALL_NUMBER) return false;

    bool bFound = false;
    float BestDistance = TNumericLimits<float>::Max();
    const float RadiusSq = FMath::Square(Query.Radius);

    const int32 NumTriangles = SurfaceAsset->Triangles.Num();
    for (int32 TriIndex = 0; TriIndex < NumTriangles; ++TriIndex)
    {
        const FACFTriangle& Triangle = SurfaceAsset->Triangles[TriIndex];
        if (!MatchesTriangleQueryFlagsLinear(Triangle, Query.RequiredQueryFlags)) continue;

        const int32 I0 = Triangle.VertexIndices[0];
        const int32 I1 = Triangle.VertexIndices[1];
        const int32 I2 = Triangle.VertexIndices[2];

        if (!SkinnedWorldPositions.IsValidIndex(I0) ||
            !SkinnedWorldPositions.IsValidIndex(I1) ||
            !SkinnedWorldPositions.IsValidIndex(I2) ||
            !SkinnedWorldNormals.IsValidIndex(I0) ||
            !SkinnedWorldNormals.IsValidIndex(I1) ||
            !SkinnedWorldNormals.IsValidIndex(I2))
        {
            continue;
        }

        const FVector P0 = SkinnedWorldPositions[I0];
        const FVector P1 = SkinnedWorldPositions[I1];
        const FVector P2 = SkinnedWorldPositions[I2];

        FVector ClosestOnSegment;
        FVector ClosestOnTriangle;
        float SegmentT = 0.0f;
        const float DistSq = ACFSurfaceMath::ClosestSegmentTriangle(
            Query.Start, Query.End, P0, P1, P2, ClosestOnSegment, ClosestOnTriangle, SegmentT);

        if (DistSq > RadiusSq) continue;

        const float Distance = SegmentT * SegmentLength;
        if (Distance >= BestDistance) continue;

        const FVector N0 = SkinnedWorldNormals[I0];
        const FVector N1 = SkinnedWorldNormals[I1];
        const FVector N2 = SkinnedWorldNormals[I2];

        const FVector Bary = FMath::ComputeBaryCentric2D(ClosestOnTriangle, P0, P1, P2);
        const FVector BaryNormal = (Bary.X * N0 + Bary.Y * N1 + Bary.Z * N2).GetSafeNormal();

        bFound = true;
        BestDistance = Distance;

        OutHit.Position = ClosestOnTriangle;
        OutHit.Normal = BaryNormal;
        OutHit.Distance = Distance;
        OutHit.TriangleIndex = TriIndex;
        OutHit.Barycentric = Bary;
        OutHit.Climbable = const_cast<UACFFreeClimbableComponent*>(this);
    }

    return bFound;
}

bool UACFFreeClimbableComponent::SweepSphereSurfaceBVH(const FACFSurfaceSphereSweepQuery& Query, FACFSurfaceHitResult& OutHit) const
{
	if (!SurfaceAsset || !MeshComponent) return false;

	if (Query.Radius <= KINDA_SMALL_NUMBER)
	{
		FACFSurfaceQuery RayQuery;
		RayQuery.Start = Query.Start;
		RayQuery.End = Query.End;
		RayQuery.bCullBackfaces = Query.bCullBackfaces;
		RayQuery.RequiredQueryFlags = Query.RequiredQueryFlags;
		return QuerySurfaceBVH(RayQuery, OutHit);
	}

	if (!FACFSurfaceRaycastRuntime::PrepareCacheForPose(SurfaceAsset, MeshComponent, RaycastRuntimeCache))
	{
		return false;
	}

	float BestDistance = TNumericLimits<float>::Max();
	FACFSurfaceHitResult BestHit;

	const FACFSurfaceBVHQuery BVHQuery{ Query.Start, Query.End, Query.Radius };

	TArray<FACFCandidateRegion> CandidateRegions;
	FACFSurfaceRaycastRuntime::CollectCandidateRegions(
		SurfaceAsset,
		RaycastRuntimeCache,
		BVHQuery,
		CandidateRegions);

	for (const FACFCandidateRegion& Region : CandidateRegions)
	{
		if (Region.EntryDistance > BestDistance) break;

		FACFSurfaceRaycastRuntime::SweepSphereRegionBVHNearFirst(
			SurfaceAsset,
			Region.RegionIndex,
			RaycastRuntimeCache,
			Query,
			BestDistance,
			BestHit);
	}

	if (BestHit.TriangleIndex == INDEX_NONE) return false;

	OutHit = BestHit;
	OutHit.Climbable = const_cast<UACFFreeClimbableComponent*>(this);
	return true;
}

bool UACFFreeClimbableComponent::SweepCapsuleSurface(
    const FACFSurfaceCapsuleSweepQuery& Query,
    FACFSurfaceCapsuleHitResult& OutHit) const
{
	if (!SurfaceAsset || !MeshComponent) return false;
    if (SurfaceAsset && SurfaceAsset->RaycastData.HasValidData(SurfaceAsset->Triangles.Num()))
    {
        return SweepCapsuleSurfaceBVH(Query, OutHit);
    }

    return SweepCapsuleSurfaceLinear(Query, OutHit);
}

bool UACFFreeClimbableComponent::SweepCapsuleSurfaceMulti(
    const FACFSurfaceCapsuleSweepQuery& Query,
    TArray<FACFSurfaceCapsuleHitResult>& OutHits) const
{
    OutHits.Reset();
    if (!SurfaceAsset || !MeshComponent) return false;

    if (SurfaceAsset->RaycastData.HasValidData(SurfaceAsset->Triangles.Num()))
    {
        return SweepCapsuleSurfaceMultiBVH(Query, OutHits);
    }

    return SweepCapsuleSurfaceMultiLinear(Query, OutHits);
}

bool UACFFreeClimbableComponent::SweepCapsuleSurfaceMultiLinear(
    const FACFSurfaceCapsuleSweepQuery& Query,
    TArray<FACFSurfaceCapsuleHitResult>& OutHits) const
{
    if (!SurfaceAsset || Query.Radius < KINDA_SMALL_NUMBER) return false;

    TRACE_CPUPROFILER_EVENT_SCOPE(ACFFreeClimb_SweepCapsuleSurfaceMultiLinear);

    const_cast<UACFFreeClimbableComponent*>(this)->ProcessSkinning();

    const float RadiusSq = FMath::Square(Query.Radius);
    for (int32 TriIndex = 0; TriIndex < SurfaceAsset->Triangles.Num(); ++TriIndex)
    {
        const FACFTriangle& Triangle = SurfaceAsset->Triangles[TriIndex];
        if (!MatchesTriangleQueryFlagsLinear(Triangle, Query.RequiredQueryFlags)) continue;
        const int32 I0 = Triangle.VertexIndices[0];
        const int32 I1 = Triangle.VertexIndices[1];
        const int32 I2 = Triangle.VertexIndices[2];

        if (!SkinnedWorldPositions.IsValidIndex(I0) ||
            !SkinnedWorldPositions.IsValidIndex(I1) ||
            !SkinnedWorldPositions.IsValidIndex(I2) ||
            !SkinnedWorldNormals.IsValidIndex(I0) ||
            !SkinnedWorldNormals.IsValidIndex(I1) ||
            !SkinnedWorldNormals.IsValidIndex(I2))
        {
            continue;
        }

        const FVector P0 = SkinnedWorldPositions[I0];
        const FVector P1 = SkinnedWorldPositions[I1];
        const FVector P2 = SkinnedWorldPositions[I2];

        FVector ClosestOnCapsule;
        FVector ClosestOnTriangle;
        float CapsuleT = 0.0f;
        const float DistSq = ACFSurfaceMath::ClosestSegmentTriangle(
            Query.EndB, Query.EndA, P0, P1, P2,
            ClosestOnCapsule, ClosestOnTriangle, CapsuleT);

        if (DistSq > RadiusSq) continue;

        const float Dist = FMath::Sqrt(FMath::Max(DistSq, 0.0f));
        const float PenetrationDepth = Query.Radius - Dist;
        const FVector Bary = FMath::ComputeBaryCentric2D(ClosestOnTriangle, P0, P1, P2);

        const FVector N0 = SkinnedWorldNormals[I0];
        const FVector N1 = SkinnedWorldNormals[I1];
        const FVector N2 = SkinnedWorldNormals[I2];
        const FVector SurfaceNormal = (Bary.X * N0 + Bary.Y * N1 + Bary.Z * N2).GetSafeNormal();
        const FVector PushNormal = Dist > KINDA_SMALL_NUMBER
            ? (ClosestOnCapsule - ClosestOnTriangle) / Dist
            : SurfaceNormal;

        FACFSurfaceCapsuleHitResult& Hit = OutHits.AddDefaulted_GetRef();
        Hit.SurfaceHit.Position = ClosestOnTriangle;
        Hit.SurfaceHit.Normal = SurfaceNormal;
        Hit.SurfaceHit.Distance = Dist;
        Hit.SurfaceHit.TriangleIndex = TriIndex;
        Hit.SurfaceHit.Barycentric = Bary;
        Hit.SurfaceHit.Climbable = const_cast<UACFFreeClimbableComponent*>(this);
        Hit.CapsulePoint = ClosestOnCapsule;
        Hit.PushNormal = PushNormal;
        Hit.PenetrationDepth = PenetrationDepth;
    }

    return !OutHits.IsEmpty();
}

bool UACFFreeClimbableComponent::SweepCapsuleSurfaceMultiBVH(
    const FACFSurfaceCapsuleSweepQuery& Query,
    TArray<FACFSurfaceCapsuleHitResult>& OutHits) const
{
    if (!SurfaceAsset || !MeshComponent || Query.Radius < KINDA_SMALL_NUMBER) return false;

    if (!FACFSurfaceRaycastRuntime::PrepareCacheForPose(SurfaceAsset, MeshComponent, RaycastRuntimeCache))
    {
    	return false;
    }

    FACFSurfaceCapsuleBVHQuery BVHQuery;
    BVHQuery.StartA = Query.StartA;
    BVHQuery.StartB = Query.StartB;
    BVHQuery.EndA = Query.EndA;
    BVHQuery.EndB = Query.EndB;
    BVHQuery.Radius = Query.Radius;

    TArray<FACFCandidateRegion> CandidateRegions;
    FACFSurfaceRaycastRuntime::CollectCandidateRegions(
        SurfaceAsset,
        RaycastRuntimeCache,
        BVHQuery,
        CandidateRegions);

    for (const FACFCandidateRegion& Region : CandidateRegions)
    {
        FACFSurfaceRaycastRuntime::SweepCapsuleRegionBVHAll(
            SurfaceAsset,
            Region.RegionIndex,
            RaycastRuntimeCache,
            Query,
            OutHits);
    }

    for (FACFSurfaceCapsuleHitResult& Hit : OutHits)
    {
        Hit.SurfaceHit.Climbable = const_cast<UACFFreeClimbableComponent*>(this);
    }

    return !OutHits.IsEmpty();
}

bool UACFFreeClimbableComponent::SweepCapsuleSurfaceLinear(
    const FACFSurfaceCapsuleSweepQuery& Query,
    FACFSurfaceCapsuleHitResult& OutHit) const
{
    if (!SurfaceAsset) return false;
    if (Query.Radius < KINDA_SMALL_NUMBER) return false;

    TRACE_CPUPROFILER_EVENT_SCOPE(ACFFreeClimb_SweepCapsuleSurfaceLinear);

    const_cast<UACFFreeClimbableComponent*>(this)->ProcessSkinning();

    float BestPenetration = 0.0f;
    FACFSurfaceCapsuleHitResult BestHit;
    const float RadiusSq = FMath::Square(Query.Radius);

    for (int32 TriIndex = 0; TriIndex < SurfaceAsset->Triangles.Num(); ++TriIndex)
    {
        const FACFTriangle& Triangle = SurfaceAsset->Triangles[TriIndex];
        if (!MatchesTriangleQueryFlagsLinear(Triangle, Query.RequiredQueryFlags)) continue;
        const int32 I0 = Triangle.VertexIndices[0];
        const int32 I1 = Triangle.VertexIndices[1];
        const int32 I2 = Triangle.VertexIndices[2];

        if (!SkinnedWorldPositions.IsValidIndex(I0) ||
            !SkinnedWorldPositions.IsValidIndex(I1) ||
            !SkinnedWorldPositions.IsValidIndex(I2) ||
            !SkinnedWorldNormals.IsValidIndex(I0) ||
            !SkinnedWorldNormals.IsValidIndex(I1) ||
            !SkinnedWorldNormals.IsValidIndex(I2))
        {
            continue;
        }

        const FVector P0 = SkinnedWorldPositions[I0];
        const FVector P1 = SkinnedWorldPositions[I1];
        const FVector P2 = SkinnedWorldPositions[I2];

        FVector ClosestOnCapsule;
        FVector ClosestOnTriangle;
        float CapsuleT = 0.0f;
        const float DistSq = ACFSurfaceMath::ClosestSegmentTriangle(
            Query.EndB, Query.EndA, P0, P1, P2,
            ClosestOnCapsule, ClosestOnTriangle, CapsuleT);

        if (DistSq > RadiusSq) continue;

        const float Dist = FMath::Sqrt(FMath::Max(DistSq, 0.0f));
        const float PenetrationDepth = Query.Radius - Dist;
        const FVector Bary = FMath::ComputeBaryCentric2D(ClosestOnTriangle, P0, P1, P2);

        const FVector N0 = SkinnedWorldNormals[I0];
        const FVector N1 = SkinnedWorldNormals[I1];
        const FVector N2 = SkinnedWorldNormals[I2];
        const FVector SurfaceNormal = (Bary.X * N0 + Bary.Y * N1 + Bary.Z * N2).GetSafeNormal();
        const FVector PushNormal = Dist > KINDA_SMALL_NUMBER
            ? (ClosestOnCapsule - ClosestOnTriangle) / Dist
            : SurfaceNormal;

        BestHit.BlendedPushDir += PushNormal * PenetrationDepth;
        BestHit.MaxPenetrationDepth = FMath::Max(BestHit.MaxPenetrationDepth, PenetrationDepth);

        if (PenetrationDepth <= BestPenetration) continue;

        BestPenetration = PenetrationDepth;
        BestHit.SurfaceHit.Position = ClosestOnTriangle;
        BestHit.SurfaceHit.Normal = SurfaceNormal;
        BestHit.SurfaceHit.Distance = Dist;
        BestHit.SurfaceHit.TriangleIndex = TriIndex;
        BestHit.SurfaceHit.Barycentric = Bary;
        BestHit.CapsulePoint = ClosestOnCapsule;
        BestHit.PushNormal = PushNormal;
        BestHit.PenetrationDepth = PenetrationDepth;
    }

    if (BestHit.SurfaceHit.TriangleIndex == INDEX_NONE) return false;

    OutHit = BestHit;
    OutHit.SurfaceHit.Climbable = const_cast<UACFFreeClimbableComponent*>(this);
    return true;
}

bool UACFFreeClimbableComponent::SweepCapsuleSurfaceBVH(
    const FACFSurfaceCapsuleSweepQuery& Query,
    FACFSurfaceCapsuleHitResult& OutHit) const
{
	if (!SurfaceAsset || !MeshComponent) return false;

    if (Query.Radius < KINDA_SMALL_NUMBER) return false;

    if (!FACFSurfaceRaycastRuntime::PrepareCacheForPose(SurfaceAsset, MeshComponent, RaycastRuntimeCache))
    {
    	return false;
    }

    FACFSurfaceCapsuleBVHQuery BVHQuery;
    BVHQuery.StartA = Query.StartA;
    BVHQuery.StartB = Query.StartB;
    BVHQuery.EndA = Query.EndA;
    BVHQuery.EndB = Query.EndB;
    BVHQuery.Radius = Query.Radius;

    TArray<FACFCandidateRegion> CandidateRegions;
    FACFSurfaceRaycastRuntime::CollectCandidateRegions(
        SurfaceAsset,
        RaycastRuntimeCache,
        BVHQuery,
        CandidateRegions);

    float BestPenetration = 0.0f;
    FACFSurfaceCapsuleHitResult BestHit;

    for (const FACFCandidateRegion& Region : CandidateRegions)
    {
        FACFSurfaceRaycastRuntime::SweepCapsuleRegionBVHNearFirst(
            SurfaceAsset,
            Region.RegionIndex,
            RaycastRuntimeCache,
            Query,
            BestPenetration,
            BestHit);
    }

    if (BestHit.SurfaceHit.TriangleIndex == INDEX_NONE) return false;

    OutHit = BestHit;
    OutHit.SurfaceHit.Climbable = const_cast<UACFFreeClimbableComponent*>(this);
    return true;
}

bool UACFFreeClimbableComponent::ResolveAnchor(int32 TriangleIndex, const FVector& Barycentric,
                                             FVector& OutPosition, FVector& OutNormal) const
{
	if (!SurfaceAsset) return false;

	if (SurfaceAsset->RaycastData.HasValidData(SurfaceAsset->Triangles.Num()))
	{
		return ResolveAnchorBVH(TriangleIndex, Barycentric, OutPosition, OutNormal);
	}

	return ResolveAnchorLinear(TriangleIndex, Barycentric, OutPosition, OutNormal);
}

bool UACFFreeClimbableComponent::ResolveTriangleFrameRotation(int32 TriangleIndex, FQuat& OutRotation) const
{
	FVector P0, P1, P2;
	if (!ResolveTriangleVertices(TriangleIndex, P0, P1, P2)) return false;
	
	const FVector Edge01 = P1 - P0;
	const FVector Edge02 = P2 - P0;

	const FVector Tangent = Edge01.GetSafeNormal();
	const FVector Normal = FVector::CrossProduct(Edge01, Edge02).GetSafeNormal();
	if (Tangent.IsNearlyZero() || Normal.IsNearlyZero()) return false;

	OutRotation = FRotationMatrix::MakeFromXZ(Tangent, Normal).ToQuat().GetNormalized();

	return !OutRotation.ContainsNaN();
}

bool UACFFreeClimbableComponent::ResolveAnchorLinear(int32 TriangleIndex, const FVector& Barycentric, FVector& OutPosition, FVector& OutNormal) const
{
	if (!SurfaceAsset || !SurfaceAsset->Triangles.IsValidIndex(TriangleIndex)) return false;

	const_cast<UACFFreeClimbableComponent*>(this)->ProcessSkinning();
	
	const FACFTriangle& Triangle = SurfaceAsset->Triangles[TriangleIndex];
	const TArray<FVector>& Skinned = SkinnedWorldPositions;

	const int32 I0 = Triangle.VertexIndices[0];
	const int32 I1 = Triangle.VertexIndices[1];
	const int32 I2 = Triangle.VertexIndices[2];
	if (!Skinned.IsValidIndex(I0) || !Skinned.IsValidIndex(I1) || !Skinned.IsValidIndex(I2)) return false;

	const FVector P0 = Skinned[I0];
	const FVector P1 = Skinned[I1];
	const FVector P2 = Skinned[I2];

	OutPosition = Barycentric.X * P0 + Barycentric.Y * P1 + Barycentric.Z * P2;

	const FVector N0 = SkinnedWorldNormals[I0];
	const FVector N1 = SkinnedWorldNormals[I1];
	const FVector N2 = SkinnedWorldNormals[I2];

	OutNormal = (Barycentric.X * N0 + Barycentric.Y * N1 + Barycentric.Z * N2).GetSafeNormal();
	return true;
}

bool UACFFreeClimbableComponent::ResolveAnchorBVH(int32 TriangleIndex, const FVector& Barycentric, FVector& OutPosition, FVector& OutNormal) const
{
	if (!SurfaceAsset || !MeshComponent || !SurfaceAsset->Triangles.IsValidIndex(TriangleIndex)) return false;

	if (!FACFSurfaceRaycastRuntime::PrepareCacheForPose(SurfaceAsset, MeshComponent, RaycastRuntimeCache)) return false;

	const FACFTriangle& Triangle = SurfaceAsset->Triangles[TriangleIndex];

	const int32 I0 = Triangle.VertexIndices[0];
	const int32 I1 = Triangle.VertexIndices[1];
	const int32 I2 = Triangle.VertexIndices[2];

	FVector P0, P1, P2;
	FVector N0, N1, N2;

	const bool bResolved =
		FACFSurfaceRaycastRuntime::SkinVertexWS(SurfaceAsset, I0, RaycastRuntimeCache, P0, N0) &&
		FACFSurfaceRaycastRuntime::SkinVertexWS(SurfaceAsset, I1, RaycastRuntimeCache, P1, N1) &&
		FACFSurfaceRaycastRuntime::SkinVertexWS(SurfaceAsset, I2, RaycastRuntimeCache, P2, N2);

	if (!bResolved)
	{
		return false;
	}

	OutPosition = Barycentric.X * P0 + Barycentric.Y * P1 + Barycentric.Z * P2;
	OutNormal = (Barycentric.X * N0 + Barycentric.Y * N1 + Barycentric.Z * N2).GetSafeNormal();

	return true;
}

void UACFFreeClimbableComponent::ProcessSkinning()
{
    if (!SurfaceAsset || !MeshComponent || SurfaceAsset->Vertices.IsEmpty()) return;

	const int32 NumVertices = SurfaceAsset->Vertices.Num();
	if (SkinnedWorldPositions.Num() != NumVertices || SkinnedWorldNormals.Num() != NumVertices)
	{
		SkinnedWorldPositions.SetNum(NumVertices);
		SkinnedWorldNormals.SetNum(NumVertices);
		FullSkinningCacheFrame = MAX_uint64;
	}

	if (FullSkinningCacheFrame == GFrameCounter) return;

    USkeletalMesh* Mesh = SurfaceAsset->SourceMesh;
    if (!Mesh) return;

    TRACE_CPUPROFILER_EVENT_SCOPE(Surface_Skinning);

    TArray<FMatrix44f> RefToLocalMatrices;
    MeshComponent->GetCurrentRefToLocalMatrices(RefToLocalMatrices, 0);

    FTransform ComponentTransform = MeshComponent->GetComponentTransform();

    int32 NumInfluences = SurfaceAsset->Vertices[0].BoneIndices.Num();

    for (int32 VertexIndex = 0; VertexIndex < NumVertices; ++VertexIndex)
    {
        FVector SkinnedPos = FVector::ZeroVector;
        FVector SkinnedNormal = FVector::ZeroVector;

        for (int32 InfluenceIndex = 0; InfluenceIndex < NumInfluences; ++InfluenceIndex)
        {
            int32 BoneIndex = SurfaceAsset->Vertices[VertexIndex].BoneIndices[InfluenceIndex];
            float BoneWeight = SurfaceAsset->Vertices[VertexIndex].BoneWeights[InfluenceIndex];

            if (BoneIndex == INDEX_NONE || BoneWeight <= 0.0f) continue;

            FVector3f LocalPos = FVector3f(SurfaceAsset->Vertices[VertexIndex].Position);
            FVector3f BonePos = RefToLocalMatrices[BoneIndex].TransformPosition(LocalPos);

            FVector3f LocalNormal = FVector3f(SurfaceAsset->Vertices[VertexIndex].Normal);
            FVector3f BoneNormal = RefToLocalMatrices[BoneIndex].TransformVector(LocalNormal);

            SkinnedPos += FVector(BonePos) * BoneWeight;
            SkinnedNormal += FVector(BoneNormal) * BoneWeight;
        }

        SkinnedWorldPositions[VertexIndex] = ComponentTransform.TransformPosition(SkinnedPos);
        SkinnedWorldNormals[VertexIndex] = ComponentTransform.TransformVector(SkinnedNormal).GetSafeNormal();
    }
	
	FullSkinningCacheFrame = GFrameCounter;
}

void UACFFreeClimbableComponent::DrawDebugSurface()
{
    if (!bDebugSurface) return;
    if (!SurfaceAsset) return;
	
    const USkeletalMesh* Mesh = SurfaceAsset->SourceMesh;

    TArray<FMatrix44f> RefToLocalMatrices;
    MeshComponent->GetCurrentRefToLocalMatrices(RefToLocalMatrices, 0);

    const FTransform& Transform = MeshComponent->GetComponentTransform();

    for (const FACFTriangle& Triangle : SurfaceAsset->Triangles)
    {
        const FVector P0 = SkinnedWorldPositions[Triangle.VertexIndices[0]];
        const FVector P1 = SkinnedWorldPositions[Triangle.VertexIndices[1]];
        const FVector P2 = SkinnedWorldPositions[Triangle.VertexIndices[2]];

        DrawDebugLine(GetWorld(), P0, P1, FColor::Green, false, -1.0f, 0, 1.0f);
        DrawDebugLine(GetWorld(), P1, P2, FColor::Green, false, -1.0f, 0, 1.0f);
        DrawDebugLine(GetWorld(), P2, P0, FColor::Green, false, -1.0f, 0, 1.0f);
    }
}

void UACFFreeClimbableComponent::DrawDebugBVHSkinnedTriangles()
{
    if (!bDebugBVHSurface) return;
    if (!SurfaceAsset) return;
    if (RaycastRuntimeCache.CacheFrame != GFrameCounter) return;

    for (int32 TriIndex = 0; TriIndex < SurfaceAsset->Triangles.Num(); ++TriIndex)
    {
        const FACFTriangle& Triangle = SurfaceAsset->Triangles[TriIndex];

        const int32 I0 = Triangle.VertexIndices[0];
        const int32 I1 = Triangle.VertexIndices[1];
        const int32 I2 = Triangle.VertexIndices[2];

        if (!RaycastRuntimeCache.MicroSkinnedVertexVersions.IsValidIndex(I0) ||
            !RaycastRuntimeCache.MicroSkinnedVertexVersions.IsValidIndex(I1) ||
            !RaycastRuntimeCache.MicroSkinnedVertexVersions.IsValidIndex(I2))
        {
            continue;
        }

        const bool bAllVertsSkinned =
            RaycastRuntimeCache.MicroSkinnedVertexVersions[I0] == RaycastRuntimeCache.PoseVersion &&
            RaycastRuntimeCache.MicroSkinnedVertexVersions[I1] == RaycastRuntimeCache.PoseVersion &&
            RaycastRuntimeCache.MicroSkinnedVertexVersions[I2] == RaycastRuntimeCache.PoseVersion;

        if (!bAllVertsSkinned) continue;

        const FVector P0 = RaycastRuntimeCache.MicroSkinnedPositionsWS[I0];
        const FVector P1 = RaycastRuntimeCache.MicroSkinnedPositionsWS[I1];
        const FVector P2 = RaycastRuntimeCache.MicroSkinnedPositionsWS[I2];

        DrawDebugLine(GetWorld(), P0, P1, FColor::Cyan, false, -1.0f, 0, 1.0f);
        DrawDebugLine(GetWorld(), P1, P2, FColor::Cyan, false, -1.0f, 0, 1.0f);
        DrawDebugLine(GetWorld(), P2, P0, FColor::Cyan, false, -1.0f, 0, 1.0f);
    }
}

bool UACFFreeClimbableComponent::ResolveTriangleVertices(int32 TriangleIndex, FVector& OutP0, FVector& OutP1, FVector& OutP2) const
{
	if (!SurfaceAsset || !MeshComponent || !SurfaceAsset->Triangles.IsValidIndex(TriangleIndex)) return false;

	const FACFTriangle& Triangle = SurfaceAsset->Triangles[TriangleIndex];
	const int32 I0 = Triangle.VertexIndices[0];
	const int32 I1 = Triangle.VertexIndices[1];
	const int32 I2 = Triangle.VertexIndices[2];

	if (!FACFSurfaceRaycastRuntime::PrepareCacheForPose(SurfaceAsset, MeshComponent, RaycastRuntimeCache)) return false;

	FVector N0, N1, N2;
	return FACFSurfaceRaycastRuntime::SkinVertexWS(SurfaceAsset, I0, RaycastRuntimeCache, OutP0, N0) &&
		FACFSurfaceRaycastRuntime::SkinVertexWS(SurfaceAsset, I1, RaycastRuntimeCache, OutP1, N1) &&
		FACFSurfaceRaycastRuntime::SkinVertexWS(SurfaceAsset, I2, RaycastRuntimeCache, OutP2, N2);
}

