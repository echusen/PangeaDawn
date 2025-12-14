#include "Actors/OreVeinActor.h"
#include "SmartObjectComponent.h"
#include "TimerManager.h"

AOreVeinActor::AOreVeinActor()
{
    PrimaryActorTick.bCanEverTick = false;

    MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
    RootComponent = MeshComponent;

    StorageComponent = CreateDefaultSubobject<UACFStorageComponent>(TEXT("StorageComponent"));
    SmartObjectComponent = CreateDefaultSubobject<USmartObjectComponent>(TEXT("SmartObjectComponent"));
}

const FMiningVeinConfig& AOreVeinActor::GetVeinConfig() const
{
    static FMiningVeinConfig DummyConfig;

    if (LevelConfig)
    {
        const FMiningLevelDefinition& Def = LevelConfig->GetLevelDefinitionChecked(LevelIndex);
        return Def.VeinConfig;
    }

    return DummyConfig;
}

void AOreVeinActor::BeginPlay()
{
    Super::BeginPlay();

    if (!HasAuthority())
    {
        return;
    }

    const FMiningVeinConfig& VeinCfg = GetVeinConfig();

    if (!StorageComponent)
    {
        UE_LOG(LogTemp, Warning, TEXT("OreVeinActor %s has no StorageComponent"), *GetName());
        return;
    }

    StorageComponent->SetMaxInventorySlots(VeinCfg.MaxStorageCapacity);

    // 1 second tick – you can expose this if needed.
    GetWorldTimerManager().SetTimer(
        OreGenerationTimerHandle,
        this,
        &AOreVeinActor::GenerateOre,
        1.0f,
        true
    );
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
    if (!HasAuthority() || !StorageComponent || !LevelConfig)
    {
        return;
    }

    const FMiningVeinConfig& VeinCfg = GetVeinConfig();

    if (StorageComponent->GetInventoryListConst().Num() >= VeinCfg.MaxStorageCapacity)
    {
        return;
    }

    const int32 NumEntries = VeinCfg.GeneratedItems.Num();
    if (NumEntries == 0)
    {
        return;
    }

    for (int32 Index = 0; Index < NumEntries; ++Index)
    {
        const FBaseItem& TemplateItem = VeinCfg.GeneratedItems[Index];
        if (!TemplateItem.ItemClass)
        {
            continue;
        }

        // Optional per-entry chance.
        if (VeinCfg.ItemChances.IsValidIndex(Index))
        {
            const float Chance = VeinCfg.ItemChances[Index];
            if (Chance < 1.f && FMath::FRand() > Chance)
            {
                continue;
            }
        }

        FBaseItem ToAdd = TemplateItem;

        // Scale count by MineralsPerSecond.
        const float RawCount = static_cast<float>(TemplateItem.Count) * VeinCfg.MineralsPerSecond;
        ToAdd.Count = FMath::Max(0, FMath::FloorToInt(RawCount));

        if (ToAdd.Count <= 0)
        {
            continue;
        }

        StorageComponent->AddItem(ToAdd);
    }
}