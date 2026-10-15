// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UACFSurfaceAsset;
struct FACFReductionSettings;
struct FACFLogicalWeldSettings;
struct FACFNormalFlattenSettings;

class CLIMBINGSYSTEMEDITOR_API FACFSurfaceGenerator
{
public:
    static bool Generate(UACFSurfaceAsset* InAsset);

    static bool RebuildBridgeOverlay(
        UACFSurfaceAsset* InAsset,
        FString* OutError = nullptr);

private:
    static USkeletalMesh* CopyTransientMesh(USkeletalMesh* SourceMesh);
    static USkeletalMesh* GenerateLODMesh(USkeletalMesh* Mesh, const FACFReductionSettings& Settings);

    static bool CreateSurfaceAsset(UACFSurfaceAsset* InAsset, USkeletalMesh* InMesh);

    static void BuildLogicalVertexGroups(
        const UACFSurfaceAsset* InAsset,
        const FACFLogicalWeldSettings& Settings,
        TArray<int32>& OutVertexToLogicalGroup,
        TArray<TArray<int32>>& OutLogicalGroups);
    static void AddLogicalGroupNeighbors(
        const TArray<TArray<int32>>& LogicalGroups,
        TArray<TArray<int32>>& InOutNeighbors);

    static void BuildVertexNeighbors(
        const UACFSurfaceAsset* InAsset,
        TArray<TArray<int32>>& OutNeighbors);
    static void FlatVertexNormals(
        UACFSurfaceAsset* InAsset,
        const FACFNormalFlattenSettings& Settings,
        const TArray<TArray<int32>>& VertexNeighbors);
};
