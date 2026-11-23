// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "ALSSaveGame.h"
#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"

#include "ALSSaveGameSettings.generated.h"

/**
 *
 */
UCLASS(config = Plugins, DefaultConfig, meta = (DisplayName = "Ascent Load & Save"))
class ASCENTSAVESYSTEM_API UALSSaveGameSettings : public UDeveloperSettings {
    GENERATED_BODY()

protected:
    UALSSaveGameSettings() 
        : SaveGameClass(UALSSaveGame::StaticClass())
    { }

    /* Stored as a soft class reference so that Blueprint subclasses survive
       engine startup (DeveloperSettings config is parsed before Blueprints
       can be loaded, which would otherwise reset the value to the default). */
    UPROPERTY(EditAnywhere, config, Category = "ALS | Defaults", meta = (MetaClass = "/Script/AscentSaveSystem.ALSSaveGame", AllowAbstract = "false"))
    TSoftClassPtr<class UALSSaveGame> SaveGameClass;

    UPROPERTY(EditAnywhere, config, Category = "ALS | Defaults")
    FName OnComponentSavedFunctionName = "OnComponentSaved";

    UPROPERTY(EditAnywhere, config, Category = "ALS | Defaults")
    FName OnComponentLoadedFunctionName = "OnComponentLoaded";

    UPROPERTY(EditAnywhere, config, Category = "ALS | Defaults")
    FString DefaultSaveName = "ACFSave";

    UPROPERTY(EditAnywhere, config, Category = "ALS | Defaults")
    FString SavesMetadata = "SaveMetadata";

    /*Slot Usable to travel player between maps*/
    UPROPERTY(EditAnywhere, config, Category = "ALS | Defaults")
    FString TravelSlotName = "TempSave";

    /**
     * Default load-time policy applied to savable actors that DO NOT have a
     * UALSLoadAndSaveComponent: when the actor is present in the level but missing
     * from the loaded save game, decides whether to destroy it.
     *
     * Default is false (forward-compatible): actors added to a level after release
     * survive old saves. Set to true to opt back into the legacy behavior where
     * any savable actor missing from the save is destroyed on load.
     */
    UPROPERTY(EditAnywhere, config, Category = "ALS | Defaults")
    bool bDestroyActorsMissingInSaveByDefault = false;

    UPROPERTY(EditAnywhere, config, Category = "ALS | Screenshot")
    int32 SaveScreenWidth = 1280;

    UPROPERTY(EditAnywhere, config, Category = "ALS | Screenshot")
    int32 SaveScreenHeight = 720;

    UPROPERTY(EditAnywhere, config, Category = "ALS | Screenshot")
    int32 MaxSlotsNum = 8;

    /* Number of rotating AutoSave slots. When they are all used, the oldest one is overwritten.
       AutoSave slots do not count against MaxSlotsNum. */
    UPROPERTY(EditAnywhere, config, Category = "ALS | AutoSave", meta = (ClampMin = 1, UIMin = 1))
    int32 MaxAutoSaveSlotsNum = 3;

    /* Prefix used to name new AutoSave slots (e.g. AutoSave_1, AutoSave_2...). */
    UPROPERTY(EditAnywhere, config, Category = "ALS | AutoSave")
    FString AutoSaveSlotPrefix = "AutoSave";

    /* Description stored on AutoSaves when AutoSave is called without one. Save lists usually show it as the slot title. */
    UPROPERTY(EditAnywhere, config, Category = "ALS | AutoSave")
    FString AutoSaveDefaultDescription = "Autosave";

    /* Widget added to the viewport while an AutoSave is running. Leave empty to show nothing. */
    UPROPERTY(EditAnywhere, config, Category = "ALS | AutoSave", meta = (MetaClass = "/Script/UMG.UserWidget"))
    TSoftClassPtr<class UUserWidget> AutoSaveThrobberWidgetClass;

    /* Minimum time in seconds the throbber stays on screen, so fast saves don't just flash it. */
    UPROPERTY(EditAnywhere, config, Category = "ALS | AutoSave", meta = (ClampMin = 0.f, UIMin = 0.f, Units = "s"))
    float AutoSaveThrobberMinDuration = 1.5f;

    UPROPERTY(EditAnywhere, config, Category = "ALS | AutoSave")
    int32 AutoSaveThrobberZOrder = 1000;

    /* On World Partition maps, when a save that reloads the player is loaded, waits for the area around the
       saved player location to be streamed in before the save is applied and the player is moved there. */
    UPROPERTY(EditAnywhere, config, Category = "ALS | World Partition")
    bool bWaitForWorldPartitionStreamingOnLoad = false;

    /* Radius streamed in around the saved player location while waiting. */
    UPROPERTY(EditAnywhere, config, Category = "ALS | World Partition", meta = (ClampMin = 100.f, UIMin = 100.f, Units = "cm"))
    float LoadStreamingRadius = 10000.f;

    /* Maximum time a load waits for the World Partition cells around the saved player location.
       When it expires the load continues anyway. 0 waits indefinitely. */
    UPROPERTY(EditAnywhere, config, Category = "ALS | World Partition", meta = (ClampMin = 0.f, UIMin = 0.f, Units = "s"))
    float LoadStreamingTimeout = 30.f;

public:
    TSubclassOf<class UALSSaveGame> GetSaveGameClass() const
    {
        if (SaveGameClass.IsNull()) {
            return UALSSaveGame::StaticClass();
        }
        UClass* LoadedClass = SaveGameClass.LoadSynchronous();
        return LoadedClass ? LoadedClass : UALSSaveGame::StaticClass();
    }

    FString GetSaveMetadataName() const
    {
        return SavesMetadata;
    }

    FString GetDefaultSaveName() const
    {
        return DefaultSaveName;
    }

    FString GetTravelSaveName() const
    {
        return TravelSlotName;
    }

    int32 GetDefaultScreenshotHeight() const
    {
        return SaveScreenHeight;
    }

    int32 GetDefaultScreenshotWidth() const
    {
        return SaveScreenWidth;
    }

    int32 GetMaxSlotsNum() const
    {
        return MaxSlotsNum;
    }

    int32 GetMaxAutoSaveSlotsNum() const
    {
        return FMath::Max(1, MaxAutoSaveSlotsNum);
    }

    FString GetAutoSaveSlotPrefix() const
    {
        return AutoSaveSlotPrefix.IsEmpty() ? TEXT("AutoSave") : AutoSaveSlotPrefix;
    }

    FString GetAutoSaveDefaultDescription() const
    {
        return AutoSaveDefaultDescription.IsEmpty() ? TEXT("Autosave") : AutoSaveDefaultDescription;
    }

    TSoftClassPtr<class UUserWidget> GetAutoSaveThrobberWidgetClass() const
    {
        return AutoSaveThrobberWidgetClass;
    }

    float GetAutoSaveThrobberMinDuration() const
    {
        return FMath::Max(0.f, AutoSaveThrobberMinDuration);
    }

    int32 GetAutoSaveThrobberZOrder() const
    {
        return AutoSaveThrobberZOrder;
    }

    bool GetWaitForWorldPartitionStreamingOnLoad() const
    {
        return bWaitForWorldPartitionStreamingOnLoad;
    }

    float GetLoadStreamingRadius() const
    {
        return FMath::Max(100.f, LoadStreamingRadius);
    }

    float GetLoadStreamingTimeout() const
    {
        return FMath::Max(0.f, LoadStreamingTimeout);
    }

    FName GetOnComponentSavedFunctionName() const
    {
        return OnComponentSavedFunctionName;
    }

    FName GetOnComponentLoadedFunctionName() const
    {
        return OnComponentLoadedFunctionName;
    }

    bool GetDestroyActorsMissingInSaveByDefault() const
    {
        return bDestroyActorsMissingInSaveByDefault;
    }
};
