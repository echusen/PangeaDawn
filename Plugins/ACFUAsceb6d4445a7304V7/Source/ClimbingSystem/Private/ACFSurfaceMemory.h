// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"

class UACFSurfaceAsset;
struct FACFSurfaceRaycastRuntimeCache;

DECLARE_STATS_GROUP(TEXT("ACF|FreeClimb"), STATGROUP_ClimbingSystem, STATCAT_Advanced);
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Climbables"), STAT_ClimbingSystemClimbableCount, STATGROUP_ClimbingSystem, CLIMBINGSYSTEM_API);
DECLARE_MEMORY_STAT_EXTERN(TEXT("Total Memory"), STAT_ClimbingSystemTotalMemory, STATGROUP_ClimbingSystem, CLIMBINGSYSTEM_API);
DECLARE_MEMORY_STAT_EXTERN(TEXT("Mesh Data"), STAT_ClimbingSystemMeshDataMemory, STATGROUP_ClimbingSystem, CLIMBINGSYSTEM_API);
DECLARE_MEMORY_STAT_EXTERN(TEXT("Collision Data"), STAT_ClimbingSystemCollisionDataMemory, STATGROUP_ClimbingSystem, CLIMBINGSYSTEM_API);
DECLARE_MEMORY_STAT_EXTERN(TEXT("Runtime Data"), STAT_ClimbingSystemRuntimeCacheMemory, STATGROUP_ClimbingSystem, CLIMBINGSYSTEM_API);

namespace ACFSurfaceMemory
{
	SIZE_T GetSurfaceAssetAllocatedSize(const UACFSurfaceAsset* SurfaceAsset);
	SIZE_T GetRaycastDataAllocatedSize(const UACFSurfaceAsset* SurfaceAsset);
	SIZE_T GetRuntimeCacheAllocatedSize(const FACFSurfaceRaycastRuntimeCache& RuntimeCache);
}
