// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.

#pragma once

#include "AUTThemeDataAsset.h"
#include "AUTTypes.h"
#include "Components/InputComponent.h"
#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "UObject/SoftObjectPath.h"

#include "AUTDeveloperSettings.generated.h"


/**
 *
 */
class FAUTAnalogCursor;

UCLASS(config = Plugins, Defaultconfig, meta = (DisplayName = "Ascent UI Settings"))
class ASCENTUITOOLS_API UAUTDeveloperSettings : public UDeveloperSettings {
    GENERATED_BODY()

public:
    UAUTDeveloperSettings() {};

    UAUTThemeDataAsset* GetTheme() const;

    TArray<FSoftObjectPath> GetDefaultSoundClasses() const { return DefaultSoundClasses; }
    TSoftObjectPtr<UWorld> GetDefaultMenuMapRef() const;
    TSoftObjectPtr<UWorld> GetDefaultNewGameMapRef() const;

protected:
    UPROPERTY(config, EditAnywhere, Category = Style, meta = (AllowedClasses = "/Script/AscentUITools.AUTThemeDataAsset"))
    FSoftObjectPath ThemesAsset;

    /** If set, this map will be loaded when the Editor starts up. */
	UPROPERTY(config, EditAnywhere, Category=DefaultMaps, meta=(AllowedClasses="/Script/Engine.World"))
	FSoftObjectPath DefaultMenuMap;

    UPROPERTY(config, EditAnywhere, Category=DefaultMaps, meta=(AllowedClasses="/Script/Engine.World"))
	FSoftObjectPath DefaultNewGameMap;

   UPROPERTY(config, EditAnywhere, Category = DefaultMaps, meta=(AllowedClasses="/Script/Engine.SoundClass"))
    TArray<FSoftObjectPath> DefaultSoundClasses;
};
