// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "Components/PrimitiveComponent.h"
#include "ACFSurfacePreviewComponent.generated.h"

class FPrimitiveSceneProxy;
class FACFSurfacePreviewSceneProxy;
class UACFSurfaceAsset;

/**
 * Static render data used by the ACF Climb Surface asset-editor overlay.
 * The data is rebuilt only when the asset preview is refreshed.
 */
UCLASS(Transient)
class CLIMBINGSYSTEMEDITOR_API UACFSurfacePreviewComponent : public UPrimitiveComponent
{
    GENERATED_BODY()

public:
    UACFSurfacePreviewComponent();

    void SetSurfaceAsset(const UACFSurfaceAsset* InAsset);

    void SetShowSurfaceTriangles(bool bInShow);
    bool IsShowingSurfaceTriangles() const { return bShowSurfaceTriangles; }

    void SetShowVertexNormals(bool bInShow);
    bool IsShowingVertexNormals() const { return bShowVertexNormals; }

    virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
    virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;

private:
    friend class FACFSurfacePreviewSceneProxy;

    void RebuildRenderData(const UACFSurfaceAsset* InAsset);

private:
    TArray<FVector3f> SurfacePositions;
    TArray<uint32> SurfaceLineIndices;

    TArray<FVector3f> NormalPositions;
    TArray<uint32> NormalLineIndices;

    FBox LocalBounds = FBox(ForceInit);

    bool bShowSurfaceTriangles = true;
    bool bShowVertexNormals = false;
};
