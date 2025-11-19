// Fill out your copyright notice in the Description page of Project Settings.


#include "Actors/VillageBase.h"

#include "Components/BoxComponent.h"
#include "Components/FacilityManagerComponent.h"
#include "Components/UpgradeSystemComponent.h"


// Sets default values
AVillageBase::AVillageBase()
{
	PrimaryActorTick.bCanEverTick = false;

	// Root
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	// Bounds defining base size
	VillageBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("VillageBounds"));
	VillageBounds->SetupAttachment(RootComponent);
	VillageBounds->SetBoxExtent(FVector(2000,2000,400));
	VillageBounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Interaction zone
	InteractionVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionVolume"));
	InteractionVolume->SetupAttachment(RootComponent);
	InteractionVolume->SetBoxExtent(FVector(150,150,150));

	// Logic Components
	UpgradeSystem = CreateDefaultSubobject<UUpgradeSystemComponent>(TEXT("UpgradeSystem"));
	FacilityManager = CreateDefaultSubobject<UFacilityManagerComponent>(TEXT("FacilityManager"));

	// Default interaction text
	InteractionText = FText::FromString(TEXT("Upgrade Village"));
}

// Called when the game starts or when spawned
void AVillageBase::BeginPlay()
{
	Super::BeginPlay();
	
	
}

void AVillageBase::OnLocalInteractedByPawn_Implementation(class APawn* Pawn, const FString& interactionType)
{
	if (!UpgradeSystem)
		return;
	
	if (UpgradeSystem->CanUpgradeToNextLevel(Pawn))
	{
		// Trigger upgrade logic
		const int32 NewLevel = UpgradeSystem->CurrentLevel + 1;

		UpgradeSystem->OnLevelIncreased(NewLevel, Pawn);

		UE_LOG(LogTemp, Log, TEXT("VillageBase: Player %s triggered upgrade to level %d"),
			*GetNameSafe(Pawn), NewLevel);
	}
}


