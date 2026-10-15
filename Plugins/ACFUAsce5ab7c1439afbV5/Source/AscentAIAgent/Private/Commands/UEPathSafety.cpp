// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "Commands/UEPathSafety.h"
#include "Misc/PackageName.h"

namespace
{
    constexpr const TCHAR* ReadOnlyPrefixes[] = {
        TEXT("/Engine/"),
        TEXT("/Script/"),
        TEXT("/Plugin/"),
        TEXT("/Plugins/"),
    };
}

bool FUEPathSafety::IsReadOnlyPath(const FString& AssetPath)
{
    for (const TCHAR* Prefix : ReadOnlyPrefixes)
    {
        if (AssetPath.StartsWith(Prefix))
        {
            return true;
        }
    }
    return false;
}

bool FUEPathSafety::IsSafeAssetPath(const FString& AssetPath, FString& OutReason, bool bAllowPluginPath)
{
    if (AssetPath.IsEmpty())
    {
        OutReason = TEXT("asset path is empty");
        return false;
    }

    if (!AssetPath.StartsWith(TEXT("/")))
    {
        OutReason = FString::Printf(
            TEXT("asset path '%s' must start with '/' (e.g. '/Game/...')"),
            *AssetPath);
        return false;
    }

    // Reject traversal and doubled separators before the engine sees them.
    // CreatePackage() treats "//" as UE_LOG(Fatal), and a Fatal terminates the
    // editor outright - the user loses every unsaved change rather than getting
    // an error back. Same reasoning for "..": never hand it downstream.
    if (AssetPath.Contains(TEXT("//")) || AssetPath.Contains(TEXT("..")))
    {
        OutReason = FString::Printf(
            TEXT("asset path '%s' contains '//' or '..'"),
            *AssetPath);
        return false;
    }

    if (IsReadOnlyPath(AssetPath))
    {
        OutReason = FString::Printf(
            TEXT("asset path '%s' is read-only (engine/script/plugin content)"),
            *AssetPath);
        return false;
    }

    // Allowlist, not denylist. Every mounted plugin content root is writable -
    // including this plugin's own, since the .uplugin sets CanContainContent -
    // so naming the roots we refuse can never be complete. Require /Game unless
    // the caller explicitly opts in.
    if (!bAllowPluginPath && !AssetPath.StartsWith(TEXT("/Game/")))
    {
        OutReason = FString::Printf(
            TEXT("asset path '%s' must be under /Game/"),
            *AssetPath);
        return false;
    }

    // Strip any ".Object" suffix: an object path is not a package name.
    FString PackageName = AssetPath;
    int32 DotIndex = INDEX_NONE;
    if (PackageName.FindChar(TEXT('.'), DotIndex))
    {
        PackageName.LeftInline(DotIndex);
    }

    FText PackageReason;
    if (!FPackageName::IsValidLongPackageName(PackageName, /*bIncludeReadOnlyRoots*/ false, &PackageReason))
    {
        OutReason = FString::Printf(
            TEXT("asset path '%s' is not a valid package name: %s"),
            *AssetPath, *PackageReason.ToString());
        return false;
    }

    return true;
}
