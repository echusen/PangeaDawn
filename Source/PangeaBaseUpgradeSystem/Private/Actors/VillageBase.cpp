// Fill out your copyright notice in the Description page of Project Settings.


#include "Actors/VillageBase.h"

#include "Components/BoxComponent.h"
#include "Components/FacilityManagerComponent.h"
#include "Components/UpgradeSystemComponent.h"
#include "UI/VillageUpgradeMenuWidget.h"


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

TArray<UActorComponent*> AVillageBase::GetComponentsToSave_Implementation() const
{
	TArray<UActorComponent*> ComponentsToSave;

	ComponentsToSave.Add(UpgradeSystem);
	ComponentsToSave.Add(FacilityManager);

	return ComponentsToSave;
}

void AVillageBase::OnLoaded_Implementation()
{
	if (UpgradeSystem)
	{
		UpgradeSystem->LoadCompletedMilestones(this);
	}
}

void AVillageBase::OnLocalInteractedByPawn_Implementation(class APawn* Pawn, const FString& interactionType)
{
	if (!UpgradeSystem)
		return;
	
	OpenUpgradeMenu(Pawn);
}

bool AVillageBase::UpgradeBase(APawn* InstigatorPawn) const
{
	if (!UpgradeSystem)
		return false;

	if (!UpgradeSystem->CanUpgradeToNextLevel(InstigatorPawn))
	{
		UE_LOG(LogTemp, Warning, TEXT("PerformUpgrade: Upgrade failed — requirements not met."));
		return false;
	}

	const int32 NewLevel = UpgradeSystem->CurrentLevel + 1;

	UpgradeSystem->OnLevelIncreased(NewLevel, InstigatorPawn);

	UE_LOG(LogTemp, Log,
		TEXT("VillageBase: %s upgraded village to level %d"),
		*GetNameSafe(InstigatorPawn),
		NewLevel);

	return true;
}

void AVillageBase::OpenUpgradeMenu(APawn* InteractingPawn)
{
	if (!UpgradeMenuClass)
		return;

	APlayerController* PC = Cast<APlayerController>(
		InteractingPawn ? InteractingPawn->GetController() : nullptr
	);
	if (!PC)
		return;

	UVillageUpgradeMenuWidget* Menu = CreateWidget<UVillageUpgradeMenuWidget>(PC, UpgradeMenuClass);
	if (!Menu)
		return;

	// Pass village & pawn to widget
	Menu->InitializeFromVillage(this, InteractingPawn);

	// Add UI
	Menu->AddToViewport();

	// Input setup
	PC->bShowMouseCursor = true;
	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(Menu->TakeWidget());
	PC->SetInputMode(InputMode);
}

