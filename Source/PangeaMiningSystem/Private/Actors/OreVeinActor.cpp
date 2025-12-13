// OreVeinActor.cpp
#include "Actors/OreVeinActor.h"
#include "SmartObjectComponent.h"
#include "TimerManager.h"

AOreVeinActor::AOreVeinActor()
{
    PrimaryActorTick.bCanEverTick = false;

    MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
    RootComponent = MeshComponent;

    StorageComponent = CreateDefaultSubobject<UACFStorageComponent>(TEXT("StorageComponent"));
}

void AOreVeinActor::BeginPlay()
{
    Super::BeginPlay();

    if (HasAuthority())
    {
        if (StorageComponent)
        {
            StorageComponent->SetMaxInventorySlots(MaxStorageCapacity);
        }

        GetWorldTimerManager().SetTimer(
            OreGenerationTimerHandle,
            this,
            &AOreVeinActor::GenerateOre,
            1.0f,
            true
        );
    }
}

void AOreVeinActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (OreGenerationTimerHandle.IsValid())
    {
        GetWorldTimerManager().ClearTimer(OreGenerationTimerHandle);
    }

    Super::EndPlay(EndPlayReason);
}

void AOreVeinActor::GenerateOre()
{
    if (!StorageComponent || !CommonMaterialClass)
    {
        return;
    }

    if (StorageComponent->GetInventoryListConst().Num() >= MaxStorageCapacity)
    {
        return;
    }

    // Generate common material
    FBaseItem CommonItem;
    CommonItem.ItemClass = CommonMaterialClass;
    CommonItem.Count = FMath::FloorToInt(MineralsPerSecond);

    if (CommonItem.Count > 0)
    {
        StorageComponent->AddItem(CommonItem);
    }

    // Roll for rare material
    if (RareMaterialClass && FMath::FRand() < RareMaterialChance)
    {
        FBaseItem RareItem;
        RareItem.ItemClass = RareMaterialClass;
        RareItem.Count = 1;

        StorageComponent->AddItem(RareItem);

        UE_LOG(LogTemp, Log, TEXT("%s: Generated rare material!"), *GetName());
    }
}
