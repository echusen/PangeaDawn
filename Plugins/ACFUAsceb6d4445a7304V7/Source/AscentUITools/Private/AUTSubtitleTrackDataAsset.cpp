// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "AUTSubtitleTrackDataAsset.h"

bool UAUTSubtitleTrackDataAsset::HasLines() const
{
    return Lines.Num() > 0;
}

int32 UAUTSubtitleTrackDataAsset::GetLineIndexAtTime(float timeSeconds) const
{
    float lineStart = InitialDelay;

    for (int32 lineIndex = 0; lineIndex < Lines.Num(); lineIndex++) {
        const float lineEnd = lineStart + FMath::Max(Lines[lineIndex].Duration, 0.f);

        if (timeSeconds >= lineStart && timeSeconds < lineEnd) {
            return lineIndex;
        }

        lineStart = lineEnd;
    }

    return INDEX_NONE;
}

bool UAUTSubtitleTrackDataAsset::GetLineAtTime(float timeSeconds, FAUTSubtitleLine& outLine) const
{
    const int32 lineIndex = GetLineIndexAtTime(timeSeconds);

    if (!Lines.IsValidIndex(lineIndex)) {
        return false;
    }

    outLine = Lines[lineIndex];
    return true;
}

float UAUTSubtitleTrackDataAsset::GetTotalDuration() const
{
    float totalDuration = InitialDelay;

    for (const FAUTSubtitleLine& subtitleLine : Lines) {
        totalDuration += FMath::Max(subtitleLine.Duration, 0.f);
    }

    return totalDuration;
}
