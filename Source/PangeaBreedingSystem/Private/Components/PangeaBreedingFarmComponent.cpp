// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.


#include "Components/PangeaBreedingFarmComponent.h"

#include "Actors/PangeaBreedingFarmActor.h"
#include "Actors/PangeaEggActor.h"
#include "Components/BoxComponent.h"
#include "Components/PangeaBreedableComponent.h"

// Sets default values for this component's properties
UPangeaBreedingFarmComponent::UPangeaBreedingFarmComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
}


// Called when the game starts
void UPangeaBreedingFarmComponent::BeginPlay()
{
	Super::BeginPlay();

    if (BreedingZone)
    {
        UE_LOG(LogTemp, Warning, TEXT("FarmLogic bound to breeding zone: %s"), *BreedingZone->GetName());
        BreedingZone->OnComponentBeginOverlap.AddDynamic(this, &UPangeaBreedingFarmComponent::OnOverlapBegin);
        BreedingZone->OnComponentEndOverlap.AddDynamic(this, &UPangeaBreedingFarmComponent::OnOverlapEnd);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("FarmLogic has no BreedingZone assigned!"));
    }
}

void UPangeaBreedingFarmComponent::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    // Check if the overlapping actor has a breedable component
    UE_LOG(LogTemp, Warning, TEXT("Breedable Entered: %s"), *OtherActor->GetName());
    
    if (UPangeaBreedableComponent* Breedable = OtherActor->FindComponentByClass<UPangeaBreedableComponent>())
    {
        if (!ContainedBreedables.Contains(Breedable))
        {
            ContainedBreedables.Add(Breedable);
        }
    }
}

void UPangeaBreedingFarmComponent::OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
    if (UPangeaBreedableComponent* Breedable = OtherActor->FindComponentByClass<UPangeaBreedableComponent>())
    {
        ContainedBreedables.Remove(Breedable);
    }
}

void UPangeaBreedingFarmComponent::GetBreedablesByGender(TArray<UPangeaBreedableComponent*>& OutMales, TArray<UPangeaBreedableComponent*>& OutFemales) const
{
    OutMales.Reset();
    OutFemales.Reset();

    for (UPangeaBreedableComponent* Breedable : ContainedBreedables)
    {
        if (!IsValid(Breedable)) continue;

        switch (Breedable->Gender) // Assuming EGender { Male, Female }
        {
            case ECreatureGender::Male:   OutMales.Add(Breedable); break;
            case ECreatureGender::Female: OutFemales.Add(Breedable); break;
            default: break;
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("Farm GetBreedablesByGender: Males=%d, Females=%d"),
           OutMales.Num(), OutFemales.Num());
}

APangeaEggActor* UPangeaBreedingFarmComponent::TryBreed(UPangeaBreedableComponent* Male, UPangeaBreedableComponent* Female)
{
    UPangeaSpeciesDataAsset* SpeciesAsset = Female->SpeciesData;
    APangeaEggActor* Egg = SpawnEgg(Male, Female, SpeciesAsset);
    
    ApplyFertilityCooldowns(Male, Female, SpeciesAsset);
    OnEggSpawned.Broadcast(Egg);

    UE_LOG(LogTemp, Log, TEXT("Breeding successful — Egg spawned for species: %s"), *SpeciesAsset->SpeciesID.ToString());
    return Egg;
}

void UPangeaBreedingFarmComponent::RefreshContainedBreedables()
{
    APangeaBreedingFarmActor* FarmActor = Cast<APangeaBreedingFarmActor>(GetOwner());
    if (!FarmActor->BreedingZone)
    {
        UE_LOG(LogTemp, Warning, TEXT("RefreshContainedBreedables — no valid collision component on %s"), *GetOwner()->GetName());
        return;
    }

    // Query all overlapping actors
    TArray<AActor*> OverlappingActors;
    FarmActor->BreedingZone->GetOverlappingActors(OverlappingActors, AActor::StaticClass());

    int32 AddedCount = 0;

    ContainedBreedables.RemoveAll([](const UPangeaBreedableComponent* Breedable)
    {
        return !IsValid(Breedable);
    });

    for (AActor* Actor : OverlappingActors)
    {
        if (!Actor || Actor == GetOwner())
            continue;

        if (UPangeaBreedableComponent* Breedable = Actor->FindComponentByClass<UPangeaBreedableComponent>())
        {
            if (!ContainedBreedables.Contains(Breedable))
            {
                ContainedBreedables.Add(Breedable);
                AddedCount++;
            }
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("RefreshContainedBreedables — found %d breedables inside %s"), AddedCount, *GetOwner()->GetName());
}

void UPangeaBreedingFarmComponent::ApplyFertilityCooldowns(UPangeaBreedableComponent* Male, UPangeaBreedableComponent* Female, const UPangeaSpeciesDataAsset* SpeciesDataAsset)
{
    const float Cooldown = SpeciesDataAsset->Fertility.FertilityCooldownSeconds;
    const bool bAffectsBoth = SpeciesDataAsset->Fertility.bAffectsBothParents;

    if (Female)
    {
        Female->StartFertilityCooldown(Cooldown);
    }

    if (Male && bAffectsBoth)
    {
        Male->StartFertilityCooldown(Cooldown);
    }
}

APangeaEggActor* UPangeaBreedingFarmComponent::SpawnEgg(UPangeaBreedableComponent* Male, UPangeaBreedableComponent* Female, UPangeaSpeciesDataAsset* SpeciesDataAsset)
{
    FActorSpawnParameters Params;
    Params.Owner = GetOwner();
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    FVector SpawnLocation = GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector;

    APangeaEggActor* Egg = GetWorld()->SpawnActor<APangeaEggActor>(
        EggClass,
        SpawnLocation,
        FRotator::ZeroRotator,
        Params
    );

    Egg->InitializeEgg(Male->BuildParentSnapshot(), Female->BuildParentSnapshot(), GeneticStrategy, SpeciesDataAsset);
    return Egg;
}



