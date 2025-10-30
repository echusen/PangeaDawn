// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.


#include "UI/PangeaBreedingFarmWidget.h"

#include "Actors/PangeaEggActor.h"
#include "Components/PangeaBreedableComponent.h"
#include "Components/PangeaBreedingFarmComponent.h"
#include "Data/PangeaSpeciesDataAsset.h"

UPangeaBreedingFarmWidget::UPangeaBreedingFarmWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
    
}

void UPangeaBreedingFarmWidget::NativeConstruct()
{
    Super::NativeConstruct();
    
    if (CurrentFarm)
    {
        // Bind UI to farm events
        CurrentFarm->OnEggSpawned.AddDynamic(this, &UPangeaBreedingFarmWidget::OnEggSpawned);
    }
}

void UPangeaBreedingFarmWidget::OnEggSpawned(APangeaEggActor* NewEgg)
{
    // Forward to Blueprint for UI updates
    AddEggEntry(NewEgg);
}

bool UPangeaBreedingFarmWidget::ValidateParentPair(UPangeaBreedableComponent* Male,UPangeaBreedableComponent* Female, FString& OutReason) const
{
    if (!Male || !Female)
    {
        OutReason = TEXT("Missing parent(s).");
        return false;
    }

    if (!Male->SpeciesData || !Female->SpeciesData)
    {
        OutReason = TEXT("Missing species data.");
        return false;
    }

    if (Male->SpeciesData->SpeciesID != Female->SpeciesData->SpeciesID)
    {
        OutReason = TEXT("Mismatched species.");
        return false;
    }

    if (Female->Gender != ECreatureGender::Female)
    {
        OutReason = TEXT("Second parent must be female.");
        return false;
    }

    if (!Male->IsFertile_Implementation() || !Female->IsFertile_Implementation())
    {
        OutReason = TEXT("One or both parents are not fertile.");
        return false;
    }

    if (!CurrentFarm->EggClass)
    {
        OutReason = TEXT("EggClass not set in breeding farm.");
        return false;
    }

    return true;
}

void UPangeaBreedingFarmWidget::TryBreedUI()
{
    if (!CurrentFarm)
    {
        UE_LOG(LogTemp, Warning, TEXT("UBreedingFarmWidget::TryBreed — No CurrentFarm set"));
        return;
    }

    FString FailureReason;
    if (!ValidateParentPair(SelectedMale, SelectedFemale, FailureReason))
    {
        UE_LOG(LogTemp, Warning, TEXT("UBreedingFarmWidget::ValidateParentPair: %s"), *FailureReason);
        return;
    }

    APangeaEggActor* OutEgg = nullptr;
    if (CurrentFarm->TryBreed(SelectedMale, SelectedFemale))
    {
        UE_LOG(LogTemp, Log, TEXT("Breeding successful: Egg spawned"));
        AddEggEntry(OutEgg);
        ClearSelection();
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("Breeding attempt failed"));
    }
}





