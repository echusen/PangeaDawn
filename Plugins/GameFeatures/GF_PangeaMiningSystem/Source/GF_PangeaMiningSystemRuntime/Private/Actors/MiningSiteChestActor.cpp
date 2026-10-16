#include "Actors/MiningSiteChestActor.h"

#include "Components/ACFInteractionComponent.h"
#include "Components/ACFInventoryComponent.h"
#include "Components/ACFStorageComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/World.h"
#include "Blueprint/UserWidget.h"
#include "Actors/MiningScreenHost.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Items/ACFItem.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogPangeaMiningSiteChest, Log, All);

AMiningSiteChestActor::AMiningSiteChestActor()
{
	bReplicates = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	ChestMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ChestMesh"));
	ChestMesh->SetupAttachment(SceneRoot);
	ChestMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

	InteractionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionSphere"));
	InteractionSphere->SetupAttachment(SceneRoot);
	InteractionSphere->SetSphereRadius(140.0f);
	InteractionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	StatusText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("StatusText"));
	StatusText->SetupAttachment(SceneRoot);
	StatusText->SetRelativeLocation(FVector(0.0f, 0.0f, 120.0f));
	StatusText->SetHorizontalAlignment(EHorizTextAligment::EHTA_Center);
	StatusText->SetWorldSize(24.0f);
	StatusText->SetText(FText::FromString(TEXT("Mining Chest")));

	StorageComponent = CreateDefaultSubobject<UACFStorageComponent>(TEXT("StorageComponent"));
	if (StorageComponent)
	{
		StorageComponent->SetMaxInventorySlots(1000);
		StorageComponent->SetMaxInventoryWeight(1000000);
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> ChestAsset(TEXT("/Game/Environment/Oppidam/Decoration/SM_Oppi_Deco_Chest_AD.SM_Oppi_Deco_Chest_AD"));
	if (ChestAsset.Succeeded())
	{
		ChestMesh->SetStaticMesh(ChestAsset.Object);
		ChestMesh->SetRelativeScale3D(FVector(0.9f));
	}
}

void AMiningSiteChestActor::BeginPlay()
{
	Super::BeginPlay();
	if (StatusText) StatusText->SetVisibility(false);

	UE_LOG(LogPangeaMiningSiteChest, Log, TEXT("Mining chest BeginPlay. Chest=%s World=%s Location=%s Owner=%s"),
		*GetNameSafe(this),
		*GetNameSafe(GetWorld()),
		*GetActorLocation().ToString(),
		*GetNameSafe(GetOwner()));

	InteractionSphere->OnComponentBeginOverlap.AddDynamic(this, &ThisClass::HandleInteractionBegin);
	InteractionSphere->OnComponentEndOverlap.AddDynamic(this, &ThisClass::HandleInteractionEnd);
	RefreshLocalInteractionRegistration();

	if (StorageComponent)
	{
		StorageComponent->OnInventoryChanged.AddDynamic(this, &ThisClass::UpdateStatusText);
	}

	UpdateStatusText();
}

void AMiningSiteChestActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	MiningScreenHost::Close(ActiveChestWidget.Get());
	UE_LOG(LogPangeaMiningSiteChest, Warning, TEXT("Mining chest EndPlay. Chest=%s World=%s Location=%s Owner=%s Reason=%d"),
		*GetNameSafe(this),
		*GetNameSafe(GetWorld()),
		*GetActorLocation().ToString(),
		*GetNameSafe(GetOwner()),
		static_cast<int32>(EndPlayReason));

	for (const TWeakObjectPtr<UACFInteractionComponent>& InteractionComponent : RegisteredInteractionComponents)
	{
		if (InteractionComponent.IsValid())
		{
			InteractionComponent->UnregisterInteractable(this);
		}
	}

	RegisteredInteractionComponents.Reset();
	Super::EndPlay(EndPlayReason);
}

void AMiningSiteChestActor::OpenChestMenu(APawn* InteractingPawn)
{
	if (!InteractingPawn)
	{
		return;
	}

	if (ActiveChestWidget.IsValid())
	{
		MiningScreenHost::Close(ActiveChestWidget.Get());
	}
	ActiveChestWidget = nullptr;

	APlayerController* PlayerController = Cast<APlayerController>(InteractingPawn->GetController());
	if (!PlayerController || !PlayerController->IsLocalController() || !PlayerController->GetLocalPlayer())
	{
		return;
	}

	ActiveChestWidget = MiningScreenHost::Open(PlayerController, TEXT("UI.Widget.Pangea.MiningChest"));
	if (!ActiveChestWidget.IsValid())
	{
		return;
	}

	if (UFunction* InitializeFunction = ActiveChestWidget.Get()->FindFunction(TEXT("InitializeFromChest")))
	{
		struct FInitializeFromChestParams
		{
			AMiningSiteChestActor* InChestActor;
			APawn* InInteractingPawn;
		};

		FInitializeFromChestParams Params{ this, InteractingPawn };
		ActiveChestWidget.Get()->ProcessEvent(InitializeFunction, &Params);
	}


}

void AMiningSiteChestActor::ClearActiveChestWidget(UUserWidget* Widget)
{
	if (!Widget || ActiveChestWidget.Get() == Widget)
	{
		ActiveChestWidget = nullptr;
	}
}

void AMiningSiteChestActor::ServerWithdrawAllToPawn_Implementation(APawn* InteractingPawn)
{
	if (!InteractingPawn || !StorageComponent)
	{
		return;
	}

	if (UACFInventoryComponent* InventoryComponent = InteractingPawn->FindComponentByClass<UACFInventoryComponent>())
	{
		InventoryComponent->MoveItemsFromInventory(StorageComponent->GetInventory(), StorageComponent);
		UpdateStatusText();
		UE_LOG(LogPangeaMiningSiteChest, Log, TEXT("Mining chest withdrew all items. Chest=%s Pawn=%s RemainingStacks=%d"),
			*GetNameSafe(this),
			*GetNameSafe(InteractingPawn),
			StorageComponent->GetInventory().Num());
	}
}

void AMiningSiteChestActor::OnInteractedByPawn_Implementation(APawn* Pawn, const FString& interactionType)
{
	UE_LOG(LogPangeaMiningSiteChest, Log, TEXT("Mining chest interacted. Chest=%s Pawn=%s"),
		*GetNameSafe(this),
		*GetNameSafe(Pawn));
}

void AMiningSiteChestActor::OnLocalInteractedByPawn_Implementation(APawn* Pawn, const FString& interactionType)
{
	UE_LOG(LogPangeaMiningSiteChest, Log, TEXT("Mining chest locally interacted. Chest=%s Pawn=%s"),
		*GetNameSafe(this),
		*GetNameSafe(Pawn));
	OpenChestMenu(Pawn);
}

void AMiningSiteChestActor::OnInteractableRegisteredByPawn_Implementation(APawn* Pawn)
{
	UE_LOG(LogPangeaMiningSiteChest, Log, TEXT("Mining chest registered. Chest=%s Pawn=%s"),
		*GetNameSafe(this),
		*GetNameSafe(Pawn));
}

void AMiningSiteChestActor::OnInteractableUnregisteredByPawn_Implementation(APawn* Pawn)
{
	UE_LOG(LogPangeaMiningSiteChest, Log, TEXT("Mining chest unregistered. Chest=%s Pawn=%s"),
		*GetNameSafe(this),
		*GetNameSafe(Pawn));
}

FText AMiningSiteChestActor::GetInteractableName_Implementation()
{
	return FText::FromString(TEXT("Open Mining Chest"));
}

bool AMiningSiteChestActor::CanBeInteracted_Implementation(APawn* Pawn)
{
	return Pawn != nullptr;
}

void AMiningSiteChestActor::UpdateStatusText()
{
	MiningScreenHost::Refresh(ActiveChestWidget.Get(), this, TEXT("InitializeFromChest"));
	if (!StatusText || !StorageComponent)
	{
		return;
	}

	int32 TotalCount = 0;
	for (const FInventoryItem& Item : StorageComponent->GetInventory())
	{
		TotalCount += Item.Count;
	}

	StatusText->SetText(FText::FromString(FString::Printf(TEXT("Mining Chest\n%d items"), TotalCount)));
}

void AMiningSiteChestActor::RefreshLocalInteractionRegistration()
{
	if (!InteractionSphere)
	{
		return;
	}

	TArray<AActor*> OverlappingActors;
	InteractionSphere->GetOverlappingActors(OverlappingActors, APawn::StaticClass());
	for (AActor* OverlappingActor : OverlappingActors)
	{
		if (APawn* OverlappingPawn = Cast<APawn>(OverlappingActor))
		{
			if (UACFInteractionComponent* InteractionComponent = OverlappingPawn->FindComponentByClass<UACFInteractionComponent>())
			{
				RegisterWithInteractionComponent(InteractionComponent);
			}
		}
	}
}

void AMiningSiteChestActor::RegisterWithInteractionComponent(UACFInteractionComponent* InteractionComponent)
{
	if (!InteractionComponent)
	{
		return;
	}

	InteractionComponent->RegisterInteractable(this);
	RegisteredInteractionComponents.AddUnique(InteractionComponent);
}

void AMiningSiteChestActor::UnregisterFromInteractionComponent(UACFInteractionComponent* InteractionComponent)
{
	if (!InteractionComponent)
	{
		return;
	}

	InteractionComponent->UnregisterInteractable(this);
	RegisteredInteractionComponents.RemoveAll([InteractionComponent](const TWeakObjectPtr<UACFInteractionComponent>& Entry)
	{
		return Entry.Get() == InteractionComponent;
	});
}

void AMiningSiteChestActor::HandleInteractionBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (APawn* OverlappingPawn = Cast<APawn>(OtherActor))
	{
		RegisterWithInteractionComponent(OverlappingPawn->FindComponentByClass<UACFInteractionComponent>());
	}
}

void AMiningSiteChestActor::HandleInteractionEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (APawn* OverlappingPawn = Cast<APawn>(OtherActor))
	{
		UnregisterFromInteractionComponent(OverlappingPawn->FindComponentByClass<UACFInteractionComponent>());
	}
}
