// // Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include <AttributeSet.h>

#include "ACFGASDeveloperSettings.generated.h"

class UDataTable;

/**
 *
 */
UCLASS(config = Plugins, Defaultconfig, meta = (DisplayName = "Ascent GAS Settings"))

class ASCENTGASRUNTIME_API UACFGASDeveloperSettings : public UDeveloperSettings {
    GENERATED_BODY()

public:
    UDataTable* GetAttributeKeys() const;

    const UDataTable* GetAttributesByCurveRow() const { return AttributesByCurveRow.LoadSynchronous(); }

    UDataTable* GetAttributesToSetByCallerTagsDT() const
    {
        return AttributesToSetByCallerTags.LoadSynchronous();
    }

     UDataTable* GetStatsToSetByCallerTagsDT() const
    {
        return StatsToSetByCallerTags.LoadSynchronous();
    }


	int32 GetMaxLevel() const { return MaxLevel; }

    FGameplayAttribute GetHealthAttribute() const { return HealthAttribute; }
protected:
    /** List of data tables to load tags from */
    UPROPERTY(config, EditAnywhere, Category = "ACF", meta = (RequiredAssetDataTags = "RowStructure=/Script/AscentGASRuntime.AttributeSerializeKeys"))
    TSoftObjectPtr<UDataTable> SerializableAttributes;

    /**Data Table to match any GameplayAttributes with the name of the Curve Data
    that will be used to generate Attributes when leveling up*/
    UPROPERTY(config, EditAnywhere, Category = "ACF", meta = (RequiredAssetDataTags = "RowStructure=/Script/AscentGASRuntime.AttributeSerializeKeys"))
    TSoftObjectPtr<UDataTable> AttributesByCurveRow;

    UPROPERTY(EditAnywhere, config, Category = ACF)
    FGameplayAttribute HealthAttribute;

    /*Max Level for all your character*/
    UPROPERTY(EditAnywhere, config, Category = ACF)
    int32 MaxLevel = 100;

    /*Define here the tags to be used for SetByCaller functions in default GameplayEffects*/
    UPROPERTY(config, EditAnywhere, Category = "ACF|GameplayEffects", meta = (RequiredAssetDataTags = "RowStructure=/Script/AdvancedRPGSystem.AttributesConfig"))
    TSoftObjectPtr<UDataTable> AttributesToSetByCallerTags;

    /*Define here the tags to be used for SetByCaller functions in default GameplayEffects*/
    UPROPERTY(config, EditAnywhere, Category = "ACF|GameplayEffects", meta = (RequiredAssetDataTags = "RowStructure=/Script/AdvancedRPGSystem.StatisticsConfig"))
    TSoftObjectPtr<UDataTable> StatsToSetByCallerTags;

};
