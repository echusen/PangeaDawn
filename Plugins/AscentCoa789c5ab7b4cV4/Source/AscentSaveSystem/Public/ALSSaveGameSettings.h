// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.

#pragma once

#include "ALSSaveGame.h"
#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"

#include "ALSSaveGameSettings.generated.h"

/**
 *
 */
UCLASS(config = Plugins, Defaultconfig, meta = (DisplayName = "Ascent Load & Save"))
class ASCENTSAVESYSTEM_API UALSSaveGameSettings : public UDeveloperSettings {
    GENERATED_BODY()

protected:
    UALSSaveGameSettings() { }

    UPROPERTY(EditAnywhere, config, Category = "ALS | Defaults")
    TSubclassOf<class UALSSaveGame> SaveGameClass = UALSSaveGame::StaticClass();

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

    UPROPERTY(EditAnywhere, config, Category = "ALS | Screenshot")
    int32 SaveScreenWidth = 1280;

    UPROPERTY(EditAnywhere, config, Category = "ALS | Screenshot")
    int32 SaveScreenHeight = 720;

    UPROPERTY(EditAnywhere, config, Category = "ALS | Screenshot")
    int32 MaxSlotsNum = 8;

public:
    TSubclassOf<class UALSSaveGame> GetSaveGameClass() const
    {
        return SaveGameClass;
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

    FName GetOnComponentSavedFunctionName() const
    {
        return OnComponentSavedFunctionName;
    }

    FName GetOnComponentLoadedFunctionName() const
    {
        return OnComponentLoadedFunctionName;
    }
};
