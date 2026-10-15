// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "SEditorViewport.h"
#include "SCommonEditorViewportToolbarBase.h"

class FAdvancedPreviewScene;
class FACFSurfaceViewportClient;
class UACFSurfaceAsset;
class UDebugSkelMeshComponent;
class UACFSurfacePreviewComponent;

struct FACFBridgeDefinition;

class CLIMBINGSYSTEMEDITOR_API SACFSurfaceViewport
    : public SEditorViewport
    , public ICommonEditorViewportToolbarInfoProvider
{
public:
    SLATE_BEGIN_ARGS(SACFSurfaceViewport) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs, UACFSurfaceAsset* InAsset);

    void RefreshPreview();

    void SetSelectedBone(FName InBoneName);
    void ClearSelectedBone();
    
    void SetBridgeEditMode(bool bEnabled);
    bool IsBridgeEditModeEnabled() const;

    void ToggleBridgePathType();
    bool IsBridgeClosedLoop() const;
    bool CanChangeBridgePathType() const;

    bool CanFinishBridgePath() const;
    void FinishBridgePath();

    bool CanUndoBridgePoint() const;
    void UndoBridgePoint();

    void CancelBridgeEditing();

    bool CanBuildBridgeDefinition() const;
    bool BuildBridgeDefinition(FACFBridgeDefinition& OutDefinition) const;
    void CompleteBridgeEditing();

    bool CanFlipBridgeNormal() const;
    bool IsBridgeWindingReversed() const;
    void FlipBridgeNormal();

    void SetSelectedBridgeIndex(int32 BridgeIndex);
    void ClearSelectedBridge();
    bool FocusSelectedBridge();

    FString GetBridgeInstructionText() const;
    FString GetBridgeFinishActionText() const;

protected:
    virtual TSharedRef<FEditorViewportClient> MakeEditorViewportClient() override;

    virtual TSharedPtr<SWidget> BuildViewportToolbar() override;

    virtual void PopulateViewportOverlays(TSharedRef<SOverlay> Overlay) override;

    virtual TSharedRef<SEditorViewport> GetViewportWidget() override;
    virtual TSharedPtr<FExtender> GetExtenders() const override;
    virtual void OnFloatingButtonClicked() override;

private:
    FText GetInfoText() const;

private:
    UACFSurfaceAsset* Asset = nullptr;

    TSharedPtr<FAdvancedPreviewScene> PreviewScene;
    TSharedPtr<FACFSurfaceViewportClient> ViewportClient;

    TObjectPtr<UDebugSkelMeshComponent> PreviewMeshComponent = nullptr;
    TObjectPtr<UACFSurfacePreviewComponent> SurfacePreviewComponent = nullptr;

    TSharedPtr<STextBlock> InfoText;

    static const FName ViewportToolbarName;
};
