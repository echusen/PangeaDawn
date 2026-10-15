// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CommonVideoPlayer.h"
#include "CoreMinimal.h"
#include <Delegates/Delegate.h>

#include "AUTVideoPlayer.generated.h"


DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPlaybackStateEvent);



/**
 * UACFVideoPlayerWidget
 *
 * A widget that extends CommonVideoPlayer to support audio playback
 * and extending the functionality of the video player to Blueprints.
 */
UCLASS()
class ASCENTUITOOLS_API UAUTVideoPlayer : public UCommonVideoPlayer {
    GENERATED_BODY()

public:

    UPROPERTY(BlueprintAssignable, Category = AUT)
    FOnPlaybackStateEvent OnPlaybackCompletedEvent;

    UPROPERTY(BlueprintAssignable, Category = AUT)
    FOnPlaybackStateEvent OnPlaybackPausedEvent;

    UPROPERTY(BlueprintAssignable, Category = AUT)
    FOnPlaybackStateEvent OnPlaybackResumedEvent;

    virtual void PostInitProperties() override;
    virtual void PostLoad() override;

protected:
    virtual void SynchronizeProperties() override;

    /** Override of base media event handler */
    virtual void HandleMediaPlayerEvent(EMediaEvent EventType) override;

private:
    /** Completes UCommonVideoPlayer runtime setup if it was skipped on the async loader. */
    void InitializeRuntimeMediaResources();

    /** Bind widget-owned media audio to the actual game/PIE world, not the editor device. */
    void EnsureRuntimeAudioWorld();
};
