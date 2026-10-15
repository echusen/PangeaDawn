// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ACFSurfaceAsset.h"

struct FACFBridgeTriangleNeighbor
{
    int32 TriangleIndex = INDEX_NONE;
    int32 SharedVertexIndexA = INDEX_NONE;
    int32 SharedVertexIndexB = INDEX_NONE;
};

struct FACFBridgePathTopology
{
    TArray<TArray<FACFBridgeTriangleNeighbor>> TriangleNeighbors;
};

class CLIMBINGSYSTEMEDITOR_API FACFBridgePathExpansion
{
public:
    static bool BuildBaseSurfaceTopology(
        const UACFSurfaceAsset* Asset,
        FACFBridgePathTopology& OutTopology,
        FString* OutError = nullptr);

    static bool ExpandPath(
        const UACFSurfaceAsset* Asset,
        const FACFBridgePathTopology& Topology,
        const TArray<FACFBridgePoint>& SourcePoints,
        bool bClosedLoop,
        TArray<FACFBridgePoint>& OutExpandedPoints,
        FString* OutError = nullptr);
};
