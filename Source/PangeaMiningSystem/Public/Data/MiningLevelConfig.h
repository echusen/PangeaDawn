#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "MiningLevelConfig.generated.h"

USTRUCT(BlueprintType)
struct FMiningLevelData
{
    GENERATED_BODY()

    // ========== LEVEL INFO ==========
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Level")
    int32 Level = 1;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Level")
    FText LevelName = FText::FromString("Basic Site");

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Level", meta = (MultiLine = true))
    FText LevelDescription;

    // ========== ACTORS TO SPAWN (one per level) ==========
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Actors")
    TSubclassOf<AActor> WorkstationActorClass = nullptr;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Actors")
    TSubclassOf<AActor> ChestActorClass = nullptr;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Actors")
    TSubclassOf<AActor> DiningTableActorClass = nullptr;

    // ========== STORAGE ==========
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Storage")
    int32 StorageCapacityUnits = 50;

    // ========== AUTOMATION ==========
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Automation")
    int32 NumberOfMiners = 0;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Automation")
    int32 AutomatedMineralsPerDay = 0;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Automation")
    float MiningSpeedMultiplier = 1.0f;

    // ========== UPGRADE COSTS ==========
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Upgrade")
    int32 UpgradeCostWood = 0;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Upgrade")
    int32 UpgradeCostStone = 0;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Upgrade")
    int32 UpgradeCostIron = 0;
};

/**
 * Single data asset containing all mining site level configurations
 */
UCLASS(BlueprintType)
class PANGEAMININGSYSTEM_API UMiningLevelConfig : public UDataAsset
{
    GENERATED_BODY()

public:
    // All levels configuration
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Levels", meta = (TitleProperty = "LevelName"))
    TArray<FMiningLevelData> MiningLevels;

    // Shared mineral item class
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Items")
    TSubclassOf<class UACFItem> MineralItemClass = nullptr;

    // Helper functions
    UFUNCTION(BlueprintCallable, Category = "Mining")
    bool GetLevelData(int32 Level, FMiningLevelData& OutLevelData) const
    {
        for (const FMiningLevelData& LevelData : MiningLevels)
        {
            if (LevelData.Level == Level)
            {
                OutLevelData = LevelData;
                return true;
            }
        }
        return false;
    }

    UFUNCTION(BlueprintCallable, Category = "Mining")
    int32 GetMaxLevel() const
    {
        int32 MaxLevel = 0;
        for (const FMiningLevelData& LevelData : MiningLevels)
        {
            if (LevelData.Level > MaxLevel)
            {
                MaxLevel = LevelData.Level;
            }
        }
        return MaxLevel;
    }
};