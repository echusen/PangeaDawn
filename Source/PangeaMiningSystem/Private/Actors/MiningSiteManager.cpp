#include "Actors/MiningSiteManager.h"
#include "Actors/OreVeinActor.h"
#include "Actors/MiningChestActor.h"

AMiningSiteManager::AMiningSiteManager()
{
    PrimaryActorTick.bCanEverTick = false;
}

void AMiningSiteManager::BeginPlay()
{
    Super::BeginPlay();
    SpawnForCurrentLevel();
}

void AMiningSiteManager::DestroySpawnedActors()
{
    if (SpawnedVein)
    {
        SpawnedVein->Destroy();
        SpawnedVein = nullptr;
    }

    if (SpawnedChest)
    {
        SpawnedChest->Destroy();
        SpawnedChest = nullptr;
    }

    if (SpawnedDining)
    {
        SpawnedDining->Destroy();
        SpawnedDining = nullptr;
    }
}

void AMiningSiteManager::SpawnForCurrentLevel()
{
    if (!LevelConfig)
    {
        UE_LOG(LogTemp, Warning, TEXT("MiningSiteManager %s has no LevelConfig"), *GetName());
        return;
    }

    const FMiningLevelDefinition& Def = LevelConfig->GetLevelDefinitionChecked(CurrentLevelIndex);

    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    const FTransform WorldVeinTransform = VeinSpawnTransform * GetActorTransform();
    const FTransform WorldChestTransform = ChestSpawnTransform * GetActorTransform();
    const FTransform WorldDiningTransform = DiningSpawnTransform * GetActorTransform();

    if (Def.VeinActorClass)
    {
        SpawnedVein = World->SpawnActorDeferred<AOreVeinActor>(
            Def.VeinActorClass,
            WorldVeinTransform,
            this,
            nullptr,
            ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);

        if (SpawnedVein)
        {
            SpawnedVein->LevelConfig = LevelConfig;
            SpawnedVein->LevelIndex = CurrentLevelIndex;
            SpawnedVein->FinishSpawning(WorldVeinTransform);
        }
    }

    if (Def.ChestActorClass)
    {
        SpawnedChest = World->SpawnActor<AMiningChestActor>(
            Def.ChestActorClass,
            WorldChestTransform);
        if (SpawnedChest)
        {
            // Optionally set capacity from Def.ChestStorageCapacityUnits here if needed.
            if (SpawnedChest->StorageComponent)
            {
                SpawnedChest->StorageComponent->SetMaxInventorySlots(Def.ChestStorageCapacityUnits);
            }
        }
    }

    if (Def.DiningActorClass)
    {
        SpawnedDining = World->SpawnActor<AActor>(
            Def.DiningActorClass,
            WorldDiningTransform);
    }
}

void AMiningSiteManager::UpgradeToNextLevel()
{
    if (!LevelConfig)
    {
        return;
    }

    const int32 MaxIndex = LevelConfig->Levels.Num() - 1;
    if (CurrentLevelIndex >= MaxIndex)
    {
        return; // already at max
    }

    ++CurrentLevelIndex;

    DestroySpawnedActors();
    SpawnForCurrentLevel();
}