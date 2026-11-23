// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "ACFSurfaceAssetEditor.h"

#include "ClimbingSystemEditor.h"
#include "ACFSurfaceAsset.h"
#include "ACFSurfaceGenerator.h"
#include "SACFSurfaceViewport.h"

#include "AssetThumbnail.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "IDetailsView.h"
#include "Materials/MaterialInterface.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "Styling/AppStyle.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "ScopedTransaction.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"

const FName FACFSurfaceAssetEditor::PreviewTabId(TEXT("ACFSurfaceAssetEditor_Preview"));
const FName FACFSurfaceAssetEditor::DetailsTabId(TEXT("ACFSurfaceAssetEditor_Details"));

FACFSurfaceAssetEditor::~FACFSurfaceAssetEditor()
{
    if (AssetPropertyChangedHandle.IsValid())
    {
        FCoreUObjectDelegates::OnObjectPropertyChanged.Remove(AssetPropertyChangedHandle);
    }
}

void FACFSurfaceAssetEditor::InitEditor(
    const EToolkitMode::Type Mode,
    const TSharedPtr<IToolkitHost>& InitToolkitHost,
    UACFSurfaceAsset* InAsset)
{
    EditingAsset = InAsset;

    FPropertyEditorModule& PropertyEditorModule =
        FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

    FDetailsViewArgs DetailsArgs;
    DetailsArgs.bHideSelectionTip = true;

    DetailsView = PropertyEditorModule.CreateDetailView(DetailsArgs);
    DetailsView->SetObject(EditingAsset);
    MaterialThumbnailPool = MakeShared<FAssetThumbnailPool>(32);
    AssetPropertyChangedHandle = FCoreUObjectDelegates::OnObjectPropertyChanged.AddRaw(
        this,
        &FACFSurfaceAssetEditor::OnAssetPropertyChanged);
    RefreshBridgeList();

    const TSharedRef<FTabManager::FLayout> Layout =
        FTabManager::NewLayout("ACFSurfaceAssetEditor_Layout_v3")
        ->AddArea
        (
            FTabManager::NewPrimaryArea()
            ->SetOrientation(Orient_Horizontal)
            ->Split 
            (
                FTabManager::NewStack()
                ->AddTab(PreviewTabId, ETabState::OpenedTab)
                ->SetHideTabWell(true)
                ->SetSizeCoefficient(0.75f)
            )
            ->Split
            (
                FTabManager::NewStack()
                ->AddTab(DetailsTabId, ETabState::OpenedTab)
                ->SetSizeCoefficient(0.25f)
            )
        );

    InitAssetEditor(
        Mode,
        InitToolkitHost,
        TEXT("ACFSurfaceAssetEditor"),
        Layout,
        true,
        true,
        InAsset);

    ExtendToolbar();

    RegenerateMenusAndToolbars();
}

FName FACFSurfaceAssetEditor::GetToolkitFName() const
{
    return FName("ACFSurfaceAssetEditor");
}

FText FACFSurfaceAssetEditor::GetBaseToolkitName() const
{
    return FText::FromString("ACF Climb Surface Asset");
}

FString FACFSurfaceAssetEditor::GetWorldCentricTabPrefix() const
{
    return TEXT("ACF Climb Surface");
}

FLinearColor FACFSurfaceAssetEditor::GetWorldCentricTabColorScale() const
{
    return FLinearColor(0.2f, 0.6f, 1.0f);
}

void FACFSurfaceAssetEditor::RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
    FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

    InTabManager->RegisterTabSpawner(
        PreviewTabId,
        FOnSpawnTab::CreateRaw(this, &FACFSurfaceAssetEditor::SpawnPreviewTab))
        .SetDisplayName(FText::FromString("Viewport"))
        .SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Viewports"));

    InTabManager->RegisterTabSpawner(
        DetailsTabId,
        FOnSpawnTab::CreateRaw(this, &FACFSurfaceAssetEditor::SpawnDetailsTab))
        .SetDisplayName(FText::FromString("Details"))
        .SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"));

}

void FACFSurfaceAssetEditor::UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
    InTabManager->UnregisterTabSpawner(PreviewTabId);
    InTabManager->UnregisterTabSpawner(DetailsTabId);

    FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);
}

TSharedRef<SDockTab> FACFSurfaceAssetEditor::SpawnPreviewTab(const FSpawnTabArgs& Args)
{
    return SNew(SDockTab)
        [
            SAssignNew(PreviewViewport, SACFSurfaceViewport, EditingAsset)
        ];
}

TSharedRef<SDockTab> FACFSurfaceAssetEditor::SpawnDetailsTab(const FSpawnTabArgs& Args)
{
    TSharedRef<SDockTab> DetailsTab = SNew(SDockTab)
        [
            SNew(SVerticalBox)

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(4.0f)
            [
                SNew(SBorder)
                .BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
                .Padding(8.0f)
                [
                    SNew(SVerticalBox)

                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(TEXT("Surface Section Filter")))
                        .Font(FAppStyle::GetFontStyle("BoldFont"))
                    ]

                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, 4.0f)
                    [
                        SNew(STextBlock)
                        .Text_Lambda([this]() { return GetSectionFilterStatusText(); })
                        .AutoWrapText(true)
                    ]

                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(TEXT("Filters use Material Slot names. Regenerating can require existing bridges to be rebuilt.")))
                        .AutoWrapText(true)
                        .ColorAndOpacity(FLinearColor(0.75f, 0.75f, 0.75f))
                    ]

                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        SAssignNew(SectionFilterBox, SVerticalBox)
                    ]

                ]
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(4.0f)
            [
                SNew(SBorder)
                .BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
                .Padding(8.0f)
                [
                    SNew(SVerticalBox)

                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(TEXT("Bridge Tool")))
                        .Font(FAppStyle::GetFontStyle("BoldFont"))
                    ]

                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, 4.0f)
                    [
                        SNew(STextBlock)
                        .Text_Lambda([this]() { return GetBridgeToolStatusText(); })
                        .AutoWrapText(true)
                    ]

                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, 4.0f, 0.0f, 0.0f)
                    [
                        SNew(SButton)
                        .Text(FText::FromString(TEXT("Create Bridge")))
                        .IsEnabled_Lambda([this]()
                        {
                            return PreviewViewport.IsValid() &&
                                !PreviewViewport->IsBridgeEditModeEnabled();
                        })
                        .OnClicked_Lambda([this]()
                        {
                            if (PreviewViewport.IsValid())
                            {
                                PreviewViewport->SetBridgeEditMode(true);
                            }
                            return FReply::Handled();
                        })
                    ]

                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, 4.0f, 0.0f, 0.0f)
                    [
                        SNew(SButton)
                        .Text_Lambda([this]() { return GetBridgePathTypeActionText(); })
                        .ToolTipText(FText::FromString(TEXT("Choose an open path or closed loop before adding points.")))
                        .IsEnabled_Lambda([this]()
                        {
                            return PreviewViewport.IsValid() &&
                                PreviewViewport->CanChangeBridgePathType();
                        })
                        .OnClicked_Lambda([this]()
                        {
                            if (PreviewViewport.IsValid())
                            {
                                PreviewViewport->ToggleBridgePathType();
                            }
                            return FReply::Handled();
                        })
                    ]

                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, 4.0f, 0.0f, 0.0f)
                    [
                        SNew(SButton)
                        .Text_Lambda([this]() { return GetBridgeFinishActionText(); })
                        .ToolTipText(FText::FromString(TEXT("Complete the active bridge path.")))
                        .IsEnabled_Lambda([this]()
                        {
                            return PreviewViewport.IsValid() &&
                                PreviewViewport->CanFinishBridgePath();
                        })
                        .OnClicked_Lambda([this]()
                        {
                            if (PreviewViewport.IsValid())
                            {
                                PreviewViewport->FinishBridgePath();
                            }
                            return FReply::Handled();
                        })
                    ]

                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, 4.0f, 0.0f, 0.0f)
                    [
                        SNew(SButton)
                        .Text(FText::FromString(TEXT("Undo Point")))
                        .ToolTipText(FText::FromString(TEXT("Remove the last point from the active path.")))
                        .IsEnabled_Lambda([this]()
                        {
                            return PreviewViewport.IsValid() &&
                                PreviewViewport->CanUndoBridgePoint();
                        })
                        .OnClicked_Lambda([this]()
                        {
                            if (PreviewViewport.IsValid())
                            {
                                PreviewViewport->UndoBridgePoint();
                            }
                            return FReply::Handled();
                        })
                    ]

                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, 8.0f, 0.0f, 0.0f)
                    [
                        SNew(SButton)
                        .Text_Lambda([this]()
                        {
                            return FText::FromString(
                                PreviewViewport.IsValid() && PreviewViewport->IsBridgeWindingReversed()
                                ? TEXT("Normal: Reversed")
                                : TEXT("Flip Bridge Normal"));
                        })
                        .ToolTipText(FText::FromString(TEXT("Reverse the bridge triangle winding in preview.")))
                        .IsEnabled_Lambda([this]()
                        {
                            return PreviewViewport.IsValid() &&
                                PreviewViewport->CanFlipBridgeNormal();
                        })
                        .OnClicked_Lambda([this]()
                        {
                            if (PreviewViewport.IsValid())
                            {
                                PreviewViewport->FlipBridgeNormal();
                            }
                            return FReply::Handled();
                        })
                    ]

                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, 4.0f, 0.0f, 0.0f)
                    [
                        SNew(SButton)
                        .Text(FText::FromString(TEXT("Save Bridge")))
                        .ToolTipText(FText::FromString(TEXT("Save this bridge and rebuild climbing surface data.")))
                        .IsEnabled_Lambda([this]()
                        {
                            return PreviewViewport.IsValid() &&
                                PreviewViewport->CanBuildBridgeDefinition();
                        })
                        .OnClicked_Lambda([this]()
                        {
                            ApplyBridge();
                            return FReply::Handled();
                        })
                    ]

                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, 4.0f, 0.0f, 0.0f)
                    [
                        SNew(SButton)
                        .Text(FText::FromString(TEXT("Discard Bridge")))
                        .ToolTipText(FText::FromString(TEXT("Discard all temporary bridge paths.")))
                        .IsEnabled_Lambda([this]()
                        {
                            return PreviewViewport.IsValid() &&
                                PreviewViewport->IsBridgeEditModeEnabled();
                        })
                        .OnClicked_Lambda([this]()
                        {
                            if (PreviewViewport.IsValid())
                            {
                                PreviewViewport->CancelBridgeEditing();
                            }
                            return FReply::Handled();
                        })
                    ]
                ]
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(4.0f)
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("Bridge Definitions")))
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(4.0f)
            [
                SAssignNew(BridgeListView, SListView<TSharedPtr<FACFBridgeListItem>>)
                .ListItemsSource(&BridgeListItems)
                .SelectionMode(ESelectionMode::Single)
                .OnGenerateRow_Raw(this, &FACFSurfaceAssetEditor::GenerateBridgeRow)
                .OnSelectionChanged_Raw(this, &FACFSurfaceAssetEditor::OnBridgeSelectionChanged)
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(4.0f, 0.0f, 4.0f, 4.0f)
            [
                SNew(SButton)
                .Text(FText::FromString(TEXT("Delete Selected Bridge")))
                .IsEnabled_Lambda([this]()
                {
                    return EditingAsset &&
                        EditingAsset->BridgeDefinitions.IsValidIndex(SelectedBridgeIndex);
                })
                .OnClicked_Raw(this, &FACFSurfaceAssetEditor::DeleteSelectedBridge)
            ]

            + SVerticalBox::Slot()
            .FillHeight(1.0f)
            [
                DetailsView.ToSharedRef()
            ]
        ];

    RefreshSectionFilter();
    return DetailsTab;
}

void FACFSurfaceAssetEditor::ExtendToolbar()
{
    TSharedPtr<FExtender> ToolbarExtender = MakeShared<FExtender>();

    ToolbarExtender->AddToolBarExtension(
        "Asset",
        EExtensionHook::After,
        GetToolkitCommands(),
        FToolBarExtensionDelegate::CreateRaw(
            this,
            &FACFSurfaceAssetEditor::FillToolbar));

    AddToolbarExtender(ToolbarExtender);
}

void FACFSurfaceAssetEditor::FillToolbar(FToolBarBuilder& ToolbarBuilder)
{
    ToolbarBuilder.AddToolBarButton(
        FUIAction(
            FExecuteAction::CreateRaw(this, &FACFSurfaceAssetEditor::GenerateSurface),
            FCanExecuteAction::CreateRaw(this, &FACFSurfaceAssetEditor::CanGenerateSurface)),
        NAME_None,
        FText::FromString("Generate"),
        FText::FromString("Generate surface data"),
        FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Plus")
    );

    ToolbarBuilder.AddSeparator();

    ToolbarBuilder.AddToolBarButton(
        FUIAction(
            FExecuteAction::CreateLambda([this]()
            {
                if (PreviewViewport.IsValid())
                {
                    PreviewViewport->SetBridgeEditMode(true);
                }
            }),
            FCanExecuteAction::CreateLambda([this]()
            {
                return PreviewViewport.IsValid() &&
                    !PreviewViewport->IsBridgeEditModeEnabled();
            })),
        NAME_None,
        FText::FromString(TEXT("Create Bridge")),
        FText::FromString(TEXT("Start a bridge workflow. Controls appear in the Bridge Tool panel.")),
        FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Plus"));
}

FText FACFSurfaceAssetEditor::GetBridgeToolStatusText() const
{
    if (!PreviewViewport.IsValid())
    {
        return FText::FromString(TEXT("Bridge tool is unavailable."));
    }

    return FText::FromString(PreviewViewport->GetBridgeInstructionText());
}

FText FACFSurfaceAssetEditor::GetBridgeFinishActionText() const
{
    if (!PreviewViewport.IsValid())
    {
        return FText::FromString(TEXT("Finish Path"));
    }

    return FText::FromString(PreviewViewport->GetBridgeFinishActionText());
}

FText FACFSurfaceAssetEditor::GetBridgePathTypeActionText() const
{
    const bool bIsClosed = PreviewViewport.IsValid() && PreviewViewport->IsBridgeClosedLoop();
    return FText::FromString(bIsClosed
        ? TEXT("Path: Closed Loop")
        : TEXT("Path: Open"));
}

void FACFSurfaceAssetEditor::RefreshSectionFilter()
{
    if (!SectionFilterBox.IsValid())
    {
        return;
    }

    SectionFilterBox->ClearChildren();
    MaterialThumbnails.Reset();

    if (!EditingAsset || !EditingAsset->SourceMesh)
    {
        SectionFilterBox->AddSlot()
        .AutoHeight()
        [
            SNew(STextBlock)
            .Text(FText::FromString(TEXT("Assign a Source Mesh to choose material slots.")))
        ];
        return;
    }

    TSet<FName> AddedMaterialSlots;

    for (const FSkeletalMaterial& Material : EditingAsset->SourceMesh->GetMaterials())
    {
        const FName MaterialSlotName = Material.MaterialSlotName;
        if (MaterialSlotName.IsNone() || AddedMaterialSlots.Contains(MaterialSlotName))
        {
            continue;
        }

        AddedMaterialSlots.Add(MaterialSlotName);

        TSharedPtr<SWidget> MaterialPreviewWidget = SNew(STextBlock)
            .Text(FText::FromString(TEXT("No Material")));

        if (UMaterialInterface* MaterialInterface = Material.MaterialInterface)
        {
            TSharedPtr<FAssetThumbnail> Thumbnail = MakeShared<FAssetThumbnail>(
                MaterialInterface,
                32,
                32,
                MaterialThumbnailPool);
            MaterialThumbnails.Add(Thumbnail);
            MaterialPreviewWidget = Thumbnail->MakeThumbnailWidget();
        }

        SectionFilterBox->AddSlot()
        .AutoHeight()
        .Padding(0.0f, 2.0f)
        [
            SNew(SCheckBox)
            .ToolTipText(FText::FromString(TEXT("Exclude every generated render section using this material slot.")))
            .IsChecked_Lambda([this, MaterialSlotName]()
            {
                return EditingAsset &&
                    EditingAsset->ExcludedMaterialSlotNames.Contains(MaterialSlotName)
                    ? ECheckBoxState::Checked
                    : ECheckBoxState::Unchecked;
            })
            .OnCheckStateChanged_Lambda([this, MaterialSlotName](ECheckBoxState CheckState)
            {
                SetMaterialSlotExcluded(MaterialSlotName, CheckState == ECheckBoxState::Checked);
            })
            [
                SNew(SHorizontalBox)

                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                [
                    MaterialPreviewWidget.ToSharedRef()
                ]

                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                .VAlign(VAlign_Center)
                .Padding(6.0f, 0.0f)
                [
                    SNew(SVerticalBox)

                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        SNew(STextBlock)
                        .Text(FText::FromName(MaterialSlotName))
                    ]

                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(Material.MaterialInterface
                            ? Material.MaterialInterface->GetName()
                            : TEXT("No Material")))
                        .ColorAndOpacity(FLinearColor(0.7f, 0.7f, 0.7f))
                    ]
                ]
            ]
        ];
    }

    if (AddedMaterialSlots.IsEmpty())
    {
        SectionFilterBox->AddSlot()
        .AutoHeight()
        [
            SNew(STextBlock)
            .Text(FText::FromString(TEXT("The Source Mesh has no named material slots.")))
        ];
    }
}

void FACFSurfaceAssetEditor::SetMaterialSlotExcluded(
    FName MaterialSlotName,
    bool bExcluded)
{
    if (!EditingAsset || MaterialSlotName.IsNone())
    {
        return;
    }

    const bool bAlreadyExcluded = EditingAsset->ExcludedMaterialSlotNames.Contains(MaterialSlotName);
    if (bAlreadyExcluded == bExcluded)
    {
        return;
    }

    const FScopedTransaction Transaction(FText::FromString(TEXT("Set ACF Climb Surface Material Slot Exclusion")));
    EditingAsset->Modify();

    if (bExcluded)
    {
        EditingAsset->ExcludedMaterialSlotNames.Add(MaterialSlotName);
    }
    else
    {
        EditingAsset->ExcludedMaterialSlotNames.Remove(MaterialSlotName);
    }

    EditingAsset->MarkPackageDirty();
}

FText FACFSurfaceAssetEditor::GetSectionFilterStatusText() const
{
    if (!EditingAsset || !EditingAsset->SourceMesh)
    {
        return FText::FromString(TEXT("Select material slots to exclude from ACF Climb Surface generation."));
    }

    int32 MaterialSlotCount = 0;
    int32 ExcludedSlotCount = 0;
    TSet<FName> SeenMaterialSlots;

    for (const FSkeletalMaterial& Material : EditingAsset->SourceMesh->GetMaterials())
    {
        const FName MaterialSlotName = Material.MaterialSlotName;
        if (MaterialSlotName.IsNone() || SeenMaterialSlots.Contains(MaterialSlotName))
        {
            continue;
        }

        SeenMaterialSlots.Add(MaterialSlotName);
        ++MaterialSlotCount;

        if (EditingAsset->ExcludedMaterialSlotNames.Contains(MaterialSlotName))
        {
            ++ExcludedSlotCount;
        }
    }

    if (MaterialSlotCount > 0 && ExcludedSlotCount == MaterialSlotCount)
    {
        return FText::FromString(TEXT("All material slots are excluded. Clear at least one slot before generating."));
    }

    return ExcludedSlotCount > 0
        ? FText::FromString(FString::Printf(
            TEXT("%d material slot(s) excluded. Generate Surface to apply the filter."),
            ExcludedSlotCount))
        : FText::FromString(TEXT("Checked slots are excluded from generation. Generate Surface to apply changes."));
}

bool FACFSurfaceAssetEditor::CanGenerateSurface() const
{
    if (!EditingAsset || !EditingAsset->SourceMesh)
    {
        return false;
    }

    bool bHasNamedMaterialSlot = false;

    for (const FSkeletalMaterial& Material : EditingAsset->SourceMesh->GetMaterials())
    {
        const FName MaterialSlotName = Material.MaterialSlotName;
        if (MaterialSlotName.IsNone())
        {
            continue;
        }

        bHasNamedMaterialSlot = true;
        if (!EditingAsset->ExcludedMaterialSlotNames.Contains(MaterialSlotName))
        {
            return true;
        }
    }

    return !bHasNamedMaterialSlot;
}

void FACFSurfaceAssetEditor::OnAssetPropertyChanged(
    UObject* Object,
    FPropertyChangedEvent& PropertyChangedEvent)
{
    if (Object != EditingAsset ||
        PropertyChangedEvent.GetPropertyName() != GET_MEMBER_NAME_CHECKED(UACFSurfaceAsset, SourceMesh))
    {
        return;
    }

    RefreshSectionFilter();
}

void FACFSurfaceAssetEditor::GenerateSurface()
{
    if (!CanGenerateSurface()) return;

    EditingAsset->Modify();

    if (!FACFSurfaceGenerator::Generate(EditingAsset))
    {
        UE_LOG(LogACFFreeClimbEditor, Warning, TEXT("Failed to generate surface for asset: %s"), *EditingAsset->GetName());
    }

    DetailsView->ForceRefresh();
    RefreshSectionFilter();
    RefreshBridgeList();

    if (PreviewViewport.IsValid())
    {
        PreviewViewport->RefreshPreview();
    }

    EditingAsset->MarkPackageDirty();
}

void FACFSurfaceAssetEditor::ApplyBridge()
{
    if (!EditingAsset || !PreviewViewport.IsValid())
    {
        return;
    }

    FACFBridgeDefinition Definition;

    if (!PreviewViewport->BuildBridgeDefinition(Definition))
    {
        return;
    }

    Definition.BaseSurfaceTopologyHash = EditingAsset->BaseSurfaceTopologyHash;

    const FScopedTransaction Transaction(FText::FromString(TEXT("Apply ACF Free Climb Bridge")));

    EditingAsset->Modify();

    const int32 DefinitionIndex = EditingAsset->BridgeDefinitions.Add(MoveTemp(Definition));

    FString Error;

    if (!FACFSurfaceGenerator::RebuildBridgeOverlay(EditingAsset, &Error))
    {
        EditingAsset->BridgeDefinitions.RemoveAt(DefinitionIndex);

        FACFSurfaceGenerator::RebuildBridgeOverlay(EditingAsset);

        UE_LOG(LogACFFreeClimbEditor, Warning, TEXT("Failed to apply bridge: %s"), *Error);

        return;
    }

    EditingAsset->MarkPackageDirty();

    PreviewViewport->CompleteBridgeEditing();
    PreviewViewport->SetSelectedBridgeIndex(DefinitionIndex);
    PreviewViewport->RefreshPreview();

    SelectedBridgeIndex = DefinitionIndex;
    RefreshBridgeList();

    if (DetailsView.IsValid())
    {
        DetailsView->ForceRefresh();
    }
}

void FACFSurfaceAssetEditor::RefreshBridgeList()
{
    BridgeListItems.Reset();

    if (EditingAsset)
    {
        for (int32 BridgeIndex = 0; BridgeIndex < EditingAsset->BridgeDefinitions.Num(); ++BridgeIndex)
        {
            const FACFBridgeDefinition& Definition = EditingAsset->BridgeDefinitions[BridgeIndex];
            const FString PathType = Definition.PathType == EACFBridgePathType::ClosedLoop
                ? TEXT("Closed Loop")
                : TEXT("Open Spline");

            TSharedPtr<FACFBridgeListItem> Item = MakeShared<FACFBridgeListItem>();
            Item->BridgeIndex = BridgeIndex;
            Item->Label = FText::FromString(FString::Printf(
                TEXT("Bridge %d  (%s, %d / %d points)"),
                BridgeIndex + 1,
                *PathType,
                Definition.FirstPathPoints.Num(),
                Definition.SecondPathPoints.Num()));
            BridgeListItems.Add(MoveTemp(Item));
        }
    }

    if (!BridgeListItems.IsValidIndex(SelectedBridgeIndex))
    {
        SelectedBridgeIndex = INDEX_NONE;
    }

    if (BridgeListView.IsValid())
    {
        BridgeListView->RequestListRefresh();

        if (BridgeListItems.IsValidIndex(SelectedBridgeIndex))
        {
            BridgeListView->SetSelection(BridgeListItems[SelectedBridgeIndex]);
        }
        else
        {
            BridgeListView->ClearSelection();
        }
    }
}

TSharedRef<ITableRow> FACFSurfaceAssetEditor::GenerateBridgeRow(
    TSharedPtr<FACFBridgeListItem> Item,
    const TSharedRef<STableViewBase>& OwnerTable)
{
    return SNew(STableRow<TSharedPtr<FACFBridgeListItem>>, OwnerTable)
        [
            SNew(STextBlock)
            .Text(Item.IsValid() ? Item->Label : FText::GetEmpty())
        ];
}

void FACFSurfaceAssetEditor::OnBridgeSelectionChanged(
    TSharedPtr<FACFBridgeListItem> Item,
    ESelectInfo::Type SelectInfo)
{
    SelectedBridgeIndex = Item.IsValid() ? Item->BridgeIndex : INDEX_NONE;

    if (!PreviewViewport.IsValid())
    {
        return;
    }

    if (SelectedBridgeIndex == INDEX_NONE)
    {
        PreviewViewport->ClearSelectedBridge();
        return;
    }

    PreviewViewport->SetSelectedBridgeIndex(SelectedBridgeIndex);
    PreviewViewport->FocusSelectedBridge();
}

FReply FACFSurfaceAssetEditor::DeleteSelectedBridge()
{
    if (!EditingAsset || !EditingAsset->BridgeDefinitions.IsValidIndex(SelectedBridgeIndex))
    {
        return FReply::Handled();
    }

    const int32 DeletedBridgeIndex = SelectedBridgeIndex;
    const FScopedTransaction Transaction(FText::FromString(TEXT("Delete ACF Free Climb Bridge")));
    EditingAsset->Modify();

    const FACFBridgeDefinition DeletedDefinition =
        EditingAsset->BridgeDefinitions[DeletedBridgeIndex];
    EditingAsset->BridgeDefinitions.RemoveAt(DeletedBridgeIndex);

    FString Error;
    if (!FACFSurfaceGenerator::RebuildBridgeOverlay(EditingAsset, &Error))
    {
        EditingAsset->BridgeDefinitions.Insert(DeletedDefinition, DeletedBridgeIndex);
        FACFSurfaceGenerator::RebuildBridgeOverlay(EditingAsset);
        UE_LOG(LogACFFreeClimbEditor, Warning, TEXT("Failed to delete bridge: %s"), *Error);
        return FReply::Handled();
    }

    EditingAsset->MarkPackageDirty();
    SelectedBridgeIndex = INDEX_NONE;

    if (PreviewViewport.IsValid())
    {
        PreviewViewport->ClearSelectedBridge();
        PreviewViewport->RefreshPreview();
    }

    RefreshBridgeList();

    if (DetailsView.IsValid())
    {
        DetailsView->ForceRefresh();
    }

    return FReply::Handled();
}
