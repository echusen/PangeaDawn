// Fill out your copyright notice in the Description page of Project Settings.


#include "Actors/VillageBase.h"

#include "Components/BoxComponent.h"
#include "Components/FacilityManagerComponent.h"
#include "Components/UpgradeSystemComponent.h"
#include "Components/GameFrameworkComponentManager.h"
#include "DataAssets/BaseUpgradeDefinition.h"
#include "Definitions/Fragments/BasePresentationFragment.h"
#include "Interfaces/PDBaseUpgradeInterface.h"
#include "UI/VillageUpgradeMenuWidget.h"
#include "ANSNavPageWidget.h"
#include "ANSUIPlayerSubsystem.h"
#include "Engine/GameInstance.h"
#include "Blueprint/WidgetBlueprintLibrary.h"


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

	// Default interaction text
	InteractionText = FText::GetEmpty();
}

void AVillageBase::PreInitializeComponents()
{
	Super::PreInitializeComponents();
	UGameFrameworkComponentManager::AddGameFrameworkComponentReceiver(this);
}

// Called when the game starts or when spawned
void AVillageBase::BeginPlay()
{
	Super::BeginPlay();

	UGameFrameworkComponentManager::SendGameFrameworkComponentExtensionEvent(
		this,
		UGameFrameworkComponentManager::NAME_GameActorReady);

	if (UPangeaUpgradeSystemComponent* UpgradeSystem = GetUpgradeSystem())
	{
		if (UpgradeSystem->UpgradeDefinition)
		{
			if (const UBasePresentationFragment* Presentation = UpgradeSystem->UpgradeDefinition->GetFragment<UBasePresentationFragment>())
			{
				if (!Presentation->InteractionText.IsEmpty())
				{
					InteractionText = Presentation->InteractionText;
				}
			}
		}
	}
}

void AVillageBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	TArray<UUserWidget*> Widgets;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, Widgets, UVillageUpgradeMenuWidget::StaticClass(), false);
	for (UUserWidget* Widget : Widgets)
	{
		UVillageUpgradeMenuWidget* Menu = Cast<UVillageUpgradeMenuWidget>(Widget);
		if (Menu && Menu->VillageActor == this)
			if (UANSNavPageWidget* Page = Menu->GetTypedOuter<UANSNavPageWidget>()) Page->RemoveSelf();
	}
	UGameFrameworkComponentManager::RemoveGameFrameworkComponentReceiver(this);
	Super::EndPlay(EndPlayReason);
}

TArray<UActorComponent*> AVillageBase::GetComponentsToSave_Implementation() const
{
	TArray<UActorComponent*> ComponentsToSave;

	if (UPangeaUpgradeSystemComponent* UpgradeSystem = GetUpgradeSystem())
	{
		ComponentsToSave.Add(UpgradeSystem);
	}

	if (UPangeaFacilityManagerComponent* FacilityManager = GetFacilityManager())
	{
		ComponentsToSave.Add(FacilityManager);
	}

	return ComponentsToSave;
}

void AVillageBase::OnLoaded_Implementation()
{
	if (UActorComponent* UpgradeProvider = GetUpgradeProviderComponent())
	{
		IPDBaseUpgradeInterface::Execute_LoadCompletedMilestonesForContext(UpgradeProvider, this);
	}
}

void AVillageBase::OnLocalInteractedByPawn_Implementation(class APawn* Pawn, const FString& interactionType)
{
	if (!HasActiveUpgradeFeature())
		return;
	
	OpenUpgradeMenu(Pawn);
}

bool AVillageBase::CanBeInteracted_Implementation(class APawn* Pawn)
{
	return HasActiveUpgradeFeature();
}

FText AVillageBase::GetInteractableName_Implementation()
{
	return HasActiveUpgradeFeature() ? InteractionText : FText::GetEmpty();
}

bool AVillageBase::UpgradeBase(APawn* InstigatorPawn) const
{
	UActorComponent* UpgradeProvider = GetUpgradeProviderComponent();
	if (!UpgradeProvider)
		return false;

	if (!IPDBaseUpgradeInterface::Execute_CanUpgradeToNextLevelForContext(UpgradeProvider, InstigatorPawn))
	{
		UE_LOG(LogTemp, Warning, TEXT("PerformUpgrade: Upgrade failed — requirements not met."));
		return false;
	}

	const bool bSuccess = IPDBaseUpgradeInterface::Execute_TryUpgradeToNextLevel(UpgradeProvider, InstigatorPawn);
	if (!bSuccess)
	{
		return false;
	}

	UE_LOG(LogTemp, Log,
		TEXT("VillageBase: %s upgraded village to level %d"),
		*GetNameSafe(InstigatorPawn),
		IPDBaseUpgradeInterface::Execute_GetCurrentUpgradeLevel(UpgradeProvider));

	return true;
}

void AVillageBase::OpenUpgradeMenu(APawn* InteractingPawn)
{
    APlayerController* PC = Cast<APlayerController>(InteractingPawn ? InteractingPawn->GetController() : nullptr);
    if (!PC || !PC->IsLocalController() || !PC->GetLocalPlayer()) return;
    UANSUIPlayerSubsystem* UI = PC->GetGameInstance()->GetSubsystem<UANSUIPlayerSubsystem>();
    UANSNavPageWidget* Page = UI ? Cast<UANSNavPageWidget>(UI->SpawnWidgetByTag(FGameplayTag::RequestGameplayTag(TEXT("UI.Widget.Pangea.Village")))) : nullptr;
    UVillageUpgradeMenuWidget* Menu = Page ? Cast<UVillageUpgradeMenuWidget>(Page->GetWidgetFromName(TEXT("ScreenContent"))) : nullptr;
    if (!Menu)
    {
        if (Page) Page->RemoveSelf();
        UE_LOG(LogTemp, Error, TEXT("Pangea village page is missing its upgrade content"));
        return;
    }
    Menu->InitializeFromVillage(this, InteractingPawn);
}
UPangeaUpgradeSystemComponent* AVillageBase::GetUpgradeSystem() const
{
	return FindComponentByClass<UPangeaUpgradeSystemComponent>();
}

UPangeaFacilityManagerComponent* AVillageBase::GetFacilityManager() const
{
	return FindComponentByClass<UPangeaFacilityManagerComponent>();
}

UActorComponent* AVillageBase::GetUpgradeProviderComponent() const
{
	return FindComponentByInterface(UPDBaseUpgradeInterface::StaticClass());
}

bool AVillageBase::HasActiveUpgradeFeature() const
{
	if (const UPangeaUpgradeSystemComponent* UpgradeSystem = GetUpgradeSystem())
	{
		return UpgradeSystem->UpgradeDefinition != nullptr;
	}

	return false;
}

