// MiningSiteManager.cpp
#include "Actors/MiningSiteManager.h"
#include "Net/UnrealNetwork.h"
#include "Components/ACFStorageComponent.h"

AMiningSiteManager::AMiningSiteManager()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;

    WorkstationSpawnTransform = FTransform(FVector(0, 0, 0));
    ChestSpawnTransform = FTransform(FVector(300, 0, 0));
    DiningTableSpawnTransform = FTransform(FVector(-300, 0, 0));
}

void AMiningSiteManager::BeginPlay()
{
    Super::BeginPlay();

    if (HasAuthority())
    {
        SpawnAllLevelActors();

        if (CurrentLevel == 0)
        {
            SetLevel(1);
        }
        else
        {
            UpdateVisibilityForLevel(CurrentLevel);
        }
    }
}

void AMiningSiteManager::SpawnAllLevelActors()
{
    if (!LevelConfig)
    {
        UE_LOG(LogTemp, Error, TEXT("MiningSiteManager: LevelConfig not set!"));
        return;
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = this;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    for (const FMiningLevelData& LevelData : LevelConfig->MiningLevels)
    {
        int32 Level = LevelData.Level;

        // Spawn Workstation
        if (LevelData.WorkstationActorClass)
        {
            FTransform WorldTransform = WorkstationSpawnTransform * GetActorTransform();
            AActor* Workstation = GetWorld()->SpawnActor<AActor>(
                LevelData.WorkstationActorClass,
                WorldTransform,
                SpawnParams
            );

            if (Workstation)
            {
                WorkstationsByLevel.Add(Level, Workstation);
            }
        }

        // Spawn Chest
        if (LevelData.ChestActorClass)
        {
            FTransform WorldTransform = ChestSpawnTransform * GetActorTransform();
            AActor* Chest = GetWorld()->SpawnActor<AActor>(
                LevelData.ChestActorClass,
                WorldTransform,
                SpawnParams
            );

            if (Chest)
            {
                ChestsByLevel.Add(Level, Chest);
                
                // Set capacity
                if (UACFStorageComponent* StorageComp = Chest->FindComponentByClass<UACFStorageComponent>())
                {
                    StorageComp->SetMaxInventorySlots(LevelData.StorageCapacityUnits);
                }
            }
        }

        // Spawn Dining Table
        if (LevelData.DiningTableActorClass)
        {
            FTransform WorldTransform = DiningTableSpawnTransform * GetActorTransform();
            AActor* DiningTable = GetWorld()->SpawnActor<AActor>(
                LevelData.DiningTableActorClass,
                WorldTransform,
                SpawnParams
            );

            if (DiningTable)
            {
                DiningTablesByLevel.Add(Level, DiningTable);
            }
        }
    }
}

void AMiningSiteManager::SetLevel(int32 NewLevel)
{
    if (!HasAuthority()) return;
    if (!LevelConfig) return;

    FMiningLevelData LevelData;
    if (!LevelConfig->GetLevelData(NewLevel, LevelData))
    {
        UE_LOG(LogTemp, Error, TEXT("Level %d not found!"), NewLevel);
        return;
    }

    CurrentLevel = NewLevel;
    UpdateVisibilityForLevel(NewLevel);

    OnLevelChanged.Broadcast(CurrentLevel);
}

void AMiningSiteManager::UpdateVisibilityForLevel(int32 Level)
{
    // Hide all actors from all levels
    for (const auto& Pair : WorkstationsByLevel)
    {
        if (Pair.Value)
        {
            Pair.Value->SetActorHiddenInGame(true);
            Pair.Value->SetActorEnableCollision(false);
        }
    }

    for (const auto& Pair : ChestsByLevel)
    {
        if (Pair.Value)
        {
            Pair.Value->SetActorHiddenInGame(true);
            Pair.Value->SetActorEnableCollision(false);
        }
    }

    for (const auto& Pair : DiningTablesByLevel)
    {
        if (Pair.Value)
        {
            Pair.Value->SetActorHiddenInGame(true);
            Pair.Value->SetActorEnableCollision(false);
        }
    }

    // Show only current level actors
    if (TObjectPtr<AActor>* Workstation = WorkstationsByLevel.Find(Level))
    {
        if (*Workstation)
        {
            (*Workstation)->SetActorHiddenInGame(false);
            (*Workstation)->SetActorEnableCollision(true);
        }
    }

    if (TObjectPtr<AActor>* Chest = ChestsByLevel.Find(Level))
    {
        if (*Chest)
        {
            (*Chest)->SetActorHiddenInGame(false);
            (*Chest)->SetActorEnableCollision(true);
        }
    }

    if (TObjectPtr<AActor>* DiningTable = DiningTablesByLevel.Find(Level))
    {
        if (*DiningTable)
        {
            (*DiningTable)->SetActorHiddenInGame(false);
            (*DiningTable)->SetActorEnableCollision(true);
        }
    }
}

bool AMiningSiteManager::UpgradeToNextLevel()
{
    if (!HasAuthority()) return false;
    
    int32 NextLevel = CurrentLevel + 1;
    if (NextLevel > LevelConfig->GetMaxLevel())
    {
        return false;
    }

    SetLevel(NextLevel);
    return true;
}

bool AMiningSiteManager::CanUpgrade() const
{
    if (!LevelConfig) return false;
    return CurrentLevel < LevelConfig->GetMaxLevel();
}

AActor* AMiningSiteManager::GetActiveWorkstation() const
{
    if (const TObjectPtr<AActor>* Actor = WorkstationsByLevel.Find(CurrentLevel))
    {
        return *Actor;
    }
    return nullptr;
}

AActor* AMiningSiteManager::GetActiveChest() const
{
    if (const TObjectPtr<AActor>* Actor = ChestsByLevel.Find(CurrentLevel))
    {
        return *Actor;
    }
    return nullptr;
}

AActor* AMiningSiteManager::GetActiveDiningTable() const
{
    if (const TObjectPtr<AActor>* Actor = DiningTablesByLevel.Find(CurrentLevel))
    {
        return *Actor;
    }
    return nullptr;
}

void AMiningSiteManager::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AMiningSiteManager, CurrentLevel);
}
