// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/CriticalSection.h"

/**
 * Per-asset write-lock guard.
 *
 * Prevents the same asset from being mutated twice in rapid succession
 * (which can happen when an LLM retries a tool call after a partial
 * result). When TryAcquire returns true, the caller has the lock until
 * Release is called or the cooldown window elapses.
 *
 * Designed to be a member of UAscentAIAgentSubsystem (one instance per
 * editor session). Thread-safe.
 */
class FUEWriteLock
{
public:
    FUEWriteLock() = default;

    FUEWriteLock(const FUEWriteLock&) = delete;
    FUEWriteLock& operator=(const FUEWriteLock&) = delete;

    /**
     * Try to acquire the write-lock for an asset.
     *
     * @param AssetPath        Asset path to lock.
     * @param CooldownSeconds  Time after Release before another acquire succeeds.
     * @return true if acquired (caller may proceed); false if locked or in cooldown.
     */
    bool TryAcquire(const FString& AssetPath, double CooldownSeconds = 0.5);

    /** Release the lock — Cooldown still applies after release. */
    void Release(const FString& AssetPath);

    /** Drop all entries (call on Deinitialize). */
    void Reset();

private:
    struct FEntry
    {
        bool bHeld = false;
        double ReleasedAtSeconds = 0.0;
    };

    TMap<FName, FEntry> Entries;
    FCriticalSection Mutex;
};
