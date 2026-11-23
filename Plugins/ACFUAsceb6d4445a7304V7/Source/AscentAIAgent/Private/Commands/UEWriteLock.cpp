// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "Commands/UEWriteLock.h"

#include "HAL/PlatformTime.h"

bool FUEWriteLock::TryAcquire(const FString& AssetPath, double CooldownSeconds)
{
    const FName Key(*AssetPath);
    const double Now = FPlatformTime::Seconds();

    FScopeLock Lock(&Mutex);

    FEntry& Entry = Entries.FindOrAdd(Key);

    if (Entry.bHeld)
    {
        return false;
    }

    if (Entry.ReleasedAtSeconds > 0.0
        && (Now - Entry.ReleasedAtSeconds) < CooldownSeconds)
    {
        return false;
    }

    Entry.bHeld = true;
    return true;
}

void FUEWriteLock::Release(const FString& AssetPath)
{
    const FName Key(*AssetPath);
    const double Now = FPlatformTime::Seconds();

    FScopeLock Lock(&Mutex);

    FEntry* Entry = Entries.Find(Key);
    if (!Entry)
    {
        return;
    }

    Entry->bHeld = false;
    Entry->ReleasedAtSeconds = Now;
}

void FUEWriteLock::Reset()
{
    FScopeLock Lock(&Mutex);
    Entries.Reset();
}
