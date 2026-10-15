// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditorViewportClient.h"
#include "ACFSurfaceAsset.h"

class FAdvancedPreviewScene;
class UACFSurfaceAsset;
class UACFSurfacePreviewComponent;
struct FACFTriangleVertex;

enum class EACFBridgeEditState : uint8
{
    Inactive,
    EditingFirstPath,
    EditingSecondPath,
    ReadyToPreview
};

class CLIMBINGSYSTEMEDITOR_API FACFSurfaceViewportClient : public FEditorViewportClient
{
public:
    FACFSurfaceViewportClient(
        FAdvancedPreviewScene& InPreviewScene,
        const TSharedRef<SEditorViewport>& InEditorViewport,
        UACFSurfaceAsset* InAsset,
        UACFSurfacePreviewComponent* InSurfacePreviewComponent);

    virtual void Tick(float DeltaSeconds) override;

    virtual void Draw(const FSceneView* View, FPrimitiveDrawInterface* PDI) override;

    virtual bool InputKey(const FInputKeyEventArgs& EventArgs) override;

    bool PickSurfacePoint(
        const FVector& RayOrigin,
        const FVector& RayDirection,
        FACFBridgePoint& OutPoint) const;

    void SetSelectedBone(FName InBoneName) { SelectedBoneName = InBoneName; }
    void ClearSelectedBone() { SelectedBoneName = NAME_None; }

    int32 GetSelectedBoneIndex() const;
    bool VertexHasBone(const FACFTriangleVertex& Vertex, int32 BoneIndex) const;

    void SetShowSurfaceTriangles(bool bInShow);
    bool IsShowingSurfaceTriangles() const { return bShowSurfaceTriangles; }

    void SetShowVertexNormals(bool bInShow);
    bool IsShowingVertexNormals() const { return bShowVertexNormals; }

    void SetBridgeEditMode(bool bEnabled);
    bool IsBridgeEditModeEnabled() const;

    void ToggleBridgePathType();
    bool IsBridgeClosedLoop() const;
    bool CanChangeBridgePathType() const;

    bool AddBridgePoint(const FACFBridgePoint& Point);
    bool CanFinishBridgePath() const;
    bool FinishBridgePath();

    bool CanUndoBridgePoint() const;
    void UndoBridgePoint();

    void CancelBridgeEditing();

    bool CanBuildBridgeDefinition() const;
    bool BuildBridgeDefinition(FACFBridgeDefinition& OutDefinition) const;
    void CompleteBridgeEditing();

    bool CanFlipBridgeNormal() const;
    bool IsBridgeWindingReversed() const { return bReverseBridgeWinding; }
    void FlipBridgeNormal();

    void SetSelectedBridgeIndex(int32 BridgeIndex);
    void ClearSelectedBridge();
    bool FocusSelectedBridge();

    FString GetBridgeStatusText() const;
    FString GetBridgeInstructionText() const;
    FString GetBridgeFinishActionText() const;

private:
    TArray<FACFBridgePoint>* GetActiveBridgePathPoints();
    const TArray<FACFBridgePoint>* GetActiveBridgePathPoints() const;

    bool BuildBridgePreview();
    void ClearBridgePreview();
    void UpdateBridgeHover();
    void UpdateBridgeHover(const FVector& RayOrigin, const FVector& RayDirection);

    static bool ResampleBridgePath(
        const TArray<FACFBridgePoint>& SourcePoints,
        bool bClosedLoop,
        int32 TargetPointCount,
        TArray<FVector>& OutPoints);

    static void AlignSecondPath(
        const TArray<FVector>& FirstPath,
        const TArray<FVector>& SecondPath,
        bool bClosedLoop,
        TArray<FVector>& OutAlignedSecondPath);

    void DrawBridgePreview(FPrimitiveDrawInterface* PDI) const;

private:
    TWeakObjectPtr<UACFSurfaceAsset> Asset = nullptr;
    TWeakObjectPtr<UACFSurfacePreviewComponent> SurfacePreviewComponent = nullptr;

    bool bShowSurfaceTriangles = true;
    bool bShowVertexNormals = false;

    FName SelectedBoneName = NAME_None;

    EACFBridgePathType BridgePathType = EACFBridgePathType::OpenSpline;
    EACFBridgeEditState BridgeEditState = EACFBridgeEditState::Inactive;

    TArray<FACFBridgePoint> FirstBridgePathPoints;
    TArray<FACFBridgePoint> SecondBridgePathPoints;

    TArray<FVector> PreviewBridgeVertices;
    TArray<int32> PreviewBridgeIndices;
    TArray<FVector> PreviewFirstAutomaticBridgePoints;
    TArray<FVector> PreviewSecondAutomaticBridgePoints;
    FString BridgePreviewError;

    bool bReverseBridgeWinding = false;

    int32 SelectedBridgeIndex = INDEX_NONE;

    TOptional<FACFBridgePoint> HoverBridgePoint;
};
