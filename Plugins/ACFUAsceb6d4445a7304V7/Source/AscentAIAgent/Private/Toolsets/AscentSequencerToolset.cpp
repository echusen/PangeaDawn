// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "Toolsets/AscentSequencerToolset.h"

#include "Toolsets/AscentToolsetUtils.h"

#include "Channels/MovieSceneEvent.h"
#include "Editor.h"
#include "Engine/World.h"
#include "K2Node_CustomEvent.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "LevelSequence.h"
#include "Misc/PackageName.h"
#include "MoviePipelineDeferredPasses.h"
#include "MoviePipelineImageSequenceOutput.h"
#include "MoviePipelineOutputSetting.h"
#include "MoviePipelinePIEExecutor.h"
#include "MoviePipelinePrimaryConfig.h"
#include "MoviePipelineQueue.h"
#include "MoviePipelineQueueSubsystem.h"
#include "MovieScene.h"
#include "MovieSceneSequence.h"
#include "MovieSceneTimeHelpers.h"
#include "Sections/MovieSceneAudioSection.h"
#include "Sections/MovieSceneEventTriggerSection.h"
#include "Sections/MovieSceneSubSection.h"
#include "SequencerTools.h"
#include "Sound/SoundBase.h"

FString UAscentSequencerToolset::SetSubSequence(UMovieSceneSubSection* Section, UMovieSceneSequence* Sequence)
{
    if (!Section || !Sequence)
    {
        return FAscentToolsetUtils::MakeErrorResponse(TEXT("[AscentSequencer] Section and Sequence are required"));
    }
    UMovieScene* OuterScene = Section->GetTypedOuter<UMovieScene>();
    UMovieScene* InnerScene = Sequence->GetMovieScene();
    if (!OuterScene || !InnerScene)
    {
        return FAscentToolsetUtils::MakeErrorResponse(TEXT("[AscentSequencer] The section or the sequence has no Movie Scene"));
    }
    if (Section->GetTypedOuter<UMovieSceneSequence>() == Sequence)
    {
        return FAscentToolsetUtils::MakeErrorResponse(TEXT("[AscentSequencer] A section cannot play the sequence that owns it"));
    }

    const FFrameNumber Start = Section->HasStartFrame() ? Section->GetInclusiveStartFrame() : FFrameNumber(0);
    const FFrameTime Length = ConvertFrameTime(FFrameTime(UE::MovieScene::DiscreteSize(InnerScene->GetPlaybackRange())),
        InnerScene->GetTickResolution(), OuterScene->GetTickResolution());

    Section->Modify();
    Section->SetSequence(Sequence);
    Section->SetRange(TRange<FFrameNumber>(Start, Start + FMath::Max(Length.CeilToFrame(), FFrameNumber(1))));

    const TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("sequence"), Sequence->GetPathName());
    WriteSectionRange(Section, *Data);
    return FAscentToolsetUtils::MakeSuccessResponse(Data);
}

FString UAscentSequencerToolset::SetAudioSound(UMovieSceneAudioSection* Section, USoundBase* Sound)
{
    if (!Section || !Sound)
    {
        return FAscentToolsetUtils::MakeErrorResponse(TEXT("[AscentSequencer] Section and Sound are required"));
    }
    const UMovieScene* Scene = Section->GetTypedOuter<UMovieScene>();
    if (!Scene)
    {
        return FAscentToolsetUtils::MakeErrorResponse(TEXT("[AscentSequencer] The section has no Movie Scene"));
    }

    Section->Modify();
    Section->SetSound(Sound);

    const float Duration = Sound->GetDuration();
    if (!Sound->IsLooping() && Duration > 0.f)
    {
        const FFrameNumber Start = Section->HasStartFrame() ? Section->GetInclusiveStartFrame() : FFrameNumber(0);
        const FFrameNumber Length = Scene->GetTickResolution().AsFrameTime(Duration).CeilToFrame();
        Section->SetRange(TRange<FFrameNumber>(Start, Start + FMath::Max(Length, FFrameNumber(1))));
    }

    const TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("sound"), Sound->GetPathName());
    Data->SetNumberField(TEXT("duration_seconds"), Duration);
    Data->SetBoolField(TEXT("looping"), Sound->IsLooping());
    WriteSectionRange(Section, *Data);
    return FAscentToolsetUtils::MakeSuccessResponse(Data);
}

FString UAscentSequencerToolset::AddEventKey(UMovieSceneEventTriggerSection* Section, int32 Frame, const FString& FunctionName,
    const TMap<FString, FString>& Payload, bool bCallInEditor)
{
    if (!Section || FunctionName.IsEmpty())
    {
        return FAscentToolsetUtils::MakeErrorResponse(TEXT("[AscentSequencer] Section and FunctionName are required"));
    }
    UMovieSceneSequence* Sequence = Section->GetTypedOuter<UMovieSceneSequence>();
    UMovieScene* Scene = Section->GetTypedOuter<UMovieScene>();
    const UMovieSceneTrack* Track = Section->GetTypedOuter<UMovieSceneTrack>();
    if (!Sequence || !Scene || !Track)
    {
        return FAscentToolsetUtils::MakeErrorResponse(TEXT("[AscentSequencer] The section is not inside a sequence track"));
    }

    FGuid BindingId;
    if (!Scene->FindTrackBinding(*Track, BindingId))
    {
        return FAscentToolsetUtils::MakeErrorResponse(TEXT("[AscentSequencer] The event track is on the sequence root. Add the event track to the binding of the object that receives the call (add_track_to_binding), then add the trigger section there"));
    }

    const UClass* BoundClass = nullptr;
    if (const FMovieScenePossessable* Possessable = Scene->FindPossessable(BindingId))
    {
        BoundClass = Possessable->GetPossessedObjectClass();
    }
    else if (const FMovieSceneSpawnable* Spawnable = Scene->FindSpawnable(BindingId))
    {
        BoundClass = Spawnable->GetObjectTemplate() ? Spawnable->GetObjectTemplate()->GetClass() : nullptr;
    }
    if (!BoundClass)
    {
        return FAscentToolsetUtils::MakeErrorResponse(TEXT("[AscentSequencer] Cannot resolve the class of the object the event track is bound to"));
    }
    if (!BoundClass->FindFunctionByName(FName(*FunctionName)))
    {
        return FAscentToolsetUtils::MakeErrorResponse(FString::Printf(
            TEXT("[AscentSequencer] %s has no function named '%s'"), *BoundClass->GetName(), *FunctionName));
    }

    const FSequencerQuickBindingResult Endpoint = USequencerToolsFunctionLibrary::CreateQuickBinding(
        Sequence, BoundClass->GetDefaultObject(), FunctionName, bCallInEditor);
    if (!Endpoint.EventEndpoint)
    {
        return FAscentToolsetUtils::MakeErrorResponse(FString::Printf(
            TEXT("[AscentSequencer] Could not create the Director Blueprint endpoint for '%s'"), *FunctionName));
    }

    TArray<FString> PayloadValues;
    for (const FString& PayloadName : Endpoint.PayloadNames)
    {
        const FString* Value = Payload.Find(PayloadName);
        PayloadValues.Add(Value ? *Value : FString());
    }
    FMovieSceneEvent Event = USequencerToolsFunctionLibrary::CreateEvent(Sequence, Section, Endpoint, PayloadValues);
    for (const FString& PayloadName : Endpoint.PayloadNames)
    {
        if (!Payload.Contains(PayloadName))
        {
            Event.PayloadVariables.Remove(FName(*PayloadName));
        }
    }

    const FFrameNumber TickFrame = ConvertFrameTime(FFrameTime(Frame), Scene->GetDisplayRate(), Scene->GetTickResolution()).RoundToFrame();
    Section->Modify();
    Section->EventChannel.GetData().AddKey(TickFrame, Event);
    if (!Section->GetRange().Contains(TickFrame))
    {
        Section->SetRange(TRange<FFrameNumber>::Hull(Section->GetRange(), TRange<FFrameNumber>::Inclusive(TickFrame, TickFrame)));
    }

    UBlueprint* Director = Endpoint.EventEndpoint->GetBlueprint();
    if (Director)
    {
        FKismetEditorUtilities::CompileBlueprint(Director);
    }

    TArray<TSharedPtr<FJsonValue>> PayloadNames;
    for (const FString& PayloadName : Endpoint.PayloadNames)
    {
        PayloadNames.Add(MakeShared<FJsonValueString>(PayloadName));
    }
    TArray<TSharedPtr<FJsonValue>> IgnoredPayload;
    for (const TPair<FString, FString>& Entry : Payload)
    {
        if (!Endpoint.PayloadNames.Contains(Entry.Key))
        {
            IgnoredPayload.Add(MakeShared<FJsonValueString>(Entry.Key));
        }
    }

    const TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetNumberField(TEXT("frame"), Frame);
    Data->SetStringField(TEXT("function"), FunctionName);
    Data->SetStringField(TEXT("endpoint"), Endpoint.EventEndpoint->CustomFunctionName.ToString());
    Data->SetStringField(TEXT("director_blueprint"), Director ? Director->GetPathName() : FString());
    Data->SetArrayField(TEXT("payload_names"), PayloadNames);
    if (!IgnoredPayload.IsEmpty())
    {
        Data->SetArrayField(TEXT("ignored_payload"), IgnoredPayload);
    }
    return FAscentToolsetUtils::MakeSuccessResponse(Data);
}

FString UAscentSequencerToolset::RenderSequence(ULevelSequence* Sequence, const FString& MapPath, const FString& OutputDirectory, const FString& Format)
{
    if (!Sequence)
    {
        return FAscentToolsetUtils::MakeErrorResponse(TEXT("[AscentSequencer] Sequence is required"));
    }
    UMoviePipelineQueueSubsystem* Subsystem = GEditor ? GEditor->GetEditorSubsystem<UMoviePipelineQueueSubsystem>() : nullptr;
    if (!Subsystem)
    {
        return FAscentToolsetUtils::MakeErrorResponse(TEXT("[AscentSequencer] The Movie Render Queue is not available. Enable the Movie Render Queue plugin"));
    }
    if (Subsystem->IsRendering())
    {
        return FAscentToolsetUtils::MakeErrorResponse(TEXT("[AscentSequencer] A render is already running. Poll GetRenderStatus until it finishes"));
    }

    const FString Kind = Format.IsEmpty() ? FString(TEXT("png")) : Format.ToLower();
    TSubclassOf<UMoviePipelineSetting> OutputClass;
    if (Kind == TEXT("png")) { OutputClass = UMoviePipelineImageSequenceOutput_PNG::StaticClass(); }
    else if (Kind == TEXT("jpg")) { OutputClass = UMoviePipelineImageSequenceOutput_JPG::StaticClass(); }
    else if (Kind == TEXT("bmp")) { OutputClass = UMoviePipelineImageSequenceOutput_BMP::StaticClass(); }
    else
    {
        return FAscentToolsetUtils::MakeErrorResponse(FString::Printf(
            TEXT("[AscentSequencer] Unknown format '%s'. Use png, jpg or bmp"), *Format));
    }

    FString MapPackage;
    if (MapPath.IsEmpty())
    {
        const UWorld* World = GEditor->GetEditorWorldContext().World();
        MapPackage = World ? World->GetPackage()->GetName() : FString();
        if (MapPackage.IsEmpty() || !FPackageName::DoesPackageExist(MapPackage))
        {
            return FAscentToolsetUtils::MakeErrorResponse(TEXT("[AscentSequencer] The level open in the Editor is not saved. Save it or pass MapPath"));
        }
    }
    else
    {
        MapPackage = FPackageName::ObjectPathToPackageName(MapPath);
        if (!FPackageName::DoesPackageExist(MapPackage))
        {
            return FAscentToolsetUtils::MakeErrorResponse(FString::Printf(TEXT("[AscentSequencer] Map '%s' does not exist"), *MapPath));
        }
    }

    UMoviePipelineQueue* Queue = NewObject<UMoviePipelineQueue>(Subsystem);
    UMoviePipelineExecutorJob* Job = Queue->AllocateNewJob(UMoviePipelineExecutorJob::StaticClass());
    Job->JobName = Sequence->GetName();
    Job->SetSequence(FSoftObjectPath(Sequence));
    Job->Map = FSoftObjectPath(MapPackage + TEXT(".") + FPackageName::GetShortName(MapPackage));

    UMoviePipelinePrimaryConfig* Config = Job->GetConfiguration();
    Config->FindOrAddSettingByClass(UMoviePipelineDeferredPassBase::StaticClass());
    Config->FindOrAddSettingByClass(OutputClass);
    UMoviePipelineOutputSetting* Output = Cast<UMoviePipelineOutputSetting>(Config->FindOrAddSettingByClass(UMoviePipelineOutputSetting::StaticClass()));
    if (Output && !OutputDirectory.IsEmpty())
    {
        Output->OutputDirectory.Path = OutputDirectory;
    }

    if (!Subsystem->RenderQueueInstanceWithExecutor(Queue, UMoviePipelinePIEExecutor::StaticClass()))
    {
        return FAscentToolsetUtils::MakeErrorResponse(TEXT("[AscentSequencer] The Movie Render Queue did not start the render. Check the LogMovieRenderPipeline log entries"));
    }

    const TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("job"), Job->JobName);
    Data->SetStringField(TEXT("map"), MapPackage);
    Data->SetStringField(TEXT("output_directory"), Output ? Output->OutputDirectory.Path : FString());
    Data->SetStringField(TEXT("format"), Kind);
    Data->SetStringField(TEXT("message"), TEXT("Render started. Poll GetRenderStatus until rendering is false."));
    return FAscentToolsetUtils::MakeSuccessResponse(Data);
}

FString UAscentSequencerToolset::GetRenderStatus()
{
    const UMoviePipelineQueueSubsystem* Subsystem = GEditor ? GEditor->GetEditorSubsystem<UMoviePipelineQueueSubsystem>() : nullptr;
    if (!Subsystem)
    {
        return FAscentToolsetUtils::MakeErrorResponse(TEXT("[AscentSequencer] The Movie Render Queue is not available. Enable the Movie Render Queue plugin"));
    }

    const UMoviePipelineExecutorBase* Executor = Subsystem->GetActiveExecutor();
    const TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetBoolField(TEXT("rendering"), Subsystem->IsRendering());
    Data->SetNumberField(TEXT("progress"), Executor ? Executor->GetStatusProgress() : 0.f);
    Data->SetStringField(TEXT("message"), Executor ? Executor->GetStatusMessage() : FString(TEXT("No render is running")));
    return FAscentToolsetUtils::MakeSuccessResponse(Data);
}

void UAscentSequencerToolset::WriteSectionRange(const UMovieSceneSection* Section, FJsonObject& Data)
{
    const UMovieScene* Scene = Section->GetTypedOuter<UMovieScene>();
    if (!Scene)
    {
        return;
    }
    const FFrameRate TickResolution = Scene->GetTickResolution();
    const FFrameRate DisplayRate = Scene->GetDisplayRate();
    if (Section->HasStartFrame())
    {
        Data.SetNumberField(TEXT("start"), ConvertFrameTime(Section->GetInclusiveStartFrame(), TickResolution, DisplayRate).FloorToFrame().Value);
    }
    if (Section->HasEndFrame())
    {
        Data.SetNumberField(TEXT("end"), ConvertFrameTime(Section->GetExclusiveEndFrame(), TickResolution, DisplayRate).FloorToFrame().Value);
    }
}
