#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Data/MiningLevelConfig.h"
#include "Components/ACFStorageComponent.h"
#include "OreVeinActor.generated.h"

class USmartObjectComponent;

/**
 * Generic ore vein that only knows how to:
 * - read FMiningVeinConfig
 * - periodically push items into its UACFStorageComponent.
 *
 * All balancing / item selection lives in UMiningLevelConfig.
 */
UCLASS(Blueprintable)
class PANGEAMININGSYSTEM_API AOreVeinActor : public AActor
{
    GENERATED_BODY()

public:
    AOreVeinActor();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    /** Static mesh or hierarchical mesh for the vein. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UStaticMeshComponent> MeshComponent;

    /** Storage for generated items. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UACFStorageComponent> StorageComponent;

    /** Smart Object so AI routines can use this as a workstation. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<USmartObjectComponent> SmartObjectComponent;

    /** Configuration asset for this site / level. Set by MiningSiteManager. */
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Mining")
    TObjectPtr<UMiningLevelConfig> LevelConfig;

    /** Index into LevelConfig->Levels that this vein uses. */
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Mining")
    int32 LevelIndex = 0;

    /** Convenience accessor for Blueprint / debug. */
    UFUNCTION(BlueprintPure, Category = "Mining")
    const FMiningVeinConfig& GetVeinConfig() const;

protected:
    /** Tick-style generation called by timer (server only). */
    UFUNCTION()
    void GenerateOre();

private:
    FTimerHandle OreGenerationTimerHandle;
};