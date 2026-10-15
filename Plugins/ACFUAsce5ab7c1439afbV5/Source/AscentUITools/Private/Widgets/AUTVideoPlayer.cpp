// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.


#include "Widgets/AUTVideoPlayer.h"
#include "Engine/World.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "MediaPlayer.h"
#include "MediaSoundComponent.h"
#include "MediaTexture.h"
#include "Styling/SlateBrush.h"
#include "UObject/UnrealType.h"
#include <CommonVideoPlayer.h>
#include <IMediaEventSink.h>

void UAUTVideoPlayer::PostInitProperties()
{
    // UCommonVideoPlayer::PostInitProperties creates a MID and calls SetTextureParameterValue.
    // In UE 5.8.1 that hits CacheTexturesSamplingInfo, which asserts game thread.
    // Packaged builds can construct this widget as an export on FAsyncLoadingThread2.
    if (IsTemplate() || IsInGameThread() || IsInParallelGameThread()) {
        Super::PostInitProperties();
        return;
    }

    UWidget::PostInitProperties();
}

void UAUTVideoPlayer::PostLoad()
{
    Super::PostLoad();
    InitializeRuntimeMediaResources();
}

void UAUTVideoPlayer::SynchronizeProperties()
{
    InitializeRuntimeMediaResources();
    EnsureRuntimeAudioWorld();
    Super::SynchronizeProperties();
}

void UAUTVideoPlayer::EnsureRuntimeAudioWorld()
{
    if (!IsInGameThread() || IsTemplate() || IsDesignTime()) {
        return;
    }
    UWorld* World = GetWorld();
    if (!World || !World->IsGameWorld() || World->GetNetMode() == NM_DedicatedServer) {
        return;
    }

    FObjectProperty* SoundProperty = FindFProperty<FObjectProperty>(UCommonVideoPlayer::StaticClass(), TEXT("SoundComponent"));
    FObjectProperty* PlayerProperty = FindFProperty<FObjectProperty>(UCommonVideoPlayer::StaticClass(), TEXT("MediaPlayer"));
    if (!SoundProperty || !PlayerProperty) {
        return;
    }
    UMediaPlayer* Player = Cast<UMediaPlayer>(PlayerProperty->GetObjectPropertyValue_InContainer(this));
    if (!Player) {
        return;
    }
    UMediaSoundComponent* Sound = Cast<UMediaSoundComponent>(SoundProperty->GetObjectPropertyValue_InContainer(this));
    if (Sound && Sound->IsRegistered() && Sound->GetWorld() == World) {
        return;
    }

    // CommonVideoPlayer creates audio before the widget has a game world. Its inner
    // AudioComponent can therefore retain the editor's audio device. Recreate it
    // and register the synth BEFORE initialization so both components use this world.
    if (Sound) {
        Sound->Stop();
        Sound->SetMediaPlayer(nullptr);
        Sound->DestroyComponent();
    }
    Sound = NewObject<UMediaSoundComponent>(this);
    Sound->bAutoActivate = false;
    Sound->Channels = EMediaSoundChannels::Stereo;
    Sound->bIsUISound = true; // ANS_VideoPlayer_WB pauses the game during playback.
    Sound->bAllowSpatialization = false;
    Sound->SetMediaPlayer(Player);
    SoundProperty->SetObjectPropertyValue_InContainer(this, Sound);
    Sound->RegisterComponentWithWorld(World);
    Sound->Initialize();
    Sound->UpdatePlayer();
    if (Player->IsPlaying() && !IsMuted()) {
        Sound->Start();
    }
}

void UAUTVideoPlayer::InitializeRuntimeMediaResources()
{
    if (IsTemplate() || !(IsInGameThread() || IsInParallelGameThread())) {
        return;
    }

    UClass* CommonVideoPlayerClass = UCommonVideoPlayer::StaticClass();
    FObjectProperty* MediaPlayerProp = FindFProperty<FObjectProperty>(CommonVideoPlayerClass, TEXT("MediaPlayer"));
    FObjectProperty* MediaTextureProp = FindFProperty<FObjectProperty>(CommonVideoPlayerClass, TEXT("MediaTexture"));
    FObjectProperty* VideoMaterialProp = FindFProperty<FObjectProperty>(CommonVideoPlayerClass, TEXT("VideoMaterial"));
    FObjectProperty* SoundComponentProp = FindFProperty<FObjectProperty>(CommonVideoPlayerClass, TEXT("SoundComponent"));
    FStructProperty* VideoBrushProp = FindFProperty<FStructProperty>(CommonVideoPlayerClass, TEXT("VideoBrush"));
    if (!MediaPlayerProp || !MediaTextureProp || !VideoMaterialProp || !SoundComponentProp || !VideoBrushProp) {
        return;
    }

    if (Cast<UMediaPlayer>(MediaPlayerProp->GetObjectPropertyValue_InContainer(this))) {
        return;
    }

    UMediaPlayer* NewMediaPlayer = NewObject<UMediaPlayer>(this);
    NewMediaPlayer->PlayOnOpen = false;
    NewMediaPlayer->OnMediaEvent().AddUObject(this, &UAUTVideoPlayer::HandleMediaPlayerEvent);
    MediaPlayerProp->SetObjectPropertyValue_InContainer(this, NewMediaPlayer);

    UMediaTexture* NewMediaTexture = NewObject<UMediaTexture>(this);
    NewMediaTexture->AutoClear = true;
    NewMediaTexture->SetMediaPlayer(NewMediaPlayer);
    NewMediaTexture->UpdateResource();
    MediaTextureProp->SetObjectPropertyValue_InContainer(this, NewMediaTexture);

    UMaterial* BaseVideoMaterial = Cast<UMaterial>(VideoMaterialProp->GetObjectPropertyValue_InContainer(this));
    if (ensure(BaseVideoMaterial)) {
        UMaterialInstanceDynamic* VideoMID = UMaterialInstanceDynamic::Create(BaseVideoMaterial, this);
        VideoMID->SetTextureParameterValue(TEXT("MediaTexture"), NewMediaTexture);
        if (FSlateBrush* Brush = VideoBrushProp->ContainerPtrToValuePtr<FSlateBrush>(this)) {
            Brush->SetResourceObject(VideoMID);
        }
    }

    UMediaSoundComponent* NewSoundComponent = NewObject<UMediaSoundComponent>(this);
    NewSoundComponent->Channels = EMediaSoundChannels::Stereo;
    NewSoundComponent->bIsUISound = true;
    NewSoundComponent->SetMediaPlayer(NewMediaPlayer);
    NewSoundComponent->Initialize();
    NewSoundComponent->UpdatePlayer();
    SoundComponentProp->SetObjectPropertyValue_InContainer(this, NewSoundComponent);
}

void UAUTVideoPlayer::HandleMediaPlayerEvent(EMediaEvent EventType)
{
    if (EventType == EMediaEvent::MediaOpened || EventType == EMediaEvent::PlaybackResumed) {
        EnsureRuntimeAudioWorld();
    }
    // Call parent first to preserve base behavior
    Super::HandleMediaPlayerEvent(EventType);

    switch (EventType) {
    case EMediaEvent::MediaOpened:
    case EMediaEvent::TracksChanged:

        break;

    case EMediaEvent::PlaybackEndReached:
        OnPlaybackCompletedEvent.Broadcast();
        break;

    case EMediaEvent::PlaybackResumed:
        OnPlaybackResumedEvent.Broadcast();
        break;
    case EMediaEvent::PlaybackSuspended:
        OnPlaybackPausedEvent.Broadcast();
        break;
    case EMediaEvent::MediaClosed:
        break;
    default:
        break;
    }
}
