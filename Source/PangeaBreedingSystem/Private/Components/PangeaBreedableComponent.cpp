// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.


#include "Components/PangeaBreedableComponent.h"
#include "AdvancedRPGSystem/Public/ARSStatisticsComponent.h"
#include "Components/PangeaBreedingFarmComponent.h"
#include "Data/PangeaSpeciesDataAsset.h"

// Sets default values for this component's properties
UPangeaBreedableComponent::UPangeaBreedableComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
}

bool UPangeaBreedableComponent::IsFertile_Implementation() const
{
    UE_LOG(LogTemp, Warning, TEXT("%s fertility check — bIsFertile=%d, bIsOnCooldown=%d, Remaining=%.1f"),
        *GetOwner()->GetName(), bIsFertile, bIsOnFertilityCooldown, FertilityCooldownRemaining);
    return !bIsOnFertilityCooldown && bIsFertile;
}

bool UPangeaBreedableComponent::SetFertile_Implementation(const bool bNewFertile)
{
    bIsFertile = bNewFertile;
    return bIsFertile;
}

// Called when the game starts
void UPangeaBreedableComponent::BeginPlay()
{
    Super::BeginPlay();

    // Auto-wire ACF attributes if present on owner
    if (!ACFAttributes && GetOwner())
    {
        ACFAttributes = GetOwner()->FindComponentByClass<UARSStatisticsComponent>();
    }
    
    if (AActor* Owner = GetOwner())
    {
        USkeletalMeshComponent* Mesh = Owner->FindComponentByClass<USkeletalMeshComponent>();
        if (Mesh)
        {
            for (int32 i = 0; i < Mesh->GetNumMaterials(); ++i)
            {
                UMaterialInterface* Mat = Mesh->GetMaterial(i);
                if (Mat && !Cast<UMaterialInstanceDynamic>(Mat))
                {
                    Mesh->CreateAndSetMaterialInstanceDynamic(i);
                    UE_LOG(LogTemp, Warning, TEXT("%s: Converted material slot %d to dynamic."), *Owner->GetName(), i);
                }
            }
        }
    }
}

void UPangeaBreedableComponent::StartFertilityCooldown(float CooldownDuration)
{
    if (CooldownDuration <= 0.f || bIsOnFertilityCooldown)
        return;

    bIsOnFertilityCooldown = true;
    FertilityCooldownRemaining = CooldownDuration;

    OnFertilityStateChanged.Broadcast(false);

    // Start ticking timer
    GetWorld()->GetTimerManager().SetTimer(
        FertilityCooldownTimerHandle,
        this,
        &UPangeaBreedableComponent::EndFertilityCooldown,
        CooldownDuration,
        false
    );

    // Start a tick update (once per second)
    GetWorld()->GetTimerManager().SetTimer(
        FertilityTickTimerHandle,
        this,
        &UPangeaBreedableComponent::UpdateFertilityCooldown,
        1.0f,
        true
    );

    UE_LOG(LogTemp, Warning, TEXT("%s: fertility cooldown started (%.1f s)"), *GetOwner()->GetName(), CooldownDuration);
}

void UPangeaBreedableComponent::UpdateFertilityCooldown()
{
    // Defensive checks — should never tick if cooldown is already done
    if (!bIsOnFertilityCooldown)
    {
        GetWorld()->GetTimerManager().ClearTimer(FertilityTickTimerHandle);
        return;
    }

    // Decrease remaining time
    FertilityCooldownRemaining = FMath::Max(0.f, FertilityCooldownRemaining - 1.f);

    // Broadcast progress
    OnFertilityCooldownTick.Broadcast(FertilityCooldownRemaining);

    // End cooldown automatically if time runs out
    if (FertilityCooldownRemaining <= 0.f)
    {
        EndFertilityCooldown();
    }
}

void UPangeaBreedableComponent::EndFertilityCooldown()
{
    bIsOnFertilityCooldown = false;
    FertilityCooldownRemaining = 0.0f;

    // Clear tick timer
    GetWorld()->GetTimerManager().ClearTimer(FertilityTickTimerHandle);

    // Notify end
    OnFertilityStateChanged.Broadcast(true);

    UE_LOG(LogTemp, Warning, TEXT("%s: fertility cooldown ended"), *GetOwner()->GetName());
}

FParentSnapshot UPangeaBreedableComponent::BuildParentSnapshot() const
{
    FParentSnapshot Snapshot;
    Snapshot.SpeciesID = SpeciesData->SpeciesID;
    Snapshot.CreatureId = FGuid::NewGuid();
    Snapshot.Traits = GeneticTraits;
    Snapshot.ParentActor = GetOwner();
    Snapshot.VisualData = CollectMaterialGenetics();

    if (!SpeciesData)
        return Snapshot;

    AActor* Owner = GetOwner();
    if (!Owner)
        return Snapshot;

    USkeletalMeshComponent* Mesh = Owner->FindComponentByClass<USkeletalMeshComponent>();
    if (!Mesh)
        return Snapshot;

    const int32 SlotIndex = SpeciesData->InheritanceMaterialSlot;

    // Make sure slot exists
    if (SlotIndex >= Mesh->GetNumMaterials())
    {
        UE_LOG(LogTemp, Warning, TEXT("%s: Invalid slot index %d for species %s"),
            *Owner->GetName(), SlotIndex, *SpeciesData->GetName());
        return Snapshot;
    }

    UMaterialInterface* Mat = Mesh->GetMaterial(SlotIndex);
    if (!Mat)
        return Snapshot;

    // ✅ Supports both constant and dynamic material instances
    for (const FMaterialGeneticGroup& Group : SpeciesData->MaterialGeneticGroups)
    {
        for (const FName& Param : Group.ParameterNames)
        {
            FMaterialParameterInfo Info(Param);
            FLinearColor Value;
            if (Mat->GetVectorParameterValue(Info, Value))
            {
                Snapshot.MaterialParams.Add(Param, Value);
            }
            else
            {
                Snapshot.MaterialParams.Add(Param, FLinearColor::White);
            }
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("Built ParentSnapshot with %d material params for %s"), 
           Snapshot.MaterialParams.Num(), *Owner->GetName());

    return Snapshot;
}

class APangeaEggActor* UPangeaBreedableComponent::BreedWith(UPangeaBreedableComponent* OtherParent, UPangeaBreedingFarmComponent* Farm)
{
    if (!OtherParent || !Farm || !bIsFertile || !OtherParent->bIsFertile) return nullptr;
    
    if (SpeciesData->SpeciesID != OtherParent->SpeciesData->SpeciesID) return nullptr; 

    APangeaEggActor* Egg = Farm->TryBreed(this, OtherParent);
    if (Egg)
    {
        OnBred.Broadcast(Egg, OtherParent->GetOwner());
    }
    return Egg;
}

void UPangeaBreedableComponent::PullAttributesIntoTraits_Implementation(FGeneticTraitSet& InOutTraits) const
{
    
}

TMap<FName, FLinearColor> UPangeaBreedableComponent::CollectMaterialGenetics() const
{
    TMap<FName, FLinearColor> Out;

    if (!SpeciesData || !SpeciesData->bInheritParentMaterials)
        return Out;

    USkeletalMeshComponent* Mesh = GetOwner() ? GetOwner()->FindComponentByClass<USkeletalMeshComponent>() : nullptr;
    if (!Mesh)
        return Out;

    const int32 SlotIndex = SpeciesData->InheritanceMaterialSlot;
    if (SlotIndex >= Mesh->GetNumMaterials())
        return Out;

    UMaterialInterface* Mat = Mesh->GetMaterial(SlotIndex);
    if (!Mat)
        return Out;

    TArray<FMaterialParameterInfo> Infos;
    TArray<FGuid> Ids;
    Mat->GetAllVectorParameterInfo(Infos, Ids); // UE 5.6 compatible

    for (const FMaterialGeneticGroup& Group : SpeciesData->MaterialGeneticGroups)
    {
        for (const FName& Param : Group.ParameterNames)
        {
            FLinearColor Value;
            if (Mat->GetVectorParameterValue(Param, Value))
            {
                Out.Add(Param, Value);
            }
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("CollectMaterialGenetics: %d params from %s (slot %d)"),
        Out.Num(),
        *GetOwner()->GetName(),
        SlotIndex);

    return Out;
}

