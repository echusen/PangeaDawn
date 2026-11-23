// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "SACFSurfaceViewport.h"

#include "SACFSurfaceViewportToolbar.h"
#include "ACFSurfaceViewportClient.h"
#include "ACFSurfaceAsset.h"
#include "ACFSurfacePreviewComponent.h"

#include "AdvancedPreviewScene.h"
#include "Animation/DebugSkelMeshComponent.h"
#include "ToolMenus.h"

const FName SACFSurfaceViewport::ViewportToolbarName = TEXT("ACFFreeClimbSurface.ViewportToolbar");

void SACFSurfaceViewport::Construct(
    const FArguments& InArgs,
    UACFSurfaceAsset* InAsset)
{
    Asset = InAsset;

    PreviewScene = MakeShared<FAdvancedPreviewScene>(FPreviewScene::ConstructionValues());

    PreviewMeshComponent = NewObject<UDebugSkelMeshComponent>();
    PreviewMeshComponent->SetMobility(EComponentMobility::Movable);

    PreviewScene->AddComponent(PreviewMeshComponent, FTransform::Identity);

    SurfacePreviewComponent = NewObject<UACFSurfacePreviewComponent>();
    SurfacePreviewComponent->SetMobility(EComponentMobility::Movable);
    PreviewScene->AddComponent(SurfacePreviewComponent, FTransform::Identity);

    SEditorViewport::Construct(SEditorViewport::FArguments());

    RefreshPreview();
}

void SACFSurfaceViewport::RefreshPreview()
{
    if (!Asset || !PreviewMeshComponent || !SurfacePreviewComponent) return;

    PreviewMeshComponent->SetSkeletalMesh(Asset->SourceMesh);
    SurfacePreviewComponent->SetSurfaceAsset(Asset);

    if (ViewportClient)
    {
        ViewportClient->Invalidate();
    }
}

void SACFSurfaceViewport::SetSelectedBone(FName InBoneName)
{
    if (ViewportClient)
    {
        ViewportClient->SetSelectedBone(InBoneName);
        ViewportClient->Invalidate();
    }
}

void SACFSurfaceViewport::ClearSelectedBone()
{
    if (ViewportClient)
    {
        ViewportClient->ClearSelectedBone();
        ViewportClient->Invalidate();
    }
}

void SACFSurfaceViewport::SetBridgeEditMode(bool bEnabled)
{
    if (ViewportClient)
    {
        ViewportClient->SetBridgeEditMode(bEnabled);
        ViewportClient->Invalidate();
    }
}

bool SACFSurfaceViewport::IsBridgeEditModeEnabled() const
{
    return ViewportClient && ViewportClient->IsBridgeEditModeEnabled();
}

void SACFSurfaceViewport::ToggleBridgePathType()
{
    if (ViewportClient)
    {
        ViewportClient->ToggleBridgePathType();
        ViewportClient->Invalidate();
    }
}

bool SACFSurfaceViewport::IsBridgeClosedLoop() const
{
    return ViewportClient && ViewportClient->IsBridgeClosedLoop();
}

bool SACFSurfaceViewport::CanChangeBridgePathType() const
{
    return ViewportClient && ViewportClient->CanChangeBridgePathType();
}

bool SACFSurfaceViewport::CanFinishBridgePath() const
{
    return ViewportClient && ViewportClient->CanFinishBridgePath();
}

void SACFSurfaceViewport::FinishBridgePath()
{
    if (ViewportClient && ViewportClient->FinishBridgePath())
    {
        ViewportClient->Invalidate();
    }
}

bool SACFSurfaceViewport::CanUndoBridgePoint() const
{
    return ViewportClient && ViewportClient->CanUndoBridgePoint();
}

void SACFSurfaceViewport::UndoBridgePoint()
{
    if (ViewportClient)
    {
        ViewportClient->UndoBridgePoint();
        ViewportClient->Invalidate();
    }
}

void SACFSurfaceViewport::CancelBridgeEditing()
{
    if (ViewportClient)
    {
        ViewportClient->CancelBridgeEditing();
        ViewportClient->Invalidate();
    }
}

bool SACFSurfaceViewport::CanBuildBridgeDefinition() const
{
    return ViewportClient &&
        ViewportClient->CanBuildBridgeDefinition();
}

bool SACFSurfaceViewport::BuildBridgeDefinition(
    FACFBridgeDefinition& OutDefinition) const
{
    return ViewportClient &&
        ViewportClient->BuildBridgeDefinition(OutDefinition);
}

void SACFSurfaceViewport::CompleteBridgeEditing()
{
    if (ViewportClient)
    {
        ViewportClient->CompleteBridgeEditing();
        ViewportClient->Invalidate();
    }
}

bool SACFSurfaceViewport::CanFlipBridgeNormal() const
{
    return ViewportClient && ViewportClient->CanFlipBridgeNormal();
}

bool SACFSurfaceViewport::IsBridgeWindingReversed() const
{
    return ViewportClient && ViewportClient->IsBridgeWindingReversed();
}

void SACFSurfaceViewport::FlipBridgeNormal()
{
    if (ViewportClient)
    {
        ViewportClient->FlipBridgeNormal();
        ViewportClient->Invalidate();
    }
}

void SACFSurfaceViewport::SetSelectedBridgeIndex(int32 BridgeIndex)
{
    if (ViewportClient)
    {
        ViewportClient->SetSelectedBridgeIndex(BridgeIndex);
        ViewportClient->Invalidate();
    }
}

void SACFSurfaceViewport::ClearSelectedBridge()
{
    if (ViewportClient)
    {
        ViewportClient->ClearSelectedBridge();
        ViewportClient->Invalidate();
    }
}

bool SACFSurfaceViewport::FocusSelectedBridge()
{
    return ViewportClient && ViewportClient->FocusSelectedBridge();
}

FString SACFSurfaceViewport::GetBridgeInstructionText() const
{
    return ViewportClient
        ? ViewportClient->GetBridgeInstructionText()
        : TEXT("Bridge tool is unavailable.");
}

FString SACFSurfaceViewport::GetBridgeFinishActionText() const
{
    return ViewportClient
        ? ViewportClient->GetBridgeFinishActionText()
        : TEXT("Finish Path");
}

TSharedRef<FEditorViewportClient> SACFSurfaceViewport::MakeEditorViewportClient()
{
    ViewportClient = MakeShared<FACFSurfaceViewportClient>(
        *PreviewScene,
        SharedThis(this),
        Asset,
        SurfacePreviewComponent);

    return ViewportClient.ToSharedRef();
}

TSharedPtr<SWidget> SACFSurfaceViewport::BuildViewportToolbar()
{
    return SNew(SACFSurfaceViewportToolbar, SharedThis(this));
}

void SACFSurfaceViewport::PopulateViewportOverlays(TSharedRef<SOverlay> Overlay)
{
    SEditorViewport::PopulateViewportOverlays(Overlay);

    Overlay->AddSlot()
    .HAlign(HAlign_Left)
    .VAlign(VAlign_Top)
    .Padding(12.0f)
    [
        SNew(SBorder)
            .BorderImage(FAppStyle::GetBrush("WhiteBrush"))
            .BorderBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.45f))
            .Padding(FMargin(8.0f))
            [
                SAssignNew(InfoText, STextBlock)
                .Text(this, &SACFSurfaceViewport::GetInfoText)
                .ColorAndOpacity(FLinearColor::White)
            ]
    ];
}

TSharedRef<SEditorViewport> SACFSurfaceViewport::GetViewportWidget()
{
    return SharedThis(this);
}

TSharedPtr<FExtender> SACFSurfaceViewport::GetExtenders() const
{
    return MakeShared<FExtender>();
}

void SACFSurfaceViewport::OnFloatingButtonClicked()
{

}

FText SACFSurfaceViewport::GetInfoText() const
{
    if (!Asset) return FText::FromString(TEXT("No Surface"));

    const FString BridgeStatus = ViewportClient
        ? ViewportClient->GetBridgeStatusText()
        : TEXT("Bridge: Unavailable");

    return FText::FromString(FString::Printf(
        TEXT("Triangles: %d\nVertices: %d\n\n%s"),
        Asset->Triangles.Num(),
        Asset->Vertices.Num(),
        *BridgeStatus));
}
