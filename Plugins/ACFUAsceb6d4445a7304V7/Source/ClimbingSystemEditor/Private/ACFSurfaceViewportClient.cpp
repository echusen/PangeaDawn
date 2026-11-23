// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "ACFSurfaceViewportClient.h"

#include "ACFBridgePathExpansion.h"
#include "ACFSurfaceAsset.h"
#include "ACFSurfacePreviewComponent.h"

#include "AdvancedPreviewScene.h"

FACFSurfaceViewportClient::FACFSurfaceViewportClient(
    FAdvancedPreviewScene& InPreviewScene,
    const TSharedRef<SEditorViewport>& InViewport,
    UACFSurfaceAsset* InAsset,
    UACFSurfacePreviewComponent* InSurfacePreviewComponent)
    : FEditorViewportClient(nullptr, &InPreviewScene, InViewport)
    , Asset(InAsset)
    , SurfacePreviewComponent(InSurfacePreviewComponent)
{
    SetViewLocation(FVector(200, 200, 150));
    SetViewRotation(FRotator(-25, -135, 0));
    SetViewModes(VMI_Lit, VMI_Lit);

    EngineShowFlags.SetGrid(true);
    bSetListenerPosition = false;
}

void FACFSurfaceViewportClient::Tick(float DeltaSeconds)
{
    FEditorViewportClient::Tick(DeltaSeconds);

    if (PreviewScene && PreviewScene->GetWorld())
    {
        PreviewScene->GetWorld()->Tick(LEVELTICK_All, DeltaSeconds);
    }

    UpdateBridgeHover();
}

void FACFSurfaceViewportClient::Draw(const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
    FEditorViewportClient::Draw(View, PDI);

    if (!Asset.IsValid()) return;

    const int32 SelectedBoneIndex = GetSelectedBoneIndex();

    auto DrawBridgePath = [PDI](
        const TArray<FACFBridgePoint>& Points,
        const FLinearColor& Color,
        bool bClosePath)
    {
        for (int32 Index = 0; Index < Points.Num(); ++Index)
        {
            const FVector& Position = Points[Index].LocalPosition;

            PDI->DrawPoint(Position, Color, 10.0f, SDPG_Foreground);

            if (Index > 0)
            {
                PDI->DrawLine(
                    Points[Index - 1].LocalPosition,
                    Position,
                    Color,
                    SDPG_Foreground);
            }
        }

        if (bClosePath && Points.Num() >= 3)
        {
            PDI->DrawLine(
                Points.Last().LocalPosition,
                Points[0].LocalPosition,
                Color,
                SDPG_Foreground);
        }
    };

    const bool bIsClosedLoop = IsBridgeClosedLoop();
    const bool bIsFirstPathComplete =
        BridgeEditState == EACFBridgeEditState::EditingSecondPath ||
        BridgeEditState == EACFBridgeEditState::ReadyToPreview;
    const bool bIsSecondPathComplete =
        BridgeEditState == EACFBridgeEditState::ReadyToPreview;

    DrawBridgePath(FirstBridgePathPoints, FLinearColor(FColor::Orange), bIsClosedLoop && bIsFirstPathComplete);
    DrawBridgePath(SecondBridgePathPoints, FLinearColor::Blue, bIsClosedLoop && bIsSecondPathComplete);

    for (int32 BridgeIndex = 0; BridgeIndex < Asset->BridgeDefinitions.Num(); ++BridgeIndex)
    {
        const FACFBridgeDefinition& Definition = Asset->BridgeDefinitions[BridgeIndex];
        const bool bSelected = BridgeIndex == SelectedBridgeIndex;
        const FLinearColor Color = bSelected
            ? FLinearColor(1.0f, 0.55f, 0.0f)
            : FLinearColor(0.0f, 0.7f, 1.0f);
        const bool bClosedLoop = Definition.PathType == EACFBridgePathType::ClosedLoop;

        DrawBridgePath(Definition.FirstPathPoints, Color, bClosedLoop);
        DrawBridgePath(Definition.SecondPathPoints, Color, bClosedLoop);
    }

    DrawBridgePreview(PDI);

    for (const FVector& Point : PreviewFirstAutomaticBridgePoints)
    {
        PDI->DrawPoint(Point, FLinearColor(1.0f, 0.85f, 0.15f), 5.0f, SDPG_Foreground);
    }

    for (const FVector& Point : PreviewSecondAutomaticBridgePoints)
    {
        PDI->DrawPoint(Point, FLinearColor(0.2f, 0.75f, 1.0f), 5.0f, SDPG_Foreground);
    }

    if (HoverBridgePoint.IsSet())
    {
        PDI->DrawPoint(
            HoverBridgePoint->LocalPosition,
            FLinearColor::Yellow,
            3.0f,
            SDPG_Foreground);
    }
}

void FACFSurfaceViewportClient::SetShowSurfaceTriangles(bool bInShow)
{
    bShowSurfaceTriangles = bInShow;

    if (SurfacePreviewComponent.IsValid())
    {
        SurfacePreviewComponent->SetShowSurfaceTriangles(bInShow);
    }
}

void FACFSurfaceViewportClient::SetShowVertexNormals(bool bInShow)
{
    bShowVertexNormals = bInShow;

    if (SurfacePreviewComponent.IsValid())
    {
        SurfacePreviewComponent->SetShowVertexNormals(bInShow);
    }
}

bool FACFSurfaceViewportClient::InputKey(const FInputKeyEventArgs& EventArgs)
{
    if (IsBridgeEditModeEnabled() && EventArgs.Event == IE_Pressed)
    {
        if (EventArgs.Key == EKeys::Escape)
        {
            CancelBridgeEditing();
            Invalidate();
            return true;
        }

        if (EventArgs.Key == EKeys::BackSpace && CanUndoBridgePoint())
        {
            UndoBridgePoint();
            Invalidate();
            return true;
        }

        if (EventArgs.Key == EKeys::Enter && CanFinishBridgePath())
        {
            FinishBridgePath();
            Invalidate();
            return true;
        }
    }

    const bool bIsBridgeClick =
        IsBridgeEditModeEnabled() &&
        EventArgs.Key == EKeys::LeftMouseButton &&
        EventArgs.Event == IE_Pressed;

    if (!bIsBridgeClick)
    {
        return FEditorViewportClient::InputKey(EventArgs);
    }

    const FViewportCursorLocation Cursor = GetCursorWorldLocationFromMousePos();
    UpdateBridgeHover(Cursor.GetOrigin(), Cursor.GetDirection());

    if (HoverBridgePoint.IsSet())
    {
        AddBridgePoint(HoverBridgePoint.GetValue());
    }

    Invalidate();
    return true;
}

bool FACFSurfaceViewportClient::PickSurfacePoint(
    const FVector& RayOrigin,
    const FVector& RayDirection,
    FACFBridgePoint& OutPoint) const
{
    if (!Asset.IsValid())
    {
        return false;
    }

    constexpr double MaxPickDistance = 1000000.0;
    const FVector Direction = RayDirection.GetSafeNormal();
    if (Direction.IsNearlyZero())
    {
        return false;
    }

    const FVector RayEnd = RayOrigin + Direction * MaxPickDistance;

    const int32 PickTriangleCount = Asset->BaseSurfaceTriangleCount > 0
        ? FMath::Min(Asset->BaseSurfaceTriangleCount, Asset->Triangles.Num())
        : Asset->Triangles.Num();

    double NearestDistanceSq = TNumericLimits<double>::Max();
    bool bFound = false;

    for (int32 TriangleIndex = 0; TriangleIndex < PickTriangleCount; ++TriangleIndex)
    {
        const FACFTriangle& Triangle = Asset->Triangles[TriangleIndex];

        const int32 I0 = Triangle.VertexIndices[0];
        const int32 I1 = Triangle.VertexIndices[1];
        const int32 I2 = Triangle.VertexIndices[2];

        if (!Asset->Vertices.IsValidIndex(I0) ||
            !Asset->Vertices.IsValidIndex(I1) ||
            !Asset->Vertices.IsValidIndex(I2))
        {
            continue;
        }

        const FVector& A = Asset->Vertices[I0].Position;
        const FVector& B = Asset->Vertices[I1].Position;
        const FVector& C = Asset->Vertices[I2].Position;

        FVector HitPosition;
        FVector TriangleNormal;

        if (!FMath::SegmentTriangleIntersection(
            RayOrigin, RayEnd, A, B, C, HitPosition, TriangleNormal))
        {
            continue;
        }

        const double DistanceSq = FVector::DistSquared(RayOrigin, HitPosition);

        if (DistanceSq >= NearestDistanceSq)
        {
            continue;
        }

        FVector Barycentric;
        if (!FMath::ComputeBarycentricTri(HitPosition, A, B, C, Barycentric))
        {
            continue;
        }

        NearestDistanceSq = DistanceSq;
        OutPoint.TriangleIndex = TriangleIndex;
        OutPoint.Barycentric = Barycentric;
        OutPoint.LocalPosition = HitPosition;
        bFound = true;
    }

    return bFound;
}

int32 FACFSurfaceViewportClient::GetSelectedBoneIndex() const
{
    if (!Asset.IsValid() || !Asset->SourceMesh || SelectedBoneName.IsNone()) return INDEX_NONE;

    return Asset->SourceMesh->GetRefSkeleton().FindBoneIndex(SelectedBoneName);
}

bool FACFSurfaceViewportClient::VertexHasBone(
    const FACFTriangleVertex& Vertex,
    int32 BoneIndex) const
{
    for (int32 InfluenceIndex = 0; InfluenceIndex < Vertex.BoneIndices.Num(); ++InfluenceIndex)
    {
        if (Vertex.BoneIndices[InfluenceIndex] == BoneIndex &&
            Vertex.BoneWeights.IsValidIndex(InfluenceIndex) &&
            Vertex.BoneWeights[InfluenceIndex] > KINDA_SMALL_NUMBER)
        {
            return true;
        }
    }

    return false;
}

void FACFSurfaceViewportClient::SetBridgeEditMode(bool bEnabled)
{
    if (!bEnabled)
    {
        CancelBridgeEditing();
        return;
    }

    if (BridgeEditState != EACFBridgeEditState::Inactive)
    {
        return;
    }

    BridgeEditState = EACFBridgeEditState::EditingFirstPath;
    FirstBridgePathPoints.Reset();
    SecondBridgePathPoints.Reset();
    bReverseBridgeWinding = false;
    HoverBridgePoint.Reset();
    ClearBridgePreview();
}

bool FACFSurfaceViewportClient::IsBridgeEditModeEnabled() const
{
    return BridgeEditState != EACFBridgeEditState::Inactive;
}

void FACFSurfaceViewportClient::ToggleBridgePathType()
{
    if (!CanChangeBridgePathType())
    {
        return;
    }

    BridgePathType = BridgePathType == EACFBridgePathType::OpenSpline
        ? EACFBridgePathType::ClosedLoop
        : EACFBridgePathType::OpenSpline;
}

bool FACFSurfaceViewportClient::IsBridgeClosedLoop() const
{
    return BridgePathType == EACFBridgePathType::ClosedLoop;
}

bool FACFSurfaceViewportClient::CanChangeBridgePathType() const
{
    return IsBridgeEditModeEnabled() &&
        FirstBridgePathPoints.IsEmpty() &&
        SecondBridgePathPoints.IsEmpty();
}

bool FACFSurfaceViewportClient::AddBridgePoint(const FACFBridgePoint& Point)
{
    TArray<FACFBridgePoint>* ActivePath = GetActiveBridgePathPoints();
    if (!ActivePath)
    {
        return false;
    }

    ActivePath->Add(Point);
    return true;
}

bool FACFSurfaceViewportClient::CanFinishBridgePath() const
{
    const TArray<FACFBridgePoint>* ActivePath = GetActiveBridgePathPoints();
    if (!ActivePath)
    {
        return false;
    }

    const int32 MinimumPointCount = BridgePathType == EACFBridgePathType::ClosedLoop ? 3 : 2;

    return ActivePath->Num() >= MinimumPointCount;
}

bool FACFSurfaceViewportClient::FinishBridgePath()
{
    if (!CanFinishBridgePath())
    {
        return false;
    }

    if (BridgeEditState == EACFBridgeEditState::EditingFirstPath)
    {
        BridgeEditState = EACFBridgeEditState::EditingSecondPath;
        return true;
    }

    if (BridgeEditState == EACFBridgeEditState::EditingSecondPath)
    {
        BridgeEditState = EACFBridgeEditState::ReadyToPreview;

        if (!BuildBridgePreview())
        {
            BridgePreviewError = TEXT("Failed to generate build preview.");
        }

        return true;
    }

    return false;
}

bool FACFSurfaceViewportClient::CanUndoBridgePoint() const
{
    const TArray<FACFBridgePoint>* ActivePath = GetActiveBridgePathPoints();
    return ActivePath && !ActivePath->IsEmpty();
}

void FACFSurfaceViewportClient::UndoBridgePoint()
{
    if (TArray<FACFBridgePoint>* ActivePath = GetActiveBridgePathPoints())
    {
        ActivePath->Pop();
    }
}

void FACFSurfaceViewportClient::CancelBridgeEditing()
{
    FirstBridgePathPoints.Reset();
    SecondBridgePathPoints.Reset();
    BridgeEditState = EACFBridgeEditState::Inactive;
    bReverseBridgeWinding = false;
    HoverBridgePoint.Reset();
    ClearBridgePreview();
}

void FACFSurfaceViewportClient::UpdateBridgeHover()
{
    if (BridgeEditState != EACFBridgeEditState::EditingFirstPath &&
        BridgeEditState != EACFBridgeEditState::EditingSecondPath)
    {
        HoverBridgePoint.Reset();
        return;
    }

    const FViewportCursorLocation Cursor = GetCursorWorldLocationFromMousePos();
    UpdateBridgeHover(Cursor.GetOrigin(), Cursor.GetDirection());
    Invalidate();
}

void FACFSurfaceViewportClient::UpdateBridgeHover(
    const FVector& RayOrigin,
    const FVector& RayDirection)
{
    HoverBridgePoint.Reset();

    FACFBridgePoint Point;
    if (!PickSurfacePoint(RayOrigin, RayDirection, Point))
    {
        return;
    }

    HoverBridgePoint = Point;
}

bool FACFSurfaceViewportClient::CanBuildBridgeDefinition() const
{
    return BridgeEditState == EACFBridgeEditState::ReadyToPreview &&
        !PreviewBridgeVertices.IsEmpty() &&
        !PreviewBridgeIndices.IsEmpty();
}

bool FACFSurfaceViewportClient::BuildBridgeDefinition(FACFBridgeDefinition& OutDefinition) const
{
    if (!CanBuildBridgeDefinition())
    {
        return false;
    }

    OutDefinition = {};
    OutDefinition.PathType = BridgePathType;
    OutDefinition.FirstPathPoints = FirstBridgePathPoints;
    OutDefinition.SecondPathPoints = SecondBridgePathPoints;
    OutDefinition.bReverseWinding = bReverseBridgeWinding;

    return true;
}

void FACFSurfaceViewportClient::CompleteBridgeEditing()
{
    FirstBridgePathPoints.Reset();
    SecondBridgePathPoints.Reset();
    BridgeEditState = EACFBridgeEditState::Inactive;
    bReverseBridgeWinding = false;
    HoverBridgePoint.Reset();
    ClearBridgePreview();
}

bool FACFSurfaceViewportClient::CanFlipBridgeNormal() const
{
    return CanBuildBridgeDefinition();
}

void FACFSurfaceViewportClient::FlipBridgeNormal()
{
    if (!CanFlipBridgeNormal())
    {
        return;
    }

    bReverseBridgeWinding = !bReverseBridgeWinding;

    if (!BuildBridgePreview())
    {
        bReverseBridgeWinding = !bReverseBridgeWinding;
        BuildBridgePreview();
    }
}

void FACFSurfaceViewportClient::SetSelectedBridgeIndex(int32 BridgeIndex)
{
    SelectedBridgeIndex = Asset.IsValid() && Asset->BridgeDefinitions.IsValidIndex(BridgeIndex)
        ? BridgeIndex
        : INDEX_NONE;
}

void FACFSurfaceViewportClient::ClearSelectedBridge()
{
    SelectedBridgeIndex = INDEX_NONE;
}

bool FACFSurfaceViewportClient::FocusSelectedBridge()
{
    if (!Asset.IsValid() || !Asset->BridgeDefinitions.IsValidIndex(SelectedBridgeIndex))
    {
        return false;
    }

    const FACFBridgeDefinition& Definition = Asset->BridgeDefinitions[SelectedBridgeIndex];
    FBox Bounds(ForceInit);

    for (const FACFBridgePoint& Point : Definition.FirstPathPoints)
    {
        Bounds += Point.LocalPosition;
    }

    for (const FACFBridgePoint& Point : Definition.SecondPathPoints)
    {
        Bounds += Point.LocalPosition;
    }

    if (!Bounds.IsValid)
    {
        return false;
    }

    FocusViewportOnBox(Bounds.ExpandBy(20.0f));
    return true;
}

FString FACFSurfaceViewportClient::GetBridgeStatusText() const
{
    const TCHAR* PathType =
        BridgePathType == EACFBridgePathType::ClosedLoop
        ? TEXT("Closed Loop")
        : TEXT("Open Spline");

    switch (BridgeEditState)
    {
    case EACFBridgeEditState::EditingFirstPath:
        return FString::Printf(
            TEXT("Bridge: First Path / %s / Points: %d"),
            PathType,
            FirstBridgePathPoints.Num());

    case EACFBridgeEditState::EditingSecondPath:
        return FString::Printf(
            TEXT("Bridge: Second Path / %s / Points: %d"),
            PathType,
            SecondBridgePathPoints.Num());

    case EACFBridgeEditState::ReadyToPreview:
        return FString::Printf(
            TEXT("Bridge: Paths Complete / %s\nPreview ready"),
            PathType);

    default:
        return TEXT("Bridge: Off");
    }
}

FString FACFSurfaceViewportClient::GetBridgeInstructionText() const
{
    switch (BridgeEditState)
    {
    case EACFBridgeEditState::EditingFirstPath:
        return TEXT("Step 1 of 3 - Click the surface to draw the first path. Enter: Finish  Backspace: Undo  Esc: Cancel");

    case EACFBridgeEditState::EditingSecondPath:
        return TEXT("Step 2 of 3 - Click the surface to draw the second path. Enter: Finish  Backspace: Undo  Esc: Cancel");

    case EACFBridgeEditState::ReadyToPreview:
        return TEXT("Step 3 of 3 - Review the bridge preview, then save it or flip its normal.");

    default:
        return TEXT("Create Bridge to begin drawing two paths on the surface.");
    }
}

FString FACFSurfaceViewportClient::GetBridgeFinishActionText() const
{
    switch (BridgeEditState)
    {
    case EACFBridgeEditState::EditingFirstPath:
        return TEXT("Finish First Path");

    case EACFBridgeEditState::EditingSecondPath:
        return TEXT("Finish Second Path");

    case EACFBridgeEditState::ReadyToPreview:
        return TEXT("Preview Ready");

    default:
        return TEXT("Finish Path");
    }
}

TArray<FACFBridgePoint>* FACFSurfaceViewportClient::GetActiveBridgePathPoints()
{
    switch (BridgeEditState)
    {
    case EACFBridgeEditState::EditingFirstPath:
        return &FirstBridgePathPoints;

    case EACFBridgeEditState::EditingSecondPath:
        return &SecondBridgePathPoints;

    default:
        return nullptr;
    }
}

const TArray<FACFBridgePoint>* FACFSurfaceViewportClient::GetActiveBridgePathPoints() const
{
    switch (BridgeEditState)
    {
    case EACFBridgeEditState::EditingFirstPath:
        return &FirstBridgePathPoints;

    case EACFBridgeEditState::EditingSecondPath:
        return &SecondBridgePathPoints;

    default:
        return nullptr;
    }
}

bool FACFSurfaceViewportClient::BuildBridgePreview()
{
    ClearBridgePreview();

    const bool bClosedLoop = IsBridgeClosedLoop();

    FACFBridgePathTopology Topology;
    TArray<FACFBridgePoint> ExpandedFirstPathPoints;
    TArray<FACFBridgePoint> ExpandedSecondPathPoints;

    if (!FACFBridgePathExpansion::BuildBaseSurfaceTopology(Asset.Get(), Topology, &BridgePreviewError) ||
        !FACFBridgePathExpansion::ExpandPath(
            Asset.Get(), Topology, FirstBridgePathPoints, bClosedLoop,
            ExpandedFirstPathPoints, &BridgePreviewError) ||
        !FACFBridgePathExpansion::ExpandPath(
            Asset.Get(), Topology, SecondBridgePathPoints, bClosedLoop,
            ExpandedSecondPathPoints, &BridgePreviewError))
    {
        return false;
    }

    const auto CollectAutomaticPoints = [](
        const TArray<FACFBridgePoint>& SourcePoints,
        const TArray<FACFBridgePoint>& ExpandedPoints,
        TArray<FVector>& OutAutomaticPoints)
    {
        OutAutomaticPoints.Reset();

        for (const FACFBridgePoint& ExpandedPoint : ExpandedPoints)
        {
            if (!SourcePoints.ContainsByPredicate([&ExpandedPoint](const FACFBridgePoint& SourcePoint)
            {
                return SourcePoint.LocalPosition.Equals(
                    ExpandedPoint.LocalPosition,
                    KINDA_SMALL_NUMBER);
            }))
            {
                OutAutomaticPoints.Add(ExpandedPoint.LocalPosition);
            }
        }
    };

    CollectAutomaticPoints(
        FirstBridgePathPoints,
        ExpandedFirstPathPoints,
        PreviewFirstAutomaticBridgePoints);
    CollectAutomaticPoints(
        SecondBridgePathPoints,
        ExpandedSecondPathPoints,
        PreviewSecondAutomaticBridgePoints);

    const int32 TargetPointCount = FMath::Max(
        ExpandedFirstPathPoints.Num(),
        ExpandedSecondPathPoints.Num());

    TArray<FVector> FirstPath;
    TArray<FVector> SecondPath;

    if (!ResampleBridgePath(
            ExpandedFirstPathPoints,
            bClosedLoop,
            TargetPointCount,
            FirstPath) ||
        !ResampleBridgePath(
            ExpandedSecondPathPoints,
            bClosedLoop,
            TargetPointCount,
            SecondPath))
    {
        return false;
    }

    TArray<FVector> AlignedSecondPath;
    AlignSecondPath(FirstPath, SecondPath, bClosedLoop, AlignedSecondPath);

    PreviewBridgeVertices.Reserve(TargetPointCount * 2);

    for (int32 Index = 0; Index < TargetPointCount; ++Index)
    {
        PreviewBridgeVertices.Add(FirstPath[Index]);
        PreviewBridgeVertices.Add(AlignedSecondPath[Index]);
    }

    const int32 SegmentCount = bClosedLoop ? TargetPointCount : TargetPointCount - 1;

    PreviewBridgeIndices.Reserve(SegmentCount * 6);

    for (int32 SegmentIndex = 0; SegmentIndex < SegmentCount; ++SegmentIndex)
    {
        const int32 NextIndex = (SegmentIndex + 1) % TargetPointCount;

        const int32 A0 = SegmentIndex * 2;
        const int32 B0 = A0 + 1;
        const int32 A1 = NextIndex * 2;
        const int32 B1 = A1 + 1;

        if (bReverseBridgeWinding)
        {
            PreviewBridgeIndices.Append({
                A0, B0, A1,
                B0, B1, A1
            });
        }
        else
        {
            PreviewBridgeIndices.Append({
                A0, A1, B0,
                B0, A1, B1
            });
        }
    }

    return PreviewBridgeIndices.Num() > 0;
}

void FACFSurfaceViewportClient::ClearBridgePreview()
{
    PreviewBridgeVertices.Reset();
    PreviewBridgeIndices.Reset();
    PreviewFirstAutomaticBridgePoints.Reset();
    PreviewSecondAutomaticBridgePoints.Reset();
    BridgePreviewError.Empty();
}

bool FACFSurfaceViewportClient::ResampleBridgePath(
    const TArray<FACFBridgePoint>& SourcePoints,
    bool bClosedLoop,
    int32 TargetPointCount,
    TArray<FVector>& OutPoints)
{
    OutPoints.Reset();

    const int32 MinimumPointCount = bClosedLoop ? 3 : 2;
    if (SourcePoints.Num() < MinimumPointCount ||
        TargetPointCount < MinimumPointCount)
    {
        return false;
    }

    if (TargetPointCount < SourcePoints.Num()) return false;

    const int32 SegmentCount = bClosedLoop ? SourcePoints.Num() : SourcePoints.Num() - 1;
    TArray<int32> SegmentSubdivisions;
    SegmentSubdivisions.Init(1, SegmentCount);

    for (int32 ExtraSampleCount = TargetPointCount - SourcePoints.Num(); ExtraSampleCount > 0; --ExtraSampleCount)
    {
        int32 BestSegmentIndex = 0;
        double BestDensity = -1.0;

        for (int32 SegmentIndex = 0; SegmentIndex < SegmentCount; ++SegmentIndex)
        {
            const int32 NextIndex = (SegmentIndex + 1) % SourcePoints.Num();
            const double Density = FVector::Distance(
                SourcePoints[SegmentIndex].LocalPosition,
                SourcePoints[NextIndex].LocalPosition) / SegmentSubdivisions[SegmentIndex];

            if (Density > BestDensity)
            {
                BestDensity = Density;
                BestSegmentIndex = SegmentIndex;
            }
        }

        ++SegmentSubdivisions[BestSegmentIndex];
    }

    OutPoints.Reserve(TargetPointCount);
    if (!bClosedLoop) OutPoints.Add(SourcePoints[0].LocalPosition);

    for (int32 SegmentIndex = 0; SegmentIndex < SegmentCount; ++SegmentIndex)
    {
        const int32 NextIndex = (SegmentIndex + 1) % SourcePoints.Num();
        if (bClosedLoop) OutPoints.Add(SourcePoints[SegmentIndex].LocalPosition);

        for (int32 SubdivisionIndex = 1; SubdivisionIndex < SegmentSubdivisions[SegmentIndex]; ++SubdivisionIndex)
        {
            const double Alpha = static_cast<double>(SubdivisionIndex) / SegmentSubdivisions[SegmentIndex];
            OutPoints.Add(FMath::Lerp(
                SourcePoints[SegmentIndex].LocalPosition,
                SourcePoints[NextIndex].LocalPosition,
                Alpha));
        }

        if (!bClosedLoop || SegmentIndex == SegmentCount - 1)
        {
            OutPoints.Add(SourcePoints[NextIndex].LocalPosition);
        }
    }

    return OutPoints.Num() == TargetPointCount;
}

void FACFSurfaceViewportClient::AlignSecondPath(
    const TArray<FVector>& FirstPath,
    const TArray<FVector>& SecondPath,
    bool bClosedLoop,
    TArray<FVector>& OutAlignedSecondPath)
{
    const int32 Count = FirstPath.Num();

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
                int32 SecondIndex = bReverse ? Count - 1 - Index : Index;

                if (bClosedLoop)
                {
                    SecondIndex = (SecondIndex + Offset) % Count;
                }

                Cost += FVector::DistSquared(
                    FirstPath[Index],
                    SecondPath[SecondIndex]);
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
        int32 SecondIndex = bBestReverse ? Count - 1 - Index : Index;

        if (bClosedLoop)
        {
            SecondIndex = (SecondIndex + BestOffset) % Count;
        }

        OutAlignedSecondPath[Index] = SecondPath[SecondIndex];
    }
}

void FACFSurfaceViewportClient::DrawBridgePreview(FPrimitiveDrawInterface* PDI) const
{
    const FLinearColor Color(0.65f, 0.2f, 1.0f);

    for (int32 Index = 0; Index + 2 < PreviewBridgeIndices.Num(); Index += 3)
    {
        const int32 I0 = PreviewBridgeIndices[Index];
        const int32 I1 = PreviewBridgeIndices[Index + 1];
        const int32 I2 = PreviewBridgeIndices[Index + 2];

        if (!PreviewBridgeVertices.IsValidIndex(I0) ||
            !PreviewBridgeVertices.IsValidIndex(I1) ||
            !PreviewBridgeVertices.IsValidIndex(I2))
        {
            continue;
        }

        const FVector& P0 = PreviewBridgeVertices[I0];
        const FVector& P1 = PreviewBridgeVertices[I1];
        const FVector& P2 = PreviewBridgeVertices[I2];

        PDI->DrawLine(P0, P1, Color, SDPG_Foreground);
        PDI->DrawLine(P1, P2, Color, SDPG_Foreground);
        PDI->DrawLine(P2, P0, Color, SDPG_Foreground);

        const FVector FaceNormal = FVector::CrossProduct(
            P2 - P0,
            P1 - P0).GetSafeNormal();

        if (!FaceNormal.IsNearlyZero())
        {
            const FVector Center = (P0 + P1 + P2) / 3.0;

            PDI->DrawLine(
                Center,
                Center + FaceNormal * 5.0,
                FLinearColor::Yellow,
                SDPG_Foreground);
        }
    }
}
