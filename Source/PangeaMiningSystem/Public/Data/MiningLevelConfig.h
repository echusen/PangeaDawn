#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Items/ACFItem.h"
#include "MiningLevelConfig.generated.h"

class AOreVeinActor;
class AMiningChestActor;
class AActor;

/**
 * Per-level vein generation data.
 * Single source of truth for ore type, rate and capacity.
 */
USTRUCT(BlueprintType)
struct FMiningVeinConfig
{
    GENERATED_BODY();

    /** Base items generated per tick (usually per second). Count is amount per tick. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mining")
    TArray<FBaseItem> GeneratedItems;

    /** Optional per-entry chance (0..1). If empty, all entries are always generated. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mining")
    TArray<float> ItemChances;

    /** Scalar applied to all GeneratedItems.Count each tick. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mining")
    float MineralsPerSecond = 1.f;

    /** Max capacity of the vein storage component. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mining")
    int32 MaxStorageCapacity = 20;
};

/**
 * One mining level / site definition.
 * The manager uses this to spawn actors and configure the vein.
 */
USTRUCT(BlueprintType)
struct FMiningLevelDefinition
{
    GENERATED_BODY();

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mining")
    int32 Level = 1;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mining")
    FText LevelName;

    /** Actor class for the ore vein workstation (must be AOreVeinActor or child). */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Actors")
    TSubclassOf<AOreVeinActor> VeinActorClass;

    /** Actor class for the chest / storage (must be AMiningChestActor or child). */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Actors")
    TSubclassOf<AMiningChestActor> ChestActorClass;

    /** Optional dining / rest actor. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Actors")
    TSubclassOf<AActor> DiningActorClass;

    /** Vein generation config for this level. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mining")
    FMiningVeinConfig VeinConfig;

    /** How many workers this level supports. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Workers")
    int32 NumberOfMiners = 0;

    /** Total storage capacity of the chest, if you want it separate from vein capacity. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Storage")
    int32 ChestStorageCapacityUnits = 100;

    /** Upgrade costs etc. – keep whatever you already had here. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Economy")
    int32 UpgradeCostWood = 0;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Economy")
    int32 UpgradeCostIron = 0;
};

/**
 * Data asset: list of mining levels / sites.
 */
UCLASS(BlueprintType)
class PANGEAMININGSYSTEM_API UMiningLevelConfig : public UDataAsset
{
    GENERATED_BODY()

public:
    /** Ordered list of levels. Index corresponds to “level – 1” by convention. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mining")
    TArray<FMiningLevelDefinition> Levels;

    /** Returns current level definition by index; safe default if index is invalid. */
    UFUNCTION(BlueprintPure, Category = "Mining")
    const FMiningLevelDefinition& GetLevelDefinitionChecked(int32 Index) const;
};