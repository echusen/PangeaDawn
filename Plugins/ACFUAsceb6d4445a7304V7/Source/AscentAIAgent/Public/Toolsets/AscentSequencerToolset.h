// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"

#include "AscentSequencerToolset.generated.h"

class FJsonObject;
class ULevelSequence;
class UMovieSceneAudioSection;
class UMovieSceneEventTriggerSection;
class UMovieSceneSection;
class UMovieSceneSequence;
class UMovieSceneSubSection;
class USoundBase;

/// Sequencer tools that Epic's AnimationAssistantToolset does not cover: the sequence a subsequence or shot section plays,
/// the sound of an audio section, event keys that call a function through the Director Blueprint, and Movie Render Queue renders.
/// Sections come from Epic's Sequencer tools (add_section, get_sections, add_event_trigger_section). Frames are display rate
/// frames, the same unit those tools use.
/// All tools return a JSON string: {"status":"success","success":true,...}
/// on success or {"status":"error","success":false,"error":...} on failure.
UCLASS(BlueprintType, Hidden)
class ASCENTAIAGENT_API UAscentSequencerToolset : public UToolsetDefinition
{
    GENERATED_BODY()

public:
    /**
     * Sets the sequence a subsequence or shot section plays, and resizes the section to that sequence's playback range.
     * @param Section Section of a Subsequence or Shots track, e.g. from add_section.
     * @param Sequence Sequence to play, e.g. "/Game/Cinematics/LS_Shot010.LS_Shot010". Cannot be the sequence that owns the section.
     * @return JSON with the sequence path and the section's new "start" and "end" frames.
     */
    UFUNCTION(meta = (AICallable, MutatingTool), Category = "ACF")
    static FString SetSubSequence(UMovieSceneSubSection* Section, UMovieSceneSequence* Sequence);

    /**
     * Sets the sound an audio section plays, and resizes the section to the sound's duration unless the sound loops.
     * @param Section Section of an Audio track, e.g. from add_section.
     * @param Sound Sound Wave or Sound Cue, e.g. "/Game/Audio/SW_Intro.SW_Intro".
     * @return JSON with the sound path, its duration in seconds, and the section's "start" and "end" frames.
     */
    UFUNCTION(meta = (AICallable, MutatingTool), Category = "ACF")
    static FString SetAudioSound(UMovieSceneAudioSection* Section, USoundBase* Sound);

    /**
     * Adds a key to an event trigger section that calls a function on the object the event track is bound to. Creates the
     * endpoint in the sequence's Director Blueprint and compiles it. The event track must be on a binding
     * (add_track_to_binding), not on the sequence root.
     * @param Section Event trigger section, from add_event_trigger_section.
     * @param Frame Frame of the key. The section grows to include it.
     * @param FunctionName Function on the bound object's class, e.g. "K2_DestroyActor" or a Blueprint function name.
     * @param Payload Function input name to value, in Unreal import-text form, e.g. {"NewScale3D":"(X=2,Y=2,Z=2)"}.
     *   Inputs left out keep their default value. Pass an empty object when the function has no inputs.
     * @param bCallInEditor True to also fire the event while scrubbing in the Editor.
     * @return JSON with the frame, the endpoint name, the Director Blueprint path, and the function's "payload_names".
     */
    UFUNCTION(meta = (AICallable, MutatingTool), Category = "ACF")
    static FString AddEventKey(UMovieSceneEventTriggerSection* Section, int32 Frame, const FString& FunctionName,
        const TMap<FString, FString>& Payload, bool bCallInEditor);

    /**
     * Renders a Level Sequence to an image sequence with the Movie Render Queue, in a Play In Editor session. Returns as soon
     * as the render starts; poll GetRenderStatus. The user's Movie Render Queue window keeps its own jobs.
     * @param Sequence Level Sequence to render.
     * @param MapPath Map to render in, e.g. "/Game/Maps/L_Intro". Pass an empty string for the level open in the Editor, which must be saved.
     * @param OutputDirectory Folder for the frames. Pass an empty string for the default, "{project_dir}/Saved/MovieRenders/".
     * @param Format "png", "jpg" or "bmp". Pass an empty string for "png".
     * @return JSON with the job name, map, output directory and format.
     */
    UFUNCTION(meta = (AICallable, MutatingTool), Category = "ACF")
    static FString RenderSequence(ULevelSequence* Sequence, const FString& MapPath, const FString& OutputDirectory, const FString& Format);

    /**
     * Reports whether a Movie Render Queue render is running and how far it is.
     * @return JSON with "rendering", "progress" (0 to 1) and "message".
     */
    UFUNCTION(meta = (AICallable, ReadOnlyTool), Category = "ACF")
    static FString GetRenderStatus();

private:
    static void WriteSectionRange(const UMovieSceneSection* Section, FJsonObject& Data);
};
