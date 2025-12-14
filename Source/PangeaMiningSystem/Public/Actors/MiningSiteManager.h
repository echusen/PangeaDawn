#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Data/MiningLevelConfig.h"
#include "MiningSiteManager.generated.h"

class AOreVeinActor;
class AMiningChestActor;

/**
 * Responsible for:
 * - reading the MiningLevelConfig
 * - spawning / upgrading vein + chest + optional dining actors
 * - wiring LevelConfig/LevelIndex into AOreVeinActor
 *
 * It does NOT handle AI routines or inventory transfers; those stay in Blueprint/ACF.
 */
UCLASS(Blueprintable)
class PANGEAMININGSYSTEM_API AMiningSiteManager : public AActor
{
    GENERATED_BODY()

public:
    AMiningSiteManager();

    virtual void BeginPlay() override;

    /** Main config asset for all levels of this site. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mining")
    TObjectPtr<UMiningLevelConfig> LevelConfig;

    /** Current level index into LevelConfig->Levels. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mining")
    int32 CurrentLevelIndex = 0;

    /** Where the vein will be spawned (relative to manager). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawning")
    FTransform VeinSpawnTransform;

    /** Where the chest will be spawned (relative to manager). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawning")
    FTransform ChestSpawnTransform;

    /** Optional extra spawn (dining area, etc.). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawning")
    FTransform DiningSpawnTransform;

    /** Runtime references – useful for Blueprint logic. */
    UPROPERTY(BlueprintReadOnly, Category = "Runtime")
    TObjectPtr<AOreVeinActor> SpawnedVein;

    UPROPERTY(BlueprintReadOnly, Category = "Runtime")
    TObjectPtr<AMiningChestActor> SpawnedChest;

    UPROPERTY(BlueprintReadOnly, Category = "Runtime")
    TObjectPtr<AActor> SpawnedDining;

    /** Upgrades to the next level definition (if any). */
    UFUNCTION(BlueprintCallable, Category = "Mining")
    void UpgradeToNextLevel();

protected:
    void SpawnForCurrentLevel();
    void DestroySpawnedActors();
};