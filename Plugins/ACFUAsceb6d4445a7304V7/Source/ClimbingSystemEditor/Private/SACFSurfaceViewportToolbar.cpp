// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "SACFSurfaceViewportToolbar.h"

#include "ACFSurfaceViewportClient.h"

#include "Framework/Commands/UIAction.h"
#include "ShowFlagMenuCommands.h"
#include "ShowFlagFilter.h"
#include "ToolMenus.h"

#define LOCTEXT_NAMESPACE "SACFSurfaceViewportToolbar"

void SACFSurfaceViewportToolbar::FillShowFlagsMenu(UToolMenu* InMenu) const
{
    FShowFlagFilter Filter(FShowFlagFilter::ExcludeAllFlagsByDefault);
    Filter.IncludeFlag(FEngineShowFlags::SF_Bones);
    Filter.IncludeFlag(FEngineShowFlags::SF_SkeletalMeshes);

    FShowFlagMenuCommands::Get().BuildShowFlagsMenu(InMenu, Filter);

    FToolMenuSection& ACFSection = InMenu->AddSection(
        "ACFFreeClimbSurface",
        LOCTEXT("ACFSurfaceShowFlags", "ACF Climb Surface"));

    ACFSection.AddMenuEntry(
        "ShowSurfaceTriangles",
        LOCTEXT("ShowSurfaceTriangles", "Surface Triangles"),
        LOCTEXT("ShowSurfaceTrianglesTooltip", "Show generated surface triangle edges."),
        FSlateIcon(),
        FUIAction(
            FExecuteAction::CreateLambda([this]()
            {
                FACFSurfaceViewportClient& ACFClient =
                    static_cast<FACFSurfaceViewportClient&>(GetViewportClient());

                ACFClient.SetShowSurfaceTriangles(!ACFClient.IsShowingSurfaceTriangles());
                ACFClient.Invalidate();
            }),
            FCanExecuteAction(),
            FIsActionChecked::CreateLambda([this]()
            {
                const FACFSurfaceViewportClient& ACFClient =
                    static_cast<const FACFSurfaceViewportClient&>(GetViewportClient());

                return ACFClient.IsShowingSurfaceTriangles();
            })),
        EUserInterfaceActionType::ToggleButton);

    ACFSection.AddMenuEntry(
        "ShowVertexNormals",
        LOCTEXT("ShowVertexNormals", "Vertex Normals"),
        LOCTEXT("ShowVertexNormalsTooltip", "Show generated surface vertex normals."),
        FSlateIcon(),
        FUIAction(
            FExecuteAction::CreateLambda([this]()
            {
                FACFSurfaceViewportClient& ACFClient =
                    static_cast<FACFSurfaceViewportClient&>(GetViewportClient());

                ACFClient.SetShowVertexNormals(!ACFClient.IsShowingVertexNormals());
                ACFClient.Invalidate();
            }),
            FCanExecuteAction(),
            FIsActionChecked::CreateLambda([this]()
            {
                const FACFSurfaceViewportClient& ACFClient =
                    static_cast<const FACFSurfaceViewportClient&>(GetViewportClient());

                return ACFClient.IsShowingVertexNormals();
            })),
        EUserInterfaceActionType::ToggleButton);
}

#undef LOCTEXT_NAMESPACE
