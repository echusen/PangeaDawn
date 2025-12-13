#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Data/MiningLevelConfig.h"
#include "MiningSiteManager.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMiningSiteLevelChanged, int32, NewLevel);

/**
 * Manages mining site by spawning all level actors once and toggling visibility
 */
UCLASS()
class PANGEAMININGSYSTEM_API AMiningSiteManager : public AActor
{
    GENERATED_BODY()

public:
    AMiningSiteManager();

    virtual void BeginPlay() override;

    // ========== LEVEL MANAGEMENT ==========
    UFUNCTION(BlueprintCallable, Category = "Mining|Level")
    void SetLevel(int32 NewLevel);

    UFUNCTION(BlueprintCallable, Category = "Mining|Level")
    bool UpgradeToNextLevel();

    UFUNCTION(BlueprintCallable, Category = "Mining|Level")
    int32 GetCurrentLevel() const { return CurrentLevel; }

    UFUNCTION(BlueprintCallable, Category = "Mining|Level")
    bool CanUpgrade() const;

    // ========== ACTOR REFERENCES ==========
    UFUNCTION(BlueprintCallable, Category = "Mining|Actors")
    AActor* GetActiveWorkstation() const;

    UFUNCTION(BlueprintCallable, Category = "Mining|Actors")
    AActor* GetActiveChest() const;

    UFUNCTION(BlueprintCallable, Category = "Mining|Actors")
    AActor* GetActiveDiningTable() const;

    // ========== EVENTS ==========
    UPROPERTY(BlueprintAssignable, Category = "Mining|Events")
    FOnMiningSiteLevelChanged OnLevelChanged;

protected:
    // ========== CONFIGURATION ==========
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mining|Setup")
    TObjectPtr<UMiningLevelConfig> LevelConfig = nullptr;

    // ========== SPAWN TRANSFORMS ==========
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mining|Placement")
    FTransform WorkstationSpawnTransform;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mining|Placement")
    FTransform ChestSpawnTransform;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mining|Placement")
    FTransform DiningTableSpawnTransform;

    // ========== STATE ==========
    UPROPERTY(SaveGame, Replicated)
    int32 CurrentLevel = 0;

    // ========== SPAWNED ACTORS (ALL LEVELS) ==========
    // We spawn ALL actors from all levels at BeginPlay
    UPROPERTY()
    TMap<int32, TObjectPtr<AActor>> WorkstationsByLevel;

    UPROPERTY()
    TMap<int32, TObjectPtr<AActor>> ChestsByLevel;

    UPROPERTY()
    TMap<int32, TObjectPtr<AActor>> DiningTablesByLevel;

private:
    void SpawnAllLevelActors();
    void UpdateVisibilityForLevel(int32 Level);
    void SetActorEnabled(AActor* Actor, bool bEnabled);
    void UpdateChestCapacity();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};