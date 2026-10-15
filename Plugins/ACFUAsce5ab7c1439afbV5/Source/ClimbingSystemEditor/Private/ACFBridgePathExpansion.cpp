// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "ACFBridgePathExpansion.h"

#include "Algo/Reverse.h"
#include "Containers/Queue.h"

namespace
{
    struct FTriangleEdgeUse
    {
        int32 TriangleIndex = INDEX_NONE;
        int32 VertexIndexA = INDEX_NONE;
        int32 VertexIndexB = INDEX_NONE;
    };

    bool FindTrianglePath(
        const FACFBridgePathTopology& Topology,
        int32 StartTriangleIndex,
        int32 EndTriangleIndex,
        TArray<int32>& OutTrianglePath)
    {
        OutTrianglePath.Reset();

        if (!Topology.TriangleNeighbors.IsValidIndex(StartTriangleIndex) ||
            !Topology.TriangleNeighbors.IsValidIndex(EndTriangleIndex))
        {
            return false;
        }

        TArray<int32> Parents;
        Parents.Init(INDEX_NONE, Topology.TriangleNeighbors.Num());

        TQueue<int32> Queue;
        Queue.Enqueue(StartTriangleIndex);
        Parents[StartTriangleIndex] = StartTriangleIndex;

        while (!Queue.IsEmpty())
        {
            int32 CurrentTriangleIndex = INDEX_NONE;
            Queue.Dequeue(CurrentTriangleIndex);

            if (CurrentTriangleIndex == EndTriangleIndex)
            {
                break;
            }

            for (const FACFBridgeTriangleNeighbor& Neighbor : Topology.TriangleNeighbors[CurrentTriangleIndex])
            {
                if (Parents[Neighbor.TriangleIndex] != INDEX_NONE)
                {
                    continue;
                }

                Parents[Neighbor.TriangleIndex] = CurrentTriangleIndex;
                Queue.Enqueue(Neighbor.TriangleIndex);
            }
        }

        if (Parents[EndTriangleIndex] == INDEX_NONE)
        {
            return false;
        }

        for (int32 TriangleIndex = EndTriangleIndex;; TriangleIndex = Parents[TriangleIndex])
        {
            OutTrianglePath.Add(TriangleIndex);

            if (TriangleIndex == StartTriangleIndex)
            {
                break;
            }
        }

        Algo::Reverse(OutTrianglePath);
        return true;
    }

    bool TryMakeSharedEdgeProjectedPoint(
        const UACFSurfaceAsset* Asset,
        const FACFBridgePoint& CurrentPoint,
        const FACFBridgePoint& TargetPoint,
        int32 CurrentTriangleIndex,
        int32 DestinationTriangleIndex,
        int32 SharedVertexIndexA,
        int32 SharedVertexIndexB,
        FACFBridgePoint& OutPoint)
    {
        if (!Asset ||
            !Asset->Triangles.IsValidIndex(CurrentTriangleIndex) ||
            !Asset->Triangles.IsValidIndex(DestinationTriangleIndex))
        {
            return false;
        }

        const FACFTriangle& CurrentTriangle = Asset->Triangles[CurrentTriangleIndex];
        const int32 I0 = CurrentTriangle.VertexIndices[0];
        const int32 I1 = CurrentTriangle.VertexIndices[1];
        const int32 I2 = CurrentTriangle.VertexIndices[2];

        if (!Asset->Vertices.IsValidIndex(I0) ||
            !Asset->Vertices.IsValidIndex(I1) ||
            !Asset->Vertices.IsValidIndex(I2))
        {
            return false;
        }

        const FVector& A = Asset->Vertices[I0].Position;
        const FVector& B = Asset->Vertices[I1].Position;
        const FVector& C = Asset->Vertices[I2].Position;
        const FVector CurrentTriangleNormal = FVector::CrossProduct(B - A, C - A).GetSafeNormal();

        if (CurrentTriangleNormal.IsNearlyZero())
        {
            return false;
        }

        const FVector ProjectedTarget = FVector::PointPlaneProject(
            TargetPoint.LocalPosition,
            FPlane(A, CurrentTriangleNormal));

        FVector CurrentBarycentric;
        FVector TargetBarycentric;
        if (!FMath::ComputeBarycentricTri(
                CurrentPoint.LocalPosition,
                A, B, C,
                CurrentBarycentric) ||
            !FMath::ComputeBarycentricTri(
                ProjectedTarget,
                A, B, C,
                TargetBarycentric))
        {
            return false;
        }

        int32 OppositeVertexSlot = INDEX_NONE;
        for (int32 VertexSlot = 0; VertexSlot < 3; ++VertexSlot)
        {
            const int32 VertexIndex = CurrentTriangle.VertexIndices[VertexSlot];
            if (VertexIndex != SharedVertexIndexA && VertexIndex != SharedVertexIndexB)
            {
                OppositeVertexSlot = VertexSlot;
                break;
            }
        }

        if (OppositeVertexSlot == INDEX_NONE)
        {
            return false;
        }

        const float StartWeight = CurrentBarycentric[OppositeVertexSlot];
        const float TargetWeight = TargetBarycentric[OppositeVertexSlot];
        const float Denominator = StartWeight - TargetWeight;

        if (FMath::Abs(Denominator) <= KINDA_SMALL_NUMBER)
        {
            return false;
        }

        const float Alpha = StartWeight / Denominator;
        if (Alpha < -KINDA_SMALL_NUMBER || Alpha > 1.0f + KINDA_SMALL_NUMBER)
        {
            return false;
        }

        const FVector Intersection = FMath::Lerp(
            CurrentPoint.LocalPosition,
            ProjectedTarget,
            FMath::Clamp(Alpha, 0.0f, 1.0f));

        const FACFTriangle& DestinationTriangle = Asset->Triangles[DestinationTriangleIndex];
        const int32 D0 = DestinationTriangle.VertexIndices[0];
        const int32 D1 = DestinationTriangle.VertexIndices[1];
        const int32 D2 = DestinationTriangle.VertexIndices[2];

        if (!Asset->Vertices.IsValidIndex(D0) ||
            !Asset->Vertices.IsValidIndex(D1) ||
            !Asset->Vertices.IsValidIndex(D2))
        {
            return false;
        }

        FVector DestinationBarycentric;
        if (!FMath::ComputeBarycentricTri(
            Intersection,
            Asset->Vertices[D0].Position,
            Asset->Vertices[D1].Position,
            Asset->Vertices[D2].Position,
            DestinationBarycentric))
        {
            return false;
        }

        if (DestinationBarycentric.X < -KINDA_SMALL_NUMBER ||
            DestinationBarycentric.Y < -KINDA_SMALL_NUMBER ||
            DestinationBarycentric.Z < -KINDA_SMALL_NUMBER)
        {
            return false;
        }

        OutPoint = {};
        OutPoint.TriangleIndex = DestinationTriangleIndex;
        OutPoint.Barycentric = DestinationBarycentric;
        OutPoint.LocalPosition = Intersection;
        return true;
    }

    bool MakeSharedEdgeMidpoint(
        const UACFSurfaceAsset* Asset,
        int32 DestinationTriangleIndex,
        int32 SharedVertexIndexA,
        int32 SharedVertexIndexB,
        FACFBridgePoint& OutPoint)
    {
        if (!Asset || !Asset->Triangles.IsValidIndex(DestinationTriangleIndex) ||
            !Asset->Vertices.IsValidIndex(SharedVertexIndexA) ||
            !Asset->Vertices.IsValidIndex(SharedVertexIndexB))
        {
            return false;
        }

        const FACFTriangle& Triangle = Asset->Triangles[DestinationTriangleIndex];
        OutPoint = {};
        OutPoint.TriangleIndex = DestinationTriangleIndex;
        OutPoint.LocalPosition = (Asset->Vertices[SharedVertexIndexA].Position +
            Asset->Vertices[SharedVertexIndexB].Position) * 0.5f;

        for (int32 VertexSlot = 0; VertexSlot < 3; ++VertexSlot)
        {
            if (Triangle.VertexIndices[VertexSlot] == SharedVertexIndexA ||
                Triangle.VertexIndices[VertexSlot] == SharedVertexIndexB)
            {
                OutPoint.Barycentric[VertexSlot] = 0.5f;
            }
        }

        return !OutPoint.Barycentric.IsNearlyZero();
    }

    void AddPointIfDistinct(
        TArray<FACFBridgePoint>& InOutPoints,
        const FACFBridgePoint& Point)
    {
        if (InOutPoints.IsEmpty() ||
            !InOutPoints.Last().LocalPosition.Equals(Point.LocalPosition, KINDA_SMALL_NUMBER))
        {
            InOutPoints.Add(Point);
        }
    }
}

bool FACFBridgePathExpansion::BuildBaseSurfaceTopology(
    const UACFSurfaceAsset* Asset,
    FACFBridgePathTopology& OutTopology,
    FString* OutError)
{
    OutTopology.TriangleNeighbors.Reset();

    if (!Asset)
    {
        if (OutError) *OutError = TEXT("Surface asset is missing.");
        return false;
    }

    const int32 BaseTriangleCount = Asset->BaseSurfaceTriangleCount > 0
        ? FMath::Min(Asset->BaseSurfaceTriangleCount, Asset->Triangles.Num())
        : Asset->Triangles.Num();

    if (BaseTriangleCount <= 0)
    {
        if (OutError) *OutError = TEXT("Base surface has no triangles.");
        return false;
    }

    OutTopology.TriangleNeighbors.SetNum(BaseTriangleCount);

    TMap<FIntPoint, TArray<FTriangleEdgeUse>> EdgeUses;
    EdgeUses.Reserve(BaseTriangleCount * 3);

    for (int32 TriangleIndex = 0; TriangleIndex < BaseTriangleCount; ++TriangleIndex)
    {
        const FACFTriangle& Triangle = Asset->Triangles[TriangleIndex];

        for (int32 EdgeSlot = 0; EdgeSlot < 3; ++EdgeSlot)
        {
            const int32 VertexIndexA = Triangle.VertexIndices[EdgeSlot];
            const int32 VertexIndexB = Triangle.VertexIndices[(EdgeSlot + 1) % 3];

            if (!Asset->Vertices.IsValidIndex(VertexIndexA) ||
                !Asset->Vertices.IsValidIndex(VertexIndexB))
            {
                if (OutError) *OutError = TEXT("Base surface contains an invalid triangle vertex.");
                return false;
            }

            const FIntPoint EdgeKey(
                FMath::Min(VertexIndexA, VertexIndexB),
                FMath::Max(VertexIndexA, VertexIndexB));
            EdgeUses.FindOrAdd(EdgeKey).Add({ TriangleIndex, VertexIndexA, VertexIndexB });
        }
    }

    for (const TPair<FIntPoint, TArray<FTriangleEdgeUse>>& Pair : EdgeUses)
    {
        const TArray<FTriangleEdgeUse>& Uses = Pair.Value;
        if (Uses.Num() != 2)
        {
            continue;
        }

        const FTriangleEdgeUse& First = Uses[0];
        const FTriangleEdgeUse& Second = Uses[1];
        OutTopology.TriangleNeighbors[First.TriangleIndex].Add(
            { Second.TriangleIndex, Pair.Key.X, Pair.Key.Y });
        OutTopology.TriangleNeighbors[Second.TriangleIndex].Add(
            { First.TriangleIndex, Pair.Key.X, Pair.Key.Y });
    }

    return true;
}

bool FACFBridgePathExpansion::ExpandPath(
    const UACFSurfaceAsset* Asset,
    const FACFBridgePathTopology& Topology,
    const TArray<FACFBridgePoint>& SourcePoints,
    bool bClosedLoop,
    TArray<FACFBridgePoint>& OutExpandedPoints,
    FString* OutError)
{
    OutExpandedPoints.Reset();

    const int32 MinimumPointCount = bClosedLoop ? 3 : 2;
    if (!Asset || SourcePoints.Num() < MinimumPointCount)
    {
        if (OutError) *OutError = TEXT("Bridge path has too few points.");
        return false;
    }

    for (const FACFBridgePoint& Point : SourcePoints)
    {
        if (!Topology.TriangleNeighbors.IsValidIndex(Point.TriangleIndex))
        {
            if (OutError) *OutError = TEXT("Bridge point does not belong to the base surface.");
            return false;
        }
    }

    AddPointIfDistinct(OutExpandedPoints, SourcePoints[0]);

    const int32 SegmentCount = bClosedLoop
        ? SourcePoints.Num()
        : SourcePoints.Num() - 1;

    for (int32 SegmentIndex = 0; SegmentIndex < SegmentCount; ++SegmentIndex)
    {
        const int32 NextPointIndex = (SegmentIndex + 1) % SourcePoints.Num();
        const FACFBridgePoint& StartPoint = SourcePoints[SegmentIndex];
        const FACFBridgePoint& EndPoint = SourcePoints[NextPointIndex];

        TArray<int32> TrianglePath;
        if (!FindTrianglePath(
            Topology,
            StartPoint.TriangleIndex,
            EndPoint.TriangleIndex,
            TrianglePath))
        {
            if (OutError) *OutError = TEXT("Bridge path points must belong to one connected base-surface island.");
            return false;
        }

        FACFBridgePoint CurrentPoint = StartPoint;

        for (int32 PathIndex = 0; PathIndex + 1 < TrianglePath.Num(); ++PathIndex)
        {
            const int32 CurrentTriangleIndex = TrianglePath[PathIndex];
            const int32 NextTriangleIndex = TrianglePath[PathIndex + 1];

            const FACFBridgeTriangleNeighbor* Neighbor =
                Topology.TriangleNeighbors[CurrentTriangleIndex].FindByPredicate(
                    [NextTriangleIndex](const FACFBridgeTriangleNeighbor& Item)
                    {
                        return Item.TriangleIndex == NextTriangleIndex;
                    });

            if (!Neighbor)
            {
                if (OutError) *OutError = TEXT("Failed to resolve bridge path shared edge.");
                return false;
            }

            FACFBridgePoint EdgePoint;
            const bool bHasProjectedIntersection =
                TryMakeSharedEdgeProjectedPoint(
                    Asset,
                    CurrentPoint,
                    EndPoint,
                    CurrentTriangleIndex,
                    NextTriangleIndex,
                    Neighbor->SharedVertexIndexA,
                    Neighbor->SharedVertexIndexB,
                    EdgePoint);

            if (!bHasProjectedIntersection &&
                !MakeSharedEdgeMidpoint(
                    Asset,
                    NextTriangleIndex,
                    Neighbor->SharedVertexIndexA,
                    Neighbor->SharedVertexIndexB,
                    EdgePoint))
            {
                if (OutError) *OutError = TEXT("Failed to create bridge path shared-edge point.");
                return false;
            }

            AddPointIfDistinct(OutExpandedPoints, EdgePoint);
            CurrentPoint = EdgePoint;
        }

        if (!bClosedLoop || SegmentIndex != SegmentCount - 1)
        {
            AddPointIfDistinct(OutExpandedPoints, EndPoint);
        }
    }

    return OutExpandedPoints.Num() >= MinimumPointCount;
}
