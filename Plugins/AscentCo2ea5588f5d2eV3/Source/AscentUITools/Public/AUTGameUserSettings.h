// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameUserSettings.h"
#include "GameplayTagContainer.h"
#include "Sound/SoundClass.h"
#include "UObject/NoExportTypes.h"

#include "AUTGameUserSettings.generated.h"


/**
 *
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSettingsApplied);

UCLASS(BlueprintType, Blueprintable)
class ASCENTUITOOLS_API UAUTGameUserSettings : public UGameUserSettings {
    GENERATED_BODY()

public:
    UAUTGameUserSettings() {};

    virtual void SetToDefaults() override;

    UPROPERTY(BlueprintAssignable, Category = Settings)
    FOnSettingsApplied OnGameSettingsApplied;

    /*Gets Mouse and Gamepad Sensitivity*/
    UFUNCTION(BlueprintPure, Category = Settings)
    FVector2D GetAxisSensitivity() const;

    /*Sets Mouse and Gamepad Sensitivity*/
    UFUNCTION(BlueprintCallable, Category = Settings)
    void SetAxisSensitivity(FVector2D axisSensitivity);

    /*Gets if Y axis should be inverted*/
    UFUNCTION(BlueprintPure, Category = Settings)
    bool GetIsYAxisInverted() const
    {
        return bInvertY;
    }

    /*Sets if Y axis should be inverted*/
    UFUNCTION(BlueprintCallable, Category = Settings)
    void SetYAxisInverted(bool inYinverted);

    /*Gets An Array of volumes for audio channels*/
    UFUNCTION(BlueprintPure, Category = Settings)
    TArray<float> GetAudioVolumeLevels() const
    {
        return AudioVolumeLevels;
    }

    /*Sets An Array of volumes for audio channels*/
    UFUNCTION(BlueprintCallable, Category = Settings)
    void SetAudioVolumeLevels(const TArray<float>& inAudioLevels)
    {
        AudioVolumeLevels = inAudioLevels;
    }

    UFUNCTION(BlueprintPure, Category = Settings)
    bool GetToggleSprint() const { return bToggleSprint; }

    UFUNCTION(BlueprintCallable, Category = Settings)
    void SetToggleSprint(bool inToggleSprint);

protected:
    virtual void ApplySettings(bool bCheckForCommandLineOverrides) override;

    UPROPERTY(config)
    TArray<float> AudioVolumeLevels;

    UPROPERTY(config)
    float XSensitivity;

    UPROPERTY(config)
    float YSensitivity;

    UPROPERTY(config)
    bool bInvertY;

    UPROPERTY(config)
    bool bToggleSprint;

    UFUNCTION(BlueprintNativeEvent, Category = Settings)
    void BPApplySettings();
};
