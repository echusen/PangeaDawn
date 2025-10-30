// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL
// 2022. All Rights Reserved.

#pragma once

#include "ARSGenerationRulesDataAsset.h"
#include "ARSTypes.h"
#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include <GameplayTagContainer.h>

#include "ARSDeveloperSettings.generated.h"

/**
 *
 */
class UARSGenerationRulesDataAsset;

UCLASS(config = Plugins, defaultconfig, meta = (DisplayName = "Ascent RPG System"))
class ADVANCEDRPGSYSTEM_API UARSDeveloperSettings : public UDeveloperSettings {
    GENERATED_BODY()

public:

    /*Define here if the project should use the Advanced RPG System or Gameplay Ability System for Attributes*/
    UPROPERTY(EditAnywhere, config, Category = ACF)
    ERPGModule AttributeModule;

    UPROPERTY(EditAnywhere, config, meta = (Categories = "RPG.Statistic"),Category = ACF)
    FGameplayTag HealthTag;

    /*Default COMBAT TAGS */

    UARSGenerationRulesDataAsset* GetAttributesGenerationRules() const
    {
        return Cast<UARSGenerationRulesDataAsset>(AttributesGenerationConfig.TryLoad());
    }

protected:
    /*Define here the tag Root of your Primary Attributes*/
    UPROPERTY(config, EditAnywhere, Category = ACF, meta = (AllowedClasses = "/Script/AdvancedRPGSystem.ARSGenerationRulesDataAsset"))
    FSoftObjectPath AttributesGenerationConfig;
};
