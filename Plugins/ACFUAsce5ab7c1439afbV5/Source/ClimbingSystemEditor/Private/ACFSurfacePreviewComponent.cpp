// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "ACFSurfacePreviewComponent.h"

#include "ACFSurfaceAsset.h"

#include "DynamicMeshBuilder.h"
#include "Engine/Engine.h"
#include "Materials/MaterialRenderProxy.h"
#include "PrimitiveSceneProxy.h"
#include "PrimitiveUniformShaderParametersBuilder.h"
#include "PrimitiveViewRelevance.h"
#include "StaticMeshResources.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ACFSurfacePreviewComponent)

namespace
{
    constexpr float NormalLength = 5.0f;

    FDynamicMeshVertex MakeVertex(const FVector3f& Position)
    {
        FDynamicMeshVertex Vertex;
        Vertex.Position = Position;
        Vertex.Color = FColor::White;
        Vertex.SetTangents(
            FVector3f::ForwardVector,
            FVector3f::RightVector,
            FVector3f::UpVector);
        return Vertex;
    }

    uint64 MakeEdgeKey(int32 A, int32 B)
    {
        const uint32 MinIndex = static_cast<uint32>(FMath::Min(A, B));
        const uint32 MaxIndex = static_cast<uint32>(FMath::Max(A, B));
        return (static_cast<uint64>(MinIndex) << 32) | MaxIndex;
    }
}

class FACFSurfacePreviewSceneProxy final : public FPrimitiveSceneProxy
{
public:
    FACFSurfacePreviewSceneProxy(const UACFSurfacePreviewComponent* Component)
        : FPrimitiveSceneProxy(Component)
        , SurfaceVertexFactory(GetScene().GetFeatureLevel(), "FACFSurfacePreviewSceneProxy.Surface")
        , NormalVertexFactory(GetScene().GetFeatureLevel(), "FACFSurfacePreviewSceneProxy.Normal")
        , bShowSurfaceTriangles(Component->IsShowingSurfaceTriangles())
        , bShowVertexNormals(Component->IsShowingVertexNormals())
    {
        TArray<FDynamicMeshVertex> SurfaceVertices;
        SurfaceVertices.Reserve(Component->SurfacePositions.Num());
        for (const FVector3f& Position : Component->SurfacePositions)
        {
            SurfaceVertices.Add(MakeVertex(Position));
        }

        SurfaceLineIndexBuffer.Indices = Component->SurfaceLineIndices;
        SurfaceVertexBuffers.InitFromDynamicVertex(&SurfaceVertexFactory, SurfaceVertices);

        TArray<FDynamicMeshVertex> NormalVertices;
        NormalVertices.Reserve(Component->NormalPositions.Num());
        for (const FVector3f& Position : Component->NormalPositions)
        {
            NormalVertices.Add(MakeVertex(Position));
        }

        NormalLineIndexBuffer.Indices = Component->NormalLineIndices;
        NormalVertexBuffers.InitFromDynamicVertex(&NormalVertexFactory, NormalVertices);

        BeginInitResource(&SurfaceVertexBuffers.PositionVertexBuffer);
        BeginInitResource(&SurfaceVertexBuffers.StaticMeshVertexBuffer);
        BeginInitResource(&SurfaceVertexBuffers.ColorVertexBuffer);
        BeginInitResource(&SurfaceLineIndexBuffer);
        BeginInitResource(&SurfaceVertexFactory);

        BeginInitResource(&NormalVertexBuffers.PositionVertexBuffer);
        BeginInitResource(&NormalVertexBuffers.StaticMeshVertexBuffer);
        BeginInitResource(&NormalVertexBuffers.ColorVertexBuffer);
        BeginInitResource(&NormalLineIndexBuffer);
        BeginInitResource(&NormalVertexFactory);
    }

    virtual ~FACFSurfacePreviewSceneProxy()
    {
        SurfaceVertexBuffers.PositionVertexBuffer.ReleaseResource();
        SurfaceVertexBuffers.StaticMeshVertexBuffer.ReleaseResource();
        SurfaceVertexBuffers.ColorVertexBuffer.ReleaseResource();
        SurfaceLineIndexBuffer.ReleaseResource();
        SurfaceVertexFactory.ReleaseResource();

        NormalVertexBuffers.PositionVertexBuffer.ReleaseResource();
        NormalVertexBuffers.StaticMeshVertexBuffer.ReleaseResource();
        NormalVertexBuffers.ColorVertexBuffer.ReleaseResource();
        NormalLineIndexBuffer.ReleaseResource();
        NormalVertexFactory.ReleaseResource();
    }

    virtual SIZE_T GetTypeHash() const override
    {
        static size_t UniquePointer;
        return reinterpret_cast<SIZE_T>(&UniquePointer);
    }

    virtual void GetDynamicMeshElements(
        const TArray<const FSceneView*>& Views,
        const FSceneViewFamily& ViewFamily,
        uint32 VisibilityMap,
        FMeshElementCollector& Collector) const override
    {
        if (bShowSurfaceTriangles && SurfaceLineIndexBuffer.Indices.Num() > 0)
        {
            DrawLineMesh(
                Views,
                VisibilityMap,
                Collector,
                SurfaceVertexFactory,
                SurfaceVertexBuffers,
                SurfaceLineIndexBuffer,
                FLinearColor::Green,
                SDPG_World);
        }

        if (bShowVertexNormals && NormalLineIndexBuffer.Indices.Num() > 0)
        {
            DrawLineMesh(
                Views,
                VisibilityMap,
                Collector,
                NormalVertexFactory,
                NormalVertexBuffers,
                NormalLineIndexBuffer,
                FLinearColor::Red,
                SDPG_World);
        }
    }

    virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
    {
        FPrimitiveViewRelevance Result;
        Result.bDrawRelevance = IsShown(View);
        Result.bDynamicRelevance = true;
        Result.bEditorPrimitiveRelevance = UseEditorCompositing(View);
        Result.bRenderInMainPass = ShouldRenderInMainPass();
        return Result;
    }

    virtual bool CanBeOccluded() const override
    {
        return false;
    }

    virtual uint32 GetMemoryFootprint() const override
    {
        return sizeof(*this) + GetAllocatedSize();
    }

    uint32 GetAllocatedSize() const
    {
        return FPrimitiveSceneProxy::GetAllocatedSize();
    }

private:
    void DrawLineMesh(
        const TArray<const FSceneView*>& Views,
        uint32 VisibilityMap,
        FMeshElementCollector& Collector,
        const FLocalVertexFactory& VertexFactory,
        const FStaticMeshVertexBuffers& VertexBuffers,
        const FDynamicMeshIndexBuffer32& IndexBuffer,
        const FLinearColor& Color,
        ESceneDepthPriorityGroup DepthPriority) const
    {
        FColoredMaterialRenderProxy* MaterialProxy = new FColoredMaterialRenderProxy(
            GEngine->LevelColorationUnlitMaterial->GetRenderProxy(),
            Color);
        Collector.RegisterOneFrameMaterialProxy(MaterialProxy);

        for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
        {
            if (!(VisibilityMap & (1u << ViewIndex)))
            {
                continue;
            }

            FMeshBatch& Mesh = Collector.AllocateMesh();
            FMeshBatchElement& BatchElement = Mesh.Elements[0];
            BatchElement.IndexBuffer = &IndexBuffer;
            BatchElement.FirstIndex = 0;
            BatchElement.NumPrimitives = IndexBuffer.Indices.Num() / 2;
            BatchElement.MinVertexIndex = 0;
            BatchElement.MaxVertexIndex = VertexBuffers.PositionVertexBuffer.GetNumVertices() - 1;

            FDynamicPrimitiveUniformBuffer& DynamicUniformBuffer =
                Collector.AllocateOneFrameResource<FDynamicPrimitiveUniformBuffer>();
            FPrimitiveUniformShaderParametersBuilder Builder;
            BuildUniformShaderParameters(Builder);
            DynamicUniformBuffer.Set(Collector.GetRHICommandList(), Builder);
            BatchElement.PrimitiveUniformBufferResource = &DynamicUniformBuffer.UniformBuffer;

            Mesh.VertexFactory = &VertexFactory;
            Mesh.MaterialRenderProxy = MaterialProxy;
            Mesh.Type = PT_LineList;
            Mesh.DepthPriorityGroup = DepthPriority;
            Mesh.CastShadow = false;
            Mesh.bUseAsOccluder = false;
            Mesh.bCanApplyViewModeOverrides = false;
            Collector.AddMesh(ViewIndex, Mesh);
        }
    }

private:
    FStaticMeshVertexBuffers SurfaceVertexBuffers;
    FDynamicMeshIndexBuffer32 SurfaceLineIndexBuffer;
    FLocalVertexFactory SurfaceVertexFactory;

    FStaticMeshVertexBuffers NormalVertexBuffers;
    FDynamicMeshIndexBuffer32 NormalLineIndexBuffer;
    FLocalVertexFactory NormalVertexFactory;

    bool bShowSurfaceTriangles = true;
    bool bShowVertexNormals = true;
};

UACFSurfacePreviewComponent::UACFSurfacePreviewComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SetCastShadow(false);
    bHiddenInGame = true;
    bUseEditorCompositing = true;
}

void UACFSurfacePreviewComponent::SetSurfaceAsset(const UACFSurfaceAsset* InAsset)
{
    RebuildRenderData(InAsset);
    UpdateBounds();
    MarkRenderStateDirty();
}

void UACFSurfacePreviewComponent::SetShowSurfaceTriangles(bool bInShow)
{
    if (bShowSurfaceTriangles == bInShow)
    {
        return;
    }

    bShowSurfaceTriangles = bInShow;
    MarkRenderStateDirty();
}

void UACFSurfacePreviewComponent::SetShowVertexNormals(bool bInShow)
{
    if (bShowVertexNormals == bInShow)
    {
        return;
    }

    bShowVertexNormals = bInShow;
    MarkRenderStateDirty();
}

FPrimitiveSceneProxy* UACFSurfacePreviewComponent::CreateSceneProxy()
{
    if (SurfacePositions.IsEmpty() ||
        (SurfaceLineIndices.IsEmpty() && NormalLineIndices.IsEmpty()))
    {
        return nullptr;
    }

    return new FACFSurfacePreviewSceneProxy(this);
}

FBoxSphereBounds UACFSurfacePreviewComponent::CalcBounds(
    const FTransform& LocalToWorld) const
{
    if (!LocalBounds.IsValid)
    {
        return FBoxSphereBounds(FSphere(LocalToWorld.GetLocation(), 1.0f));
    }

    return FBoxSphereBounds(LocalBounds).TransformBy(LocalToWorld);
}

void UACFSurfacePreviewComponent::RebuildRenderData(const UACFSurfaceAsset* InAsset)
{
    SurfacePositions.Reset();
    SurfaceLineIndices.Reset();
    NormalPositions.Reset();
    NormalLineIndices.Reset();
    LocalBounds = FBox(ForceInit);

    if (!InAsset)
    {
        return;
    }

    SurfacePositions.Reserve(InAsset->Vertices.Num());
    NormalPositions.Reserve(InAsset->Vertices.Num() * 2);
    NormalLineIndices.Reserve(InAsset->Vertices.Num() * 2);

    for (const FACFTriangleVertex& Vertex : InAsset->Vertices)
    {
        const FVector3f Position(Vertex.Position);
        SurfacePositions.Add(Position);
        LocalBounds += Vertex.Position;

        const uint32 NormalStartIndex = static_cast<uint32>(NormalPositions.Num());
        NormalPositions.Add(Position);
        NormalPositions.Add(FVector3f(Vertex.Position + Vertex.Normal * NormalLength));
        NormalLineIndices.Add(NormalStartIndex);
        NormalLineIndices.Add(NormalStartIndex + 1);
    }

    SurfaceLineIndices.Reserve(InAsset->Triangles.Num() * 6);
    TSet<uint64> UniqueEdges;
    UniqueEdges.Reserve(InAsset->Triangles.Num() * 3);

    const auto AddEdge = [this, &UniqueEdges](int32 A, int32 B)
    {
        if (!SurfacePositions.IsValidIndex(A) ||
            !SurfacePositions.IsValidIndex(B) ||
            A == B)
        {
            return;
        }

        const uint64 EdgeKey = MakeEdgeKey(A, B);
        if (UniqueEdges.Contains(EdgeKey))
        {
            return;
        }

        UniqueEdges.Add(EdgeKey);

        SurfaceLineIndices.Add(static_cast<uint32>(A));
        SurfaceLineIndices.Add(static_cast<uint32>(B));
    };

    for (const FACFTriangle& Triangle : InAsset->Triangles)
    {
        const int32 I0 = Triangle.VertexIndices[0];
        const int32 I1 = Triangle.VertexIndices[1];
        const int32 I2 = Triangle.VertexIndices[2];

        AddEdge(I0, I1);
        AddEdge(I1, I2);
        AddEdge(I2, I0);
    }
}
