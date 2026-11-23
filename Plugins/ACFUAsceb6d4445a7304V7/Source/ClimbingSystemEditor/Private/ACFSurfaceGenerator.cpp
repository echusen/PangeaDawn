// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "ACFSurfaceGenerator.h"

#include "ACFBridgePathExpansion.h"
#include "ClimbingSystemEditor.h"
#include "ACFSurfaceAsset.h"
#include "ACFFreeClimbTypes.h"

#include "Rendering/SkeletalMeshRenderData.h"

#include "Interfaces/ITargetPlatform.h"
#include "Interfaces/ITargetPlatformManagerModule.h"
#include "SkeletalMeshReductionSettings.h"
#include "LODUtilities.h"
#include "ACFSurfaceBVHBuilder.h"

namespace
{
    class FDisjointSet
    {
    public:
        explicit FDisjointSet(int32 NumElements);
        int32 Find(int32 Index);
        void Union(int32 A, int32 B);

    private:
        TArray<int32> Parent;
        TArray<uint8> Rank;
    };

    FIntVector ToLogicalWeldCell(const FVector& Position, float CellSize);
    int32 GetDominantBoneIndex(const FACFTriangleVertex& Vertex);
    bool CanLogicalWeld(
        const FACFTriangleVertex& A,
        const FACFTriangleVertex& B,
        const FACFLogicalWeldSettings& Settings,
        float MinNormalDot);

    uint32 CalculateBaseSurfaceTopologyHash(const UACFSurfaceAsset* InAsset);
    bool AppendBridgeGeometry(UACFSurfaceAsset* InAsset, const FACFBridgeDefinition& Definition);
    bool ResolveBridgePoint(
        const UACFSurfaceAsset* InAsset,
        const FACFBridgePoint& Point,
        FACFTriangleVertex& OutVertex);
    bool ResampleBridgePath(
        const TArray<FACFTriangleVertex>& SourceVertices,
        bool bClosedLoop,
        int32 TargetPointCount,
        TArray<FACFTriangleVertex>& OutVertices);
    bool AlignSecondBridgePath(
        const TArray<FACFTriangleVertex>& FirstPath,
        const TArray<FACFTriangleVertex>& SecondPath,
        bool bClosedLoop,
        TArray<FACFTriangleVertex>& OutAlignedSecondPath);
    bool BlendBridgeVertices(
        const FACFTriangleVertex& A,
        const FACFTriangleVertex& B,
        double Alpha,
        FACFTriangleVertex& OutVertex);

    using FACFBoneWeightMap = TMap<int32, float>;

    void AccumulateBoneInfluences(
        const FACFTriangleVertex& SourceVertex,
        float Scale,
        FACFBoneWeightMap& InOutWeights);
    bool SetNormalizedBoneInfluences(
        const FACFBoneWeightMap& Weights,
        int32 MaxInfluenceCount,
        FACFTriangleVertex& OutVertex);
    bool BuildBlendedBoneInfluences(
        const FACFTriangleVertex& V0,
        float Weight0,
        const FACFTriangleVertex& V1,
        float Weight1,
        const FACFTriangleVertex& V2,
        float Weight2,
        FACFTriangleVertex& OutVertex);
}

bool FACFSurfaceGenerator::Generate(UACFSurfaceAsset* InAsset)
{
    if (!InAsset || !InAsset->SourceMesh)
    {
        return false;
    }

    USkeletalMesh* TransientMesh = CopyTransientMesh(InAsset->SourceMesh);
    if (!TransientMesh)
    {
        return false;
    }

    TransientMesh = GenerateLODMesh(TransientMesh, InAsset->ReductionSettings);
    if (!TransientMesh)
    {
        return false;
    }

    if (!CreateSurfaceAsset(InAsset, TransientMesh))
    {
        return false;
    }

    TArray<int32> VertexToLogicalGroup;
    TArray<TArray<int32>> LogicalGroups;
    BuildLogicalVertexGroups(InAsset, InAsset->LogicalWeldSettings, VertexToLogicalGroup, LogicalGroups);

    TArray<TArray<int32>> VertexNeighbors;
    BuildVertexNeighbors(InAsset, VertexNeighbors);
    AddLogicalGroupNeighbors(LogicalGroups, VertexNeighbors);
    FlatVertexNormals(InAsset, InAsset->NormalFlattenSettings, VertexNeighbors);

    InAsset->BaseSurfaceVertexCount = InAsset->Vertices.Num();
    InAsset->BaseSurfaceTriangleCount = InAsset->Triangles.Num();
    InAsset->BaseSurfaceTopologyHash = CalculateBaseSurfaceTopologyHash(InAsset);

    if (!RebuildBridgeOverlay(InAsset))
    {
        UE_LOG(LogACFFreeClimbEditor, Warning,
            TEXT("Failed to rebuild bridge overlay for %s"),
            *InAsset->GetName());
    }

    return true;
}

bool FACFSurfaceGenerator::RebuildBridgeOverlay(
    UACFSurfaceAsset* InAsset,
    FString* OutError)
{
    if (!InAsset)
    {
        if (OutError)
        {
            *OutError = TEXT("Surface asset is null.");
        }
        return false;
    }

    if (InAsset->BaseSurfaceVertexCount <= 0 ||
        InAsset->BaseSurfaceTriangleCount <= 0)
    {
        if (OutError)
        {
            *OutError = TEXT("Base surface has not been generated.");
        }
        return false;
    }

    if (InAsset->Vertices.Num() < InAsset->BaseSurfaceVertexCount ||
        InAsset->Triangles.Num() < InAsset->BaseSurfaceTriangleCount)
    {
        return false;
    }

    InAsset->Vertices.SetNum(InAsset->BaseSurfaceVertexCount);
    InAsset->Triangles.SetNum(InAsset->BaseSurfaceTriangleCount);

    for (const FACFBridgeDefinition& Definition : InAsset->BridgeDefinitions)
    {
        if (Definition.BaseSurfaceTopologyHash != InAsset->BaseSurfaceTopologyHash)
        {
            continue;
        }

        if (!AppendBridgeGeometry(InAsset, Definition))
        {
            continue;
        }
    }

    InAsset->RaycastData.Reset();

    if (InAsset->bBuildRaycastBVH &&
        !FACFSurfaceBVHBuilder::Build(InAsset))
    {
        if (OutError)
        {
            *OutError = TEXT("Failed to build raycast BVH.");
        }

        InAsset->RaycastData.Reset();
        return false;
    }

    return true;
}

USkeletalMesh* FACFSurfaceGenerator::CopyTransientMesh(USkeletalMesh* SourceMesh)
{
    USkeletalMesh* TransientMesh = DuplicateObject<USkeletalMesh>(
        SourceMesh,
        GetTransientPackage()
    );
    if (!TransientMesh) return nullptr;

    TransientMesh->SetFlags(RF_Transient);

    return TransientMesh;
}

USkeletalMesh* FACFSurfaceGenerator::GenerateLODMesh(
    USkeletalMesh* Mesh,
    const FACFReductionSettings& Settings)
{
    while (Mesh->GetLODNum() < 2)
    {
        Mesh->AddLODInfo();
    }

    FSkeletalMeshLODInfo* LODInfo = Mesh->GetLODInfo(1);
    if (!LODInfo) return nullptr;

    FSkeletalMeshOptimizationSettings& OptSettings = LODInfo->ReductionSettings;
    OptSettings.BaseLOD = 0;
    OptSettings.TerminationCriterion = SkeletalMeshTerminationCriterion::SMTC_NumOfTriangles;
    OptSettings.NumOfTrianglesPercentage = Settings.NumOfTrianglesPercentage;
    OptSettings.VolumeImportance = Settings.VolumeImportance;
    OptSettings.bLockEdges = Settings.bLockEdges;
    OptSettings.WeldingThreshold = Settings.WeldingThreshold;

    OptSettings.bEnforceBoneBoundaries = true;
    OptSettings.bMergeCoincidentVertBones = true;
    OptSettings.SkinningImportance = SMOI_Highest;
    OptSettings.SilhouetteImportance = SkeletalMeshOptimizationImportance::SMOI_Highest;
    OptSettings.TextureImportance = SMOI_Lowest;
    OptSettings.ShadingImportance = SMOI_Lowest;
    OptSettings.bRecalcNormals = false;
    OptSettings.ReductionMethod = SkeletalMeshOptimizationType::SMOT_MaxDeviation;

    const ITargetPlatform* TargetPlatform = GetTargetPlatformManagerRef().GetRunningTargetPlatform();
    if (!TargetPlatform) return nullptr;

    if (!FLODUtilities::RegenerateLOD(Mesh, TargetPlatform, 2, true)) return nullptr;

    return Mesh;
}

bool FACFSurfaceGenerator::CreateSurfaceAsset(UACFSurfaceAsset* InAsset, USkeletalMesh* InMesh)
{
    if (!InAsset || !InMesh) return false;

    FSkeletalMeshRenderData* RenderData = InMesh->GetResourceForRendering();
    if (!RenderData || !RenderData->LODRenderData.IsValidIndex(1)) return false;

    FSkeletalMeshLODRenderData& LODData = RenderData->LODRenderData[1];

    const FPositionVertexBuffer& PositionBuffer = LODData.StaticVertexBuffers.PositionVertexBuffer;
    const FStaticMeshVertexBuffer& VertexBuffer = LODData.StaticVertexBuffers.StaticMeshVertexBuffer;
    const FRawStaticIndexBuffer16or32Interface* IndexBuffer = LODData.MultiSizeIndexContainer.GetIndexBuffer();
    const FSkinWeightVertexBuffer& SkinWeightBuffer = LODData.SkinWeightVertexBuffer;

    if (!IndexBuffer) return false;

    InAsset->Triangles.Reset();
    InAsset->Vertices.Reset();
    InAsset->RaycastData.Reset();

    const int32 NumSourceVertices = PositionBuffer.GetNumVertices();
    const int32 NumInfluences = SkinWeightBuffer.GetMaxBoneInfluences();
    const float WeightScale = static_cast<float>(MAX_uint16);

    TArray<int32> SourceToSurfaceVertexIndex;
    SourceToSurfaceVertexIndex.Init(INDEX_NONE, NumSourceVertices);

    TSet<FName> ExcludedMaterialSlots;
    ExcludedMaterialSlots.Reserve(InAsset->ExcludedMaterialSlotNames.Num());
    for (const FName MaterialSlotName : InAsset->ExcludedMaterialSlotNames)
    {
        if (!MaterialSlotName.IsNone())
        {
            ExcludedMaterialSlots.Add(MaterialSlotName);
        }
    }

    const TArray<FSkeletalMaterial>& Materials = InMesh->GetMaterials();

    for (int32 SectionIndex = 0; SectionIndex < LODData.RenderSections.Num(); ++SectionIndex)
    {
        const FSkelMeshRenderSection& Section = LODData.RenderSections[SectionIndex];

        if (Materials.IsValidIndex(Section.MaterialIndex) &&
            ExcludedMaterialSlots.Contains(Materials[Section.MaterialIndex].MaterialSlotName))
        {
            continue;
        }

        for (uint32 TriIndex = 0; TriIndex < Section.NumTriangles; ++TriIndex)
        {
            const uint32 IndexOffset = Section.BaseIndex + TriIndex * 3;

            FACFTriangle Triangle;

            for (int32 Corner = 0; Corner < 3; ++Corner)
            {
                const int32 SourceVertexIndex = IndexBuffer->Get(IndexOffset + Corner);

                if (!SourceToSurfaceVertexIndex.IsValidIndex(SourceVertexIndex)) return false;

                int32& SurfaceVertexIndex = SourceToSurfaceVertexIndex[SourceVertexIndex];
                if (SurfaceVertexIndex == INDEX_NONE)
                {
                    FACFTriangleVertex Vertex;
                    Vertex.Position = FVector(PositionBuffer.VertexPosition(SourceVertexIndex));
                    Vertex.Normal = FVector(VertexBuffer.VertexTangentZ(SourceVertexIndex));

                    Vertex.BoneIndices.SetNum(NumInfluences);
                    Vertex.BoneWeights.SetNum(NumInfluences);

                    for (int32 InfluenceIndex = 0; InfluenceIndex < NumInfluences; ++InfluenceIndex)
                    {
                        const uint16 RawWeight = SkinWeightBuffer.GetBoneWeight(SourceVertexIndex, InfluenceIndex);

                        if (RawWeight == 0)
                        {
                            Vertex.BoneIndices[InfluenceIndex] = INDEX_NONE;
                            Vertex.BoneWeights[InfluenceIndex] = 0.0f;
                            continue;
                        }

                        const uint16 SectionBoneIndex = SkinWeightBuffer.GetBoneIndex(SourceVertexIndex, InfluenceIndex);
                        if (!Section.BoneMap.IsValidIndex(SectionBoneIndex))
                        {
                            Vertex.BoneIndices[InfluenceIndex] = INDEX_NONE;
                            Vertex.BoneWeights[InfluenceIndex] = 0.0f;
                            continue;
                        }

                        Vertex.BoneIndices[InfluenceIndex] = Section.BoneMap[SectionBoneIndex];
                        Vertex.BoneWeights[InfluenceIndex] = RawWeight / WeightScale;
                    }

                    SurfaceVertexIndex = InAsset->Vertices.Add(MoveTemp(Vertex));
                }

                Triangle.VertexIndices[Corner] = SurfaceVertexIndex;
            }

            const int32 I0 = Triangle.VertexIndices[0];
            const int32 I1 = Triangle.VertexIndices[1];
            const int32 I2 = Triangle.VertexIndices[2];

            const FVector& P0 = InAsset->Vertices[I0].Position;
            const FVector& P1 = InAsset->Vertices[I1].Position;
            const FVector& P2 = InAsset->Vertices[I2].Position;

            Triangle.Normal = FVector::CrossProduct(P2 - P0, P1 - P0).GetSafeNormal();

            InAsset->Triangles.Add(Triangle);
        }
    }

    return true;
}

void FACFSurfaceGenerator::BuildLogicalVertexGroups(
    const UACFSurfaceAsset* InAsset,
    const FACFLogicalWeldSettings& Settings,
    TArray<int32>& OutVertexToLogicalGroup,
    TArray<TArray<int32>>& OutLogicalGroups)
{
    OutVertexToLogicalGroup.Reset();
    OutLogicalGroups.Reset();

    if (!InAsset || !Settings.bEnabled ||
        Settings.WeldDistance <= KINDA_SMALL_NUMBER)
    {
        return;
    }

    const int32 NumVertices = InAsset->Vertices.Num();

    OutVertexToLogicalGroup.Init(INDEX_NONE, NumVertices);
    if (NumVertices == 0)
    {
        return;
    }

    TBitArray<> bReferencedVertices(false, NumVertices);

    for (const FACFTriangle& Triangle : InAsset->Triangles)
    {
        for (int32 Corner = 0; Corner < 3; ++Corner)
        {
            const int32 VertexIndex = Triangle.VertexIndices[Corner];

            if (bReferencedVertices.IsValidIndex(VertexIndex))
            {
                bReferencedVertices[VertexIndex] = true;
            }
        }
    }

    const float MinNormalDot = FMath::Cos(FMath::DegreesToRadians(Settings.MaxNormalAngleDegrees));

    FDisjointSet DisjointSet(NumVertices);

    TMap<FIntVector, TArray<int32>> SpatialHash;

    for (int32 VertexIndex = 0; VertexIndex < NumVertices; ++VertexIndex)
    {
        if (!bReferencedVertices[VertexIndex])
        {
            continue;
        }

        const FACFTriangleVertex& Vertex = InAsset->Vertices[VertexIndex];

        const FIntVector Cell = ToLogicalWeldCell(Vertex.Position, Settings.WeldDistance);

        for (int32 OffsetX = -1; OffsetX <= 1; ++OffsetX)
        {
            for (int32 OffsetY = -1; OffsetY <= 1; ++OffsetY)
            {
                for (int32 OffsetZ = -1; OffsetZ <= 1; ++OffsetZ)
                {
                    const FIntVector NeighborCell = Cell + FIntVector(OffsetX, OffsetY, OffsetZ);

                    const TArray<int32>* ExistingVertices = SpatialHash.Find(NeighborCell);

                    if (!ExistingVertices)
                    {
                        continue;
                    }

                    for (const int32 ExistingIndex : *ExistingVertices)
                    {
                        const FACFTriangleVertex& ExistingVertex = InAsset->Vertices[ExistingIndex];

                        if (CanLogicalWeld(Vertex, ExistingVertex, Settings, MinNormalDot))
                        {
                            DisjointSet.Union(VertexIndex, ExistingIndex);
                        }
                    }
                }
            }
        }

        SpatialHash.FindOrAdd(Cell).Add(VertexIndex);
    }

    TMap<int32, int32> RootToGroupIndex;

    for (int32 VertexIndex = 0; VertexIndex < NumVertices; ++VertexIndex)
    {
        if (!bReferencedVertices[VertexIndex])
        {
            continue;
        }

        const int32 Root = DisjointSet.Find(VertexIndex);

        int32* ExistingGroupIndex = RootToGroupIndex.Find(Root);
        if (!ExistingGroupIndex)
        {
            const int32 NewGroupIndex = OutLogicalGroups.AddDefaulted();

            RootToGroupIndex.Add(Root, NewGroupIndex);
            ExistingGroupIndex = RootToGroupIndex.Find(Root);
        }

        OutVertexToLogicalGroup[VertexIndex] = *ExistingGroupIndex;
        OutLogicalGroups[*ExistingGroupIndex].Add(VertexIndex);
    }
}

void FACFSurfaceGenerator::AddLogicalGroupNeighbors(
    const TArray<TArray<int32>>& LogicalGroups,
    TArray<TArray<int32>>& InOutNeighbors)
{
    for (const TArray<int32>& Group : LogicalGroups)
    {
        if (Group.Num() < 2)
        {
            continue;
        }

        for (int32 I = 0; I < Group.Num(); ++I)
        {
            const int32 VertexA = Group[I];

            if (!InOutNeighbors.IsValidIndex(VertexA))
            {
                continue;
            }

            for (int32 J = I + 1; J < Group.Num(); ++J)
            {
                const int32 VertexB = Group[J];

                if (!InOutNeighbors.IsValidIndex(VertexB))
                {
                    continue;
                }

                InOutNeighbors[VertexA].AddUnique(VertexB);
                InOutNeighbors[VertexB].AddUnique(VertexA);
            }
        }
    }
}

void FACFSurfaceGenerator::BuildVertexNeighbors(
    const UACFSurfaceAsset* InAsset,
    TArray<TArray<int32>>& OutNeighbors)
{
    OutNeighbors.SetNum(InAsset->Vertices.Num());

    auto AddNeighbor = [&OutNeighbors](int32 A, int32 B)
    {
        if (A == B) return;
        if (!OutNeighbors.IsValidIndex(A)) return;
        if (!OutNeighbors.IsValidIndex(B)) return;

        OutNeighbors[A].AddUnique(B);
    };

    for (const FACFTriangle& Triangle : InAsset->Triangles)
    {
        const int32 I0 = Triangle.VertexIndices[0];
        const int32 I1 = Triangle.VertexIndices[1];
        const int32 I2 = Triangle.VertexIndices[2];

        AddNeighbor(I0, I1);
        AddNeighbor(I0, I2);

        AddNeighbor(I1, I0);
        AddNeighbor(I1, I2);

        AddNeighbor(I2, I0);
        AddNeighbor(I2, I1);
    }
}

void FACFSurfaceGenerator::FlatVertexNormals(
    UACFSurfaceAsset* InAsset,
    const FACFNormalFlattenSettings& Settings,
    const TArray<TArray<int32>>& VertexNeighbors)
{
    if (!InAsset) return;

    const float MinDot = FMath::Cos(FMath::DegreesToRadians(Settings.MaxAngleDegrees));

    TArray<FVector> OriginalNormals;
    OriginalNormals.SetNum(InAsset->Vertices.Num());

    for (int32 Iter = 0; Iter < Settings.Iteration; ++Iter)
    {
        for (int32 Index = 0; Index < InAsset->Vertices.Num(); ++Index)
        {
            OriginalNormals[Index] = InAsset->Vertices[Index].Normal.GetSafeNormal();
        }

        for (int32 VertexIndex = 0; VertexIndex < InAsset->Vertices.Num(); ++VertexIndex)
        {
            const FVector Original = OriginalNormals[VertexIndex];

            FVector Sum = Original;
            int32 Count = 1;

            for (const int32 NeighborIndex : VertexNeighbors[VertexIndex])
            {
                if (!OriginalNormals.IsValidIndex(NeighborIndex)) continue;

                const FVector NeighborNormal = OriginalNormals[NeighborIndex];
                if (FVector::DotProduct(Original, NeighborNormal) < MinDot) continue;

                Sum += NeighborNormal;
                ++Count;
            }

            const FVector Average = (Sum / Count).GetSafeNormal();
            const FVector Smoothed = FMath::Lerp(Original, Average, Settings.Strength).GetSafeNormal();

            InAsset->Vertices[VertexIndex].Normal = Smoothed;
        }
    }
}

namespace
{
    FDisjointSet::FDisjointSet(int32 NumElements)
    {
        Parent.SetNum(NumElements);
        Rank.Init(0, NumElements);

        for (int32 Index = 0; Index < NumElements; ++Index)
        {
            Parent[Index] = Index;
        }
    }

    int32 FDisjointSet::Find(int32 Index)
    {
        int32 Root = Index;
        while (Parent[Root] != Root)
        {
            Root = Parent[Root];
        }

        while (Parent[Index] != Index)
        {
            const int32 Next = Parent[Index];
            Parent[Index] = Root;
            Index = Next;
        }

        return Root;
    }

    void FDisjointSet::Union(int32 A, int32 B)
    {
        int32 RootA = Find(A);
        int32 RootB = Find(B);
        if (RootA == RootB)
        {
            return;
        }

        if (Rank[RootA] < Rank[RootB])
        {
            Swap(RootA, RootB);
        }

        Parent[RootB] = RootA;

        if (Rank[RootA] == Rank[RootB])
        {
            ++Rank[RootB];
        }
    }

    FIntVector ToLogicalWeldCell(const FVector& Position, float CellSize)
    {
        return FIntVector(
            FMath::FloorToInt(Position.X / CellSize),
            FMath::FloorToInt(Position.Y / CellSize),
            FMath::FloorToInt(Position.Z / CellSize));
    }

    int32 GetDominantBoneIndex(const FACFTriangleVertex& Vertex)
    {
        int32 DominantBoneIndex = INDEX_NONE;
        float DominantWeight = 0.0f;

        const int32 NumInfluences = FMath::Min(Vertex.BoneIndices.Num(), Vertex.BoneWeights.Num());

        for (int32 InfluenceIndex = 0; InfluenceIndex < NumInfluences; ++InfluenceIndex)
        {
            const int32 BoneIndex = Vertex.BoneIndices[InfluenceIndex];
            const float Weight = Vertex.BoneWeights[InfluenceIndex];

            if (BoneIndex == INDEX_NONE || Weight <= DominantWeight)
            {
                continue;
            }

            DominantBoneIndex = BoneIndex;
            DominantWeight = Weight;
        }
    
        return DominantBoneIndex;
    }

    bool CanLogicalWeld(
        const FACFTriangleVertex& A,
        const FACFTriangleVertex& B,
        const FACFLogicalWeldSettings& Settings,
        float MinNormalDot)
    {
        if (FVector::DistSquared(A.Position, B.Position) > FMath::Square(Settings.WeldDistance))
        {
            return false;
        }

        const FVector NormalA = A.Normal.GetSafeNormal();
        const FVector NormalB = B.Normal.GetSafeNormal();

        if (NormalA.IsNearlyZero() || NormalB.IsNearlyZero())
        {
            return false;
        }

        if (FVector::DotProduct(NormalA, NormalB) < MinNormalDot)
        {
            return false;
        }

        if (Settings.bRequireSameDominantBone &&
            GetDominantBoneIndex(A) != GetDominantBoneIndex(B))
        {
            return false;
        }

        return true;
    }

    uint32 CalculateBaseSurfaceTopologyHash(const UACFSurfaceAsset* InAsset)
    {
        if (!InAsset)
        {
            return 0;
        }

        uint32 Hash = 0;

        const int32 VertexCount = InAsset->BaseSurfaceVertexCount > 0
            ? InAsset->BaseSurfaceVertexCount
            : InAsset->Vertices.Num();

        const int32 TriangleCount = InAsset->BaseSurfaceTriangleCount > 0
            ? InAsset->BaseSurfaceTriangleCount
            : InAsset->Triangles.Num();

        Hash = HashCombine(Hash, GetTypeHash(VertexCount));
        Hash = HashCombine(Hash, GetTypeHash(TriangleCount));

        for (int32 TriangleIndex = 0; TriangleIndex < TriangleCount; ++TriangleIndex)
        {
            const FACFTriangle& Triangle = InAsset->Triangles[TriangleIndex];

            Hash = HashCombine(Hash, GetTypeHash(Triangle.VertexIndices[0]));
            Hash = HashCombine(Hash, GetTypeHash(Triangle.VertexIndices[1]));
            Hash = HashCombine(Hash, GetTypeHash(Triangle.VertexIndices[2]));
        }

        return Hash;
    }

    bool AppendBridgeGeometry(UACFSurfaceAsset* InAsset, const FACFBridgeDefinition& Definition)
    {
        if (!InAsset)
        {
            return false;
        }

        const bool bClosedLoop = Definition.PathType == EACFBridgePathType::ClosedLoop;

        const int32 MinimumPointCount = bClosedLoop ? 3 : 2;

        if (Definition.FirstPathPoints.Num() < MinimumPointCount ||
            Definition.SecondPathPoints.Num() < MinimumPointCount)
        {
            return false;
        }

        FACFBridgePathTopology Topology;
        TArray<FACFBridgePoint> ExpandedFirstPathPoints;
        TArray<FACFBridgePoint> ExpandedSecondPathPoints;

        if (!FACFBridgePathExpansion::BuildBaseSurfaceTopology(InAsset, Topology) ||
            !FACFBridgePathExpansion::ExpandPath(
                InAsset, Topology, Definition.FirstPathPoints, bClosedLoop,
                ExpandedFirstPathPoints) ||
            !FACFBridgePathExpansion::ExpandPath(
                InAsset, Topology, Definition.SecondPathPoints, bClosedLoop,
                ExpandedSecondPathPoints))
        {
            return false;
        }

        TArray<FACFTriangleVertex> FirstSourceVertices;
        TArray<FACFTriangleVertex> SecondSourceVertices;

        FirstSourceVertices.Reserve(ExpandedFirstPathPoints.Num());
        SecondSourceVertices.Reserve(ExpandedSecondPathPoints.Num());

        for (const FACFBridgePoint& Point : ExpandedFirstPathPoints)
        {
            FACFTriangleVertex Vertex;

            if (!ResolveBridgePoint(InAsset, Point, Vertex))
            {
                return false;
            }

            FirstSourceVertices.Add(MoveTemp(Vertex));
        }

        for (const FACFBridgePoint& Point : ExpandedSecondPathPoints)
        {
            FACFTriangleVertex Vertex;

            if (!ResolveBridgePoint(InAsset, Point, Vertex))
            {
                return false;
            }

            SecondSourceVertices.Add(MoveTemp(Vertex));
        }

        const int32 TargetPointCount = FMath::Max(
            FirstSourceVertices.Num(),
            SecondSourceVertices.Num());

        TArray<FACFTriangleVertex> FirstPath;
        TArray<FACFTriangleVertex> SecondPath;
        TArray<FACFTriangleVertex> AlignedSecondPath;

        if (!ResampleBridgePath(
                FirstSourceVertices,
                bClosedLoop,
                TargetPointCount,
                FirstPath) ||
            !ResampleBridgePath(
                SecondSourceVertices,
                bClosedLoop,
                TargetPointCount,
                SecondPath) || 
            !AlignSecondBridgePath(
                FirstPath,
                SecondPath,
                bClosedLoop,
                AlignedSecondPath))
        {
            return false;
        }

        TArray<FACFTriangleVertex> NewVertices;
        TArray<FACFTriangle> NewTriangles;

        NewVertices.Reserve(TargetPointCount * 2);

        for (int32 Index = 0; Index < TargetPointCount; ++Index)
        {
            NewVertices.Add(FirstPath[Index]);
            NewVertices.Add(AlignedSecondPath[Index]);
        }

        const int32 VertexOffset = InAsset->Vertices.Num();

        auto AddTriangle = [&NewVertices, &NewTriangles, VertexOffset](
            int32 LocalI0,
            int32 LocalI1,
            int32 LocalI2)
        {
            const FVector& P0 = NewVertices[LocalI0].Position;
            const FVector& P1 = NewVertices[LocalI1].Position;
            const FVector& P2 = NewVertices[LocalI2].Position;

            const FVector Normal = FVector::CrossProduct(
                P2 - P0,
                P1 - P0).GetSafeNormal();

            if (Normal.IsNearlyZero())
            {
                return false;
            }

            FACFTriangle Triangle;
            Triangle.VertexIndices[0] = VertexOffset + LocalI0;
            Triangle.VertexIndices[1] = VertexOffset + LocalI1;
            Triangle.VertexIndices[2] = VertexOffset + LocalI2;
            Triangle.Normal = Normal;
            Triangle.QueryFlags = static_cast<uint8>(EACFSurfaceQueryFlags::ClimbingMove);

            NewTriangles.Add(MoveTemp(Triangle));
            return true;
        };

        const int32 SegmentCount = bClosedLoop ? TargetPointCount : TargetPointCount - 1;

        for (int32 SegmentIndex = 0; SegmentIndex < SegmentCount; ++SegmentIndex)
        {
            const int32 NextIndex = (SegmentIndex + 1) % TargetPointCount;

            const int32 A0 = SegmentIndex * 2;
            const int32 B0 = A0 + 1;
            const int32 A1 = NextIndex * 2;
            const int32 B1 = A1 + 1;

            const bool bAdded = Definition.bReverseWinding
                ? AddTriangle(A0, B0, A1) &&
                    AddTriangle(B0, B1, A1)
                : AddTriangle(A0, A1, B0) &&
                    AddTriangle(B0, A1, B1);

            if (!bAdded)
            {
                return false;
            }
        }

        if (NewTriangles.IsEmpty())
        {
            return false;
        }

        InAsset->Vertices.Append(MoveTemp(NewVertices));
        InAsset->Triangles.Append(MoveTemp(NewTriangles));

        return true;
    }

    bool ResolveBridgePoint(
        const UACFSurfaceAsset* InAsset,
        const FACFBridgePoint& Point,
        FACFTriangleVertex& OutVertex)
    {
        if (!InAsset ||
            Point.TriangleIndex < 0 ||
            Point.TriangleIndex >= InAsset->BaseSurfaceTriangleCount)
        {
            return false;
        }

        const FACFTriangle& Triangle = InAsset->Triangles[Point.TriangleIndex];

        const int32 I0 = Triangle.VertexIndices[0];
        const int32 I1 = Triangle.VertexIndices[1];
        const int32 I2 = Triangle.VertexIndices[2];

        if (!InAsset->Vertices.IsValidIndex(I0) ||
            !InAsset->Vertices.IsValidIndex(I1) ||
            !InAsset->Vertices.IsValidIndex(I2))
        {
            return false;
        }

        const FACFTriangleVertex& V0 = InAsset->Vertices[I0];
        const FACFTriangleVertex& V1 = InAsset->Vertices[I1];
        const FACFTriangleVertex& V2 = InAsset->Vertices[I2];

        FVector Barycentric = Point.Barycentric;

        if (Barycentric.X < -KINDA_SMALL_NUMBER ||
            Barycentric.Y < -KINDA_SMALL_NUMBER ||
            Barycentric.Z < -KINDA_SMALL_NUMBER)
        {
            return false;
        }

        const double WeightSum = Barycentric.X + Barycentric.Y + Barycentric.Z;

        if (WeightSum <= KINDA_SMALL_NUMBER)
        {
            return false;
        }

        Barycentric /= WeightSum;

        OutVertex.Position =
            V0.Position * Barycentric.X +
            V1.Position * Barycentric.Y +
            V2.Position * Barycentric.Z;
        OutVertex.Normal = (
            V0.Normal * Barycentric.X +
            V1.Normal * Barycentric.Y +
            V2.Normal * Barycentric.Z).GetSafeNormal();

        return BuildBlendedBoneInfluences(
            V0, Barycentric.X,
            V1, Barycentric.Y,
            V2, Barycentric.Z,
            OutVertex);
    }

    bool ResampleBridgePath(
        const TArray<FACFTriangleVertex>& SourceVertices,
        bool bClosedLoop,
        int32 TargetPointCount,
        TArray<FACFTriangleVertex>& OutVertices)
    {
        OutVertices.Reset();

        const int32 MinimumPointCount = bClosedLoop ? 3 : 2;

        if (SourceVertices.Num() < MinimumPointCount ||
            TargetPointCount < MinimumPointCount)
        {
            return false;
        }

        if (TargetPointCount < SourceVertices.Num())
        {
            return false;
        }

        const int32 SegmentCount = bClosedLoop
            ? SourceVertices.Num()
            : SourceVertices.Num() - 1;
        TArray<int32> SegmentSubdivisions;
        SegmentSubdivisions.Init(1, SegmentCount);

        for (int32 ExtraSampleCount = TargetPointCount - SourceVertices.Num();
            ExtraSampleCount > 0;
            --ExtraSampleCount)
        {
            int32 BestSegmentIndex = 0;
            double BestDensity = -1.0;

            for (int32 SegmentIndex = 0; SegmentIndex < SegmentCount; ++SegmentIndex)
            {
                const int32 NextIndex = (SegmentIndex + 1) % SourceVertices.Num();
                const double Density = FVector::Distance(
                    SourceVertices[SegmentIndex].Position,
                    SourceVertices[NextIndex].Position) /
                    SegmentSubdivisions[SegmentIndex];

                if (Density > BestDensity)
                {
                    BestDensity = Density;
                    BestSegmentIndex = SegmentIndex;
                }
            }

            ++SegmentSubdivisions[BestSegmentIndex];
        }

        OutVertices.Reserve(TargetPointCount);

        if (!bClosedLoop)
        {
            OutVertices.Add(SourceVertices[0]);
        }

        for (int32 SegmentIndex = 0; SegmentIndex < SegmentCount; ++SegmentIndex)
        {
            const int32 NextIndex = (SegmentIndex + 1) % SourceVertices.Num();

            if (bClosedLoop)
            {
                OutVertices.Add(SourceVertices[SegmentIndex]);
            }

            for (int32 SubdivisionIndex = 1;
                SubdivisionIndex < SegmentSubdivisions[SegmentIndex];
                ++SubdivisionIndex)
            {
                const double Alpha = static_cast<double>(SubdivisionIndex) /
                    SegmentSubdivisions[SegmentIndex];

                FACFTriangleVertex Sample;
                if (!BlendBridgeVertices(
                    SourceVertices[SegmentIndex],
                    SourceVertices[NextIndex],
                    Alpha,
                    Sample))
                {
                    return false;
                }

                OutVertices.Add(MoveTemp(Sample));
            }

            if (!bClosedLoop || SegmentIndex == SegmentCount - 1)
            {
                OutVertices.Add(SourceVertices[NextIndex]);
            }
        }

        return OutVertices.Num() == TargetPointCount;
    }

    bool AlignSecondBridgePath(
        const TArray<FACFTriangleVertex>& FirstPath,
        const TArray<FACFTriangleVertex>& SecondPath,
        bool bClosedLoop,
        TArray<FACFTriangleVertex>& OutAlignedSecondPath)
    {
        OutAlignedSecondPath.Reset();

        const int32 Count = FirstPath.Num();

        if (Count <= 0 || SecondPath.Num() != Count)
        {
            return false;
        }

        double BestCost = TNumericLimits<double>::Max();
        int32 BestOffset = 0;
        bool bBestReverse = false;

        const int32 OffsetCount = bClosedLoop ? Count : 1;

        for (int32 Offset = 0; Offset < OffsetCount; ++Offset)
        {
            for (bool bReverse : { false, true })
            {
                double Cost = 0.0;

                for (int32 Index = 0; Index < Count; ++Index)
                {
                    int32 SecondIndex = bReverse
                        ? Count - 1 - Index
                        : Index;

                    if (bClosedLoop)
                    {
                        SecondIndex = (SecondIndex + Offset) % Count;
                    }

                    Cost += FVector::DistSquared(
                        FirstPath[Index].Position,
                        SecondPath[SecondIndex].Position);
                }

                if (Cost < BestCost)
                {
                    BestCost = Cost;
                    BestOffset = Offset;
                    bBestReverse = bReverse;
                }
            }
        }

        OutAlignedSecondPath.SetNum(Count);

        for (int32 Index = 0; Index < Count; ++Index)
        {
            int32 SecondIndex = bBestReverse
                ? Count - 1 - Index
                : Index;

            if (bClosedLoop)
            {
                SecondIndex = (SecondIndex + BestOffset) % Count;
            }

            OutAlignedSecondPath[Index] = SecondPath[SecondIndex];
        }

        return true;
    }

    bool BlendBridgeVertices(
        const FACFTriangleVertex& A,
        const FACFTriangleVertex& B,
        double Alpha,
        FACFTriangleVertex& OutVertex)
    {
        const float T = FMath::Clamp(static_cast<float>(Alpha), 0.0f, 1.0f);

        OutVertex.Position = FMath::Lerp(A.Position, B.Position, T);
        OutVertex.Normal = FMath::Lerp(A.Normal, B.Normal, T).GetSafeNormal();

        FACFBoneWeightMap Weights;
        AccumulateBoneInfluences(A, 1.0f - T, Weights);
        AccumulateBoneInfluences(B, T, Weights);

        const int32 MaxInfluenceCount = FMath::Max(
            A.BoneIndices.Num(),
            B.BoneIndices.Num());

        return SetNormalizedBoneInfluences(Weights, MaxInfluenceCount, OutVertex);
    }
    
    void AccumulateBoneInfluences(
        const FACFTriangleVertex& SourceVertex,
        float Scale,
        FACFBoneWeightMap& InOutWeights)
    {
        if (Scale <= KINDA_SMALL_NUMBER)
        {
            return;
        }

        const int32 Count = FMath::Min(
            SourceVertex.BoneIndices.Num(),
            SourceVertex.BoneWeights.Num());

        for (int32 Index = 0; Index < Count; ++Index)
        {
            const int32 BoneIndex = SourceVertex.BoneIndices[Index];
            const float Weight = SourceVertex.BoneWeights[Index];

            if (BoneIndex == INDEX_NONE || Weight <= KINDA_SMALL_NUMBER)
            {
                continue;
            }

            InOutWeights.FindOrAdd(BoneIndex) += Weight * Scale;
        }
    }

    bool SetNormalizedBoneInfluences(
        const FACFBoneWeightMap& Weights,
        int32 MaxInfluenceCount,
        FACFTriangleVertex& OutVertex)
    {
        TArray<TPair<int32, float>> SortedWeights;
        SortedWeights.Reserve(Weights.Num());

        for (const TPair<int32, float>& Pair : Weights)
        {
            if (Pair.Value > KINDA_SMALL_NUMBER)
            {
                SortedWeights.Add(Pair);
            }
        }

        SortedWeights.Sort([](
            const TPair<int32, float>& A,
            const TPair<int32, float>& B)
        {
            return A.Value > B.Value;
        });

        const int32 InfluenceCount = FMath::Min(
            MaxInfluenceCount,
            SortedWeights.Num());

        if (InfluenceCount <= 0)
        {
            return false;
        }

        float TotalWeight = 0.0f;
        for (int32 Index = 0; Index < InfluenceCount; ++Index)
        {
            TotalWeight += SortedWeights[Index].Value;
        }

        if (TotalWeight <= KINDA_SMALL_NUMBER)
        {
            return false;
        }

        OutVertex.BoneIndices.Init(INDEX_NONE, MaxInfluenceCount);
        OutVertex.BoneWeights.Init(0.0f, MaxInfluenceCount);

        for (int32 Index = 0; Index < InfluenceCount; ++Index)
        {
            OutVertex.BoneIndices[Index] = SortedWeights[Index].Key;
            OutVertex.BoneWeights[Index] = SortedWeights[Index].Value / TotalWeight;
        }

        return true;
    }

    bool BuildBlendedBoneInfluences(
        const FACFTriangleVertex& V0,
        float Weight0,
        const FACFTriangleVertex& V1,
        float Weight1,
        const FACFTriangleVertex& V2,
        float Weight2,
        FACFTriangleVertex& OutVertex)
    {
        FACFBoneWeightMap Weights;

        AccumulateBoneInfluences(V0, Weight0, Weights);
        AccumulateBoneInfluences(V1, Weight1, Weights);
        AccumulateBoneInfluences(V2, Weight2, Weights);

        const int32 MaxInfluenceCount = FMath::Max(
            V0.BoneIndices.Num(),
            FMath::Max(
                V1.BoneIndices.Num(),
                V2.BoneIndices.Num()));

        return SetNormalizedBoneInfluences(Weights, MaxInfluenceCount, OutVertex);
    }
}
