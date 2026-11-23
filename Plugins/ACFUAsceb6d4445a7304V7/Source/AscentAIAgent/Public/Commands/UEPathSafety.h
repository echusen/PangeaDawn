// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Path safety checks for asset-mutating commands.
 *
 * Engine/Script/Plugin content is read-only from the agent's
 * perspective — touching it via TCP commands would corrupt vendor
 * assets. Mutators MUST call IsSafeAssetPath() before doing anything
 * destructive (create / modify / delete).
 */
class FUEPathSafety
{
public:
    /**
     * Check whether an asset path is safe to write to.
     *
     * Requires the path to be under /Game unless bAllowPluginPath is set, and
     * rejects "//" and ".." before they reach engine code that treats them as
     * Fatal (which kills the editor instead of returning an error).
     *
     * @param AssetPath        Asset path to validate (e.g. "/Game/Foo/BP_Bar").
     * @param OutReason        Filled on rejection with a human-readable reason.
     * @param bAllowPluginPath Permit a mounted plugin content root outside /Game.
     *                         Off by default: plugin content is vendor-shipped.
     * @return true if the path is writable.
     */
    static bool IsSafeAssetPath(const FString& AssetPath, FString& OutReason, bool bAllowPluginPath = false);

    /** Returns true if AssetPath is read-only (engine / script / plugin content). */
    static bool IsReadOnlyPath(const FString& AssetPath);
};
