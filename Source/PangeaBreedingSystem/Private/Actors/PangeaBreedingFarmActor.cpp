// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.


#include "Actors/PangeaBreedingFarmActor.h"

#include "Components/BoxComponent.h"
#include "Components/PangeaBreedingFarmComponent.h"


// Sets default values
APangeaBreedingFarmActor::APangeaBreedingFarmActor()
{
    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    RootComponent = Root;
    
    BreedingZone = CreateDefaultSubobject<UBoxComponent>(TEXT("BreedingZone"));
    BreedingZone->SetupAttachment(Root);

    BreedingZone->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    BreedingZone->SetCollisionResponseToAllChannels(ECR_Ignore);
    BreedingZone->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    BreedingZone->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
    BreedingZone->SetGenerateOverlapEvents(true);
    BreedingZone->bHiddenInGame = false;
    
    FarmLogic = CreateDefaultSubobject<UPangeaBreedingFarmComponent>(TEXT("Pangea Breeding Farm Component"));
}

void APangeaBreedingFarmActor::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);

    FarmLogic->BreedingZone = BreedingZone;

#if WITH_EDITOR
    // Optional: draw debug volume in editor
    DrawDebugBox(GetWorld(), BreedingZone->GetComponentLocation(),
                 BreedingZone->GetScaledBoxExtent(),
                 FQuat::Identity, ZoneColor, false, -1.f, 0, 2.f);
#endif
}

