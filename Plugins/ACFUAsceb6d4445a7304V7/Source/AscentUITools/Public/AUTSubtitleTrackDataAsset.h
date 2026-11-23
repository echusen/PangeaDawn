// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

#include "AUTSubtitleTrackDataAsset.generated.h"

/**
 * One subtitle line of a prerendered movie, with the time it stays on screen.
 * Durations are authored by hand: a prerendered movie exposes no per line timing, so nothing can compute them.
 */
USTRUCT(BlueprintType)
struct FAUTSubtitleLine {
    GENERATED_BODY()

    /** Text displayed while this line is on screen. It is an FText so the localization pipeline gathers and translates it. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = AUT, meta = (MultiLine = true))
    FText Line;

    /** Seconds this line stays on screen before the next one replaces it. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = AUT, meta = (ClampMin = "0.0", UIMin = "0.0"))
    float Duration = 3.f;

    /** Who is speaking, displayed above the line in movies with more than one voice. Leave it empty to hide it. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = AUT)
    FText SpeakerName;
};

/**
 * Subtitle track of a prerendered movie: the lines in playback order and how long each one lasts.
 * The widget showing the track accumulates its own time and asks this asset which line belongs to that time,
 * so a track is just data and can be filled in by a writer without touching any graph or Blueprint.
 */
UCLASS()
class ASCENTUITOOLS_API UAUTSubtitleTrackDataAsset : public UPrimaryDataAsset {
    GENERATED_BODY()

public:
    /** Lines in playback order. The first one appears once InitialDelay has elapsed. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = AUT, meta = (TitleProperty = "Line"))
    TArray<FAUTSubtitleLine> Lines;

    /** Seconds before the first line appears, for the black or the logo opening the movie. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = AUT, meta = (ClampMin = "0.0", UIMin = "0.0"))
    float InitialDelay = 0.f;

    /** Returns true when there is at least one line to display. */
    UFUNCTION(BlueprintPure, Category = AUT)
    bool HasLines() const;

    /** Index of the line covering the given time in seconds, or INDEX_NONE while before the first line or after the last one. */
    UFUNCTION(BlueprintPure, Category = AUT)
    int32 GetLineIndexAtTime(float timeSeconds) const;

    /** Line covering the given time in seconds. Returns false while no line is on screen, leaving outLine untouched. */
    UFUNCTION(BlueprintPure, Category = AUT)
    bool GetLineAtTime(float timeSeconds, FAUTSubtitleLine& outLine) const;

    /** Initial delay plus every line duration, in seconds. */
    UFUNCTION(BlueprintPure, Category = AUT)
    float GetTotalDuration() const;
};
