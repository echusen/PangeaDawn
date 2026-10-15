// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "ACFFreeClimbableWorldSubsystem.h"
#include "ACFFreeClimbableComponent.h"
#include "ACFSurfaceAsset.h"
#include "ACFSurfaceMath.h"
#include "ACFSurfaceMemory.h"
#include "CollisionQueryParams.h"
#include "GameFramework/Actor.h"

namespace
{
	struct FACFFreeClimbableCandidate
	{
		UACFFreeClimbableComponent* Climbable = nullptr;
		float EntryDistance = 0.0f;
	};

	bool AcceptsClimbableOwner(const UACFFreeClimbableComponent* Climbable, const AActor* IgnoredActor, TFunctionRef<bool(const AActor*)> ShouldAcceptActor)
	{
		if (!Climbable)
		{
			return false;
		}

		const AActor* Owner = Climbable->GetOwner();
		if (IgnoredActor && Owner == IgnoredActor)
		{
			return false;
		}

		return ShouldAcceptActor(Owner);
	}

	TFunctionRef<bool(const AActor*)> AcceptAllClimbableActors()
	{
		static const auto AcceptAll = [](const AActor*) { return true; };
		return AcceptAll;
	}
}

void UACFFreeClimbableWorldSubsystem::RegisterClimbable(UACFFreeClimbableComponent* Climbable)
{
	if (!Climbable) return;
	
	Climbables.AddUnique(TWeakObjectPtr<UACFFreeClimbableComponent>(Climbable));
	LastMemoryStatsFrame = MAX_uint64;
}

void UACFFreeClimbableWorldSubsystem::UnregisterClimbable(UACFFreeClimbableComponent* Climbable)
{
	if (!Climbable) return;
	Climbables.RemoveAllSwap([Climbable](const TWeakObjectPtr<UACFFreeClimbableComponent>& Entry)
	{
		return !Entry.IsValid() || Entry.Get() == Climbable;
	});
	LastMemoryStatsFrame = MAX_uint64;
	UpdateMemoryStats();
}

void UACFFreeClimbableWorldSubsystem::UpdateMemoryStats()
{
#if STATS
	static constexpr uint64 MemoryStatsUpdateInterval = 30;
	if ((LastMemoryStatsFrame != MAX_uint64 &&
			GFrameCounter - LastMemoryStatsFrame < MemoryStatsUpdateInterval) ||
		!GET_STATID(STAT_ClimbingSystemTotalMemory).IsValidStat())
	{
		return;
	}

	LastMemoryStatsFrame = GFrameCounter;

	SIZE_T MeshDataBytes = 0;
	SIZE_T CollisionDataBytes = 0;
	SIZE_T RuntimeCacheBytes = 0;
	int32 ClimbableCount = 0;
	TSet<const UACFSurfaceAsset*> CountedSurfaceAssets;

	for (const TWeakObjectPtr<UACFFreeClimbableComponent>& WeakClimbable : Climbables)
	{
		const UACFFreeClimbableComponent* Climbable = WeakClimbable.Get();
		if (!Climbable) continue;

		++ClimbableCount;

		const UACFSurfaceAsset* SurfaceAsset = Climbable->SurfaceAsset;
		if (SurfaceAsset && !CountedSurfaceAssets.Contains(SurfaceAsset))
		{
			CountedSurfaceAssets.Add(SurfaceAsset);

			const SIZE_T SurfaceAssetBytes = ACFSurfaceMemory::GetSurfaceAssetAllocatedSize(SurfaceAsset);
			const SIZE_T SurfaceCollisionBytes = ACFSurfaceMemory::GetRaycastDataAllocatedSize(SurfaceAsset);
			MeshDataBytes += SurfaceAssetBytes > SurfaceCollisionBytes
				? SurfaceAssetBytes - SurfaceCollisionBytes
				: 0;
			CollisionDataBytes += SurfaceCollisionBytes;
		}

		RuntimeCacheBytes += ACFSurfaceMemory::GetRuntimeCacheAllocatedSize(Climbable->RaycastRuntimeCache);
	}

	SET_DWORD_STAT(STAT_ClimbingSystemClimbableCount, ClimbableCount);
	SET_MEMORY_STAT(STAT_ClimbingSystemMeshDataMemory, MeshDataBytes);
	SET_MEMORY_STAT(STAT_ClimbingSystemCollisionDataMemory, CollisionDataBytes);
	SET_MEMORY_STAT(STAT_ClimbingSystemRuntimeCacheMemory, RuntimeCacheBytes);
	SET_MEMORY_STAT(
		STAT_ClimbingSystemTotalMemory,
		MeshDataBytes + CollisionDataBytes + RuntimeCacheBytes);
#endif
}

bool UACFFreeClimbableWorldSubsystem::QuerySurface(const FACFSurfaceQuery& Query, const AActor* IgnoredActor, FACFSurfaceHitResult& OutHit) const
{
	return QuerySurface(Query, IgnoredActor, OutHit, AcceptAllClimbableActors());
}

bool UACFFreeClimbableWorldSubsystem::QuerySurface(const FACFSurfaceQuery& Query, const AActor* IgnoredActor, FACFSurfaceHitResult& OutHit, TFunctionRef<bool(const AActor*)> ShouldAcceptActor) const
{
	TArray<FACFFreeClimbableCandidate, TInlineAllocator<8>> Candidates;
	Candidates.Reserve(Climbables.Num());
	
	for (const TWeakObjectPtr<UACFFreeClimbableComponent>& WeakClimbable : Climbables)
	{
		UACFFreeClimbableComponent* Climbable = WeakClimbable.Get();
		if (!AcceptsClimbableOwner(Climbable, IgnoredActor, ShouldAcceptActor)) continue;
		
		FBox BroadPhaseBounds(ForceInit);
		if(!Climbable->GetBroadPhaseBounds(BroadPhaseBounds)) continue;
		
		float EntryDistance = 0.0f;
		if (!ACFSurfaceMath::SegmentIntersectsAABB(Query.Start, Query.End, BroadPhaseBounds, EntryDistance)) continue;
		
		Candidates.Add({Climbable, EntryDistance});
	}
	
	Candidates.Sort([](const FACFFreeClimbableCandidate& A, const FACFFreeClimbableCandidate& B)
	{
		return A.EntryDistance < B.EntryDistance;
	});
	
	bool bFound = false;
	float BestDistance = TNumericLimits<float>::Max();
	
	for (const FACFFreeClimbableCandidate& Candidate : Candidates)
	{
		if (Candidate.EntryDistance > BestDistance) break;
		
		FACFSurfaceHitResult CandidateHit;
		if (!Candidate.Climbable->QuerySurface(Query, CandidateHit)) continue;
		if (CandidateHit.Distance >= BestDistance) continue;
		
		bFound = true;
		BestDistance = CandidateHit.Distance;
		OutHit = CandidateHit;
	}
	
	return bFound;
}

bool UACFFreeClimbableWorldSubsystem::SweepSphereSurface(const FACFSurfaceSphereSweepQuery& Query, const AActor* IgnoredActor, FACFSurfaceHitResult& OutHit) const
{
	return SweepSphereSurface(Query, IgnoredActor, OutHit, AcceptAllClimbableActors());
}

bool UACFFreeClimbableWorldSubsystem::SweepSphereSurface(const FACFSurfaceSphereSweepQuery& Query, const AActor* IgnoredActor, FACFSurfaceHitResult& OutHit, TFunctionRef<bool(const AActor*)> ShouldAcceptActor) const
{
	OutHit = FACFSurfaceHitResult{};
	
	const float Radius = FMath::Max(Query.Radius, 0.0f);
	
	TArray<FACFFreeClimbableCandidate, TInlineAllocator<8>> Candidates;
	Candidates.Reserve(Climbables.Num());
	
	for (const TWeakObjectPtr<UACFFreeClimbableComponent>& WeakClimbable : Climbables)
	{
		UACFFreeClimbableComponent* Climbable = WeakClimbable.Get();
		if (!AcceptsClimbableOwner(Climbable, IgnoredActor, ShouldAcceptActor)) continue;
		
		FBox BroadPhaseBounds(ForceInit);
		if (!Climbable->GetBroadPhaseBounds(BroadPhaseBounds)) continue;
		
		const FBox ExpandedBounds = BroadPhaseBounds.ExpandBy(Radius);
		
		float EntryDistance = 0.0f;
		if (!ACFSurfaceMath::SegmentIntersectsAABB(Query.Start, Query.End, ExpandedBounds, EntryDistance)) continue;
		
		Candidates.Add({Climbable, EntryDistance});
	}
	
	Candidates.Sort([](const FACFFreeClimbableCandidate& A, const FACFFreeClimbableCandidate& B)
	   {
		   return A.EntryDistance < B.EntryDistance;
	   });

	bool bFound = false;
	float BestDistance = TNumericLimits<float>::Max();
	for (const FACFFreeClimbableCandidate& Candidate : Candidates)
	{
		if (Candidate.EntryDistance > BestDistance) break;

		FACFSurfaceHitResult CandidateHit;
		if (!Candidate.Climbable->SweepSphereSurface(Query, CandidateHit)) continue;

		if (CandidateHit.Distance >= BestDistance) continue;

		bFound = true;
		BestDistance = CandidateHit.Distance;
		OutHit = CandidateHit;
	}

	return bFound;
}

bool UACFFreeClimbableWorldSubsystem::SweepSphereSurfaceMulti(const FACFSurfaceSphereSweepQuery& Query, const AActor* IgnoredActor, TArray<FACFSurfaceHitResult>& OutHits) const
{
	return SweepSphereSurfaceMulti(Query, IgnoredActor, OutHits, AcceptAllClimbableActors());
}

bool UACFFreeClimbableWorldSubsystem::SweepSphereSurfaceMulti(const FACFSurfaceSphereSweepQuery& Query, const AActor* IgnoredActor, TArray<FACFSurfaceHitResult>& OutHits, TFunctionRef<bool(const AActor*)> ShouldAcceptActor) const
{
	OutHits.Reset();

    const float Radius = FMath::Max(Query.Radius, 0.0f);

    TArray<FACFFreeClimbableCandidate, TInlineAllocator<8>> Candidates;
    Candidates.Reserve(Climbables.Num());

    for (const TWeakObjectPtr<UACFFreeClimbableComponent>& WeakClimbable : Climbables)
    {
        UACFFreeClimbableComponent* Climbable = WeakClimbable.Get();
        if (!AcceptsClimbableOwner(Climbable, IgnoredActor, ShouldAcceptActor)) continue;

        FBox BroadPhaseBounds(ForceInit);
        if (!Climbable->GetBroadPhaseBounds(BroadPhaseBounds)) continue;

        const FBox ExpandedBounds = BroadPhaseBounds.ExpandBy(Radius);

        float EntryDistance = 0.0f;
        if (!ACFSurfaceMath::SegmentIntersectsAABB(Query.Start, Query.End, ExpandedBounds, EntryDistance)) continue;

        Candidates.Add({ Climbable, EntryDistance });
    }

    TArray<FACFSurfaceHitResult> CandidateHits;

    for (const FACFFreeClimbableCandidate& Candidate : Candidates)
    {
        if (!Candidate.Climbable->SweepSphereSurfaceMulti(Query, CandidateHits)) continue;
        OutHits.Append(CandidateHits);
    }

    OutHits.Sort([](const FACFSurfaceHitResult& A, const FACFSurfaceHitResult& B)
        {
            return A.Distance < B.Distance;
        });

    return !OutHits.IsEmpty();
}

bool UACFFreeClimbableWorldSubsystem::SweepCapsuleSurface(const FACFSurfaceCapsuleSweepQuery& Query, const AActor* IgnoredActor, FACFSurfaceCapsuleHitResult& OutHit) const
{
	return SweepCapsuleSurface(Query, IgnoredActor, OutHit, AcceptAllClimbableActors());
}

bool UACFFreeClimbableWorldSubsystem::SweepCapsuleSurface(const FACFSurfaceCapsuleSweepQuery& Query, const AActor* IgnoredActor, FACFSurfaceCapsuleHitResult& OutHit, TFunctionRef<bool(const AActor*)> ShouldAcceptActor) const
{
	OutHit = FACFSurfaceCapsuleHitResult{};

    if (Query.Radius < KINDA_SMALL_NUMBER) return false;

    FBox QueryBounds(ForceInit);
    QueryBounds += Query.StartA;
    QueryBounds += Query.StartB;
    QueryBounds += Query.EndA;
    QueryBounds += Query.EndB;

    QueryBounds = QueryBounds.ExpandBy(Query.Radius);

    bool bFound = false;

    float BestPenetrationDepth = 0.0f;
    float MaxPenetrationDepth = 0.0f;

    FVector BlendedPushDir = FVector::ZeroVector;

    FACFSurfaceCapsuleHitResult BestHit;

    for (const TWeakObjectPtr<UACFFreeClimbableComponent>& WeakClimbable : Climbables)
    {
        UACFFreeClimbableComponent* Climbable = WeakClimbable.Get();
        if (!AcceptsClimbableOwner(Climbable, IgnoredActor, ShouldAcceptActor)) continue;

        FBox BroadPhaseBounds(ForceInit);
        if (!Climbable->GetBroadPhaseBounds(BroadPhaseBounds)) continue;

        if (!QueryBounds.Intersect(BroadPhaseBounds)) continue;

        FACFSurfaceCapsuleHitResult CandidateHit;
        if (!Climbable->SweepCapsuleSurface(Query, CandidateHit)) continue;

        bFound = true;

        BlendedPushDir += CandidateHit.BlendedPushDir;
        MaxPenetrationDepth = FMath::Max(MaxPenetrationDepth, CandidateHit .MaxPenetrationDepth);

        if (CandidateHit.PenetrationDepth <= BestPenetrationDepth) continue;

        BestPenetrationDepth = CandidateHit.PenetrationDepth;
        BestHit = CandidateHit;
    }

    if (!bFound) return false;

    OutHit = BestHit;
    OutHit.BlendedPushDir = BlendedPushDir;
    OutHit.MaxPenetrationDepth = MaxPenetrationDepth;

    return true;
}
