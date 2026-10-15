// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolkits/AssetEditorToolkit.h"

class UACFSurfaceAsset;
class SACFSurfaceViewport;
class FAssetThumbnail;
class FAssetThumbnailPool;
struct FPropertyChangedEvent;
class ITableRow;
class STableViewBase;
class SVerticalBox;
template <typename ItemType> class SListView;

struct FACFBridgeListItem
{
    int32 BridgeIndex = INDEX_NONE;
    FText Label;
};

class CLIMBINGSYSTEMEDITOR_API FACFSurfaceAssetEditor : public FAssetEditorToolkit
{
public:
    virtual ~FACFSurfaceAssetEditor() override;

    void InitEditor(
        const EToolkitMode::Type Mode,
        const TSharedPtr<IToolkitHost>& InitToolkitHost,
        UACFSurfaceAsset* InAsset);

    virtual FName GetToolkitFName() const override;
    virtual FText GetBaseToolkitName() const override;
    virtual FString GetWorldCentricTabPrefix() const override;
    virtual FLinearColor GetWorldCentricTabColorScale() const override;

protected:
    virtual void RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;
    virtual void UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;

private:
    TSharedRef<SDockTab> SpawnPreviewTab(const FSpawnTabArgs& Args);
    TSharedRef<SDockTab> SpawnDetailsTab(const FSpawnTabArgs& Args);

    void ExtendToolbar();
    void FillToolbar(FToolBarBuilder& ToolbarBuilder);

    void GenerateSurface();
    FText GetBridgeToolStatusText() const;
    FText GetBridgeFinishActionText() const;
    FText GetBridgePathTypeActionText() const;

    void RefreshSectionFilter();
    void SetMaterialSlotExcluded(FName MaterialSlotName, bool bExcluded);
    FText GetSectionFilterStatusText() const;
    bool CanGenerateSurface() const;
    void OnAssetPropertyChanged(UObject* Object, FPropertyChangedEvent& PropertyChangedEvent);

    void ApplyBridge();
    void RefreshBridgeList();
    TSharedRef<ITableRow> GenerateBridgeRow(
        TSharedPtr<FACFBridgeListItem> Item,
        const TSharedRef<STableViewBase>& OwnerTable);
    void OnBridgeSelectionChanged(
        TSharedPtr<FACFBridgeListItem> Item,
        ESelectInfo::Type SelectInfo);
    FReply DeleteSelectedBridge();

private:
    TObjectPtr<UACFSurfaceAsset> EditingAsset = nullptr;

    TSharedPtr<SACFSurfaceViewport> PreviewViewport;
    TSharedPtr<IDetailsView> DetailsView;
    TSharedPtr<SVerticalBox> SectionFilterBox;
    TSharedPtr<FAssetThumbnailPool> MaterialThumbnailPool;
    TArray<TSharedPtr<FAssetThumbnail>> MaterialThumbnails;
    FDelegateHandle AssetPropertyChangedHandle;

    TArray<TSharedPtr<FACFBridgeListItem>> BridgeListItems;
    TSharedPtr<SListView<TSharedPtr<FACFBridgeListItem>>> BridgeListView;
    int32 SelectedBridgeIndex = INDEX_NONE;

    static const FName PreviewTabId;
    static const FName DetailsTabId;
};
