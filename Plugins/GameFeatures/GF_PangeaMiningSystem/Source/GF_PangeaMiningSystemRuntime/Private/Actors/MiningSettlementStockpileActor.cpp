#include "Actors/MiningSettlementStockpileActor.h"

#include "Blueprint/UserWidget.h"
#include "Actors/MiningScreenHost.h"
#include "Components/ACFInteractionComponent.h"
#include "Components/ACFInventoryComponent.h"
#include "Components/ACFStorageComponent.h"
#include "Components/PangeaMiningTransportStorageComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogPangeaMiningStockpile, Log, All);

AMiningSettlementStockpileActor::AMiningSettlementStockpileActor()
{
	bReplicates = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	StockpileMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StockpileMesh"));
	StockpileMesh->SetupAttachment(SceneRoot);
	StockpileMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

	StockpileLabel = CreateDefaultSubobject<UTextRenderComponent>(TEXT("StockpileLabel"));
	StockpileLabel->SetupAttachment(SceneRoot);
	StockpileLabel->SetRelativeLocation(FVector(0.0f, 0.0f, 180.0f));
	StockpileLabel->SetHorizontalAlignment(EHorizTextAligment::EHTA_Center);
	StockpileLabel->SetWorldSize(28.0f);
	StockpileLabel->SetText(FText::FromString(TEXT("Settlement Stockpile")));

	InteractionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionSphere"));
	InteractionSphere->SetupAttachment(SceneRoot);
	InteractionSphere->SetSphereRadius(180.0f);
	InteractionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	CourierUnloadMarker = CreateDefaultSubobject<USceneComponent>(TEXT("CourierUnloadMarker"));
	CourierUnloadMarker->SetupAttachment(SceneRoot);
	CourierUnloadMarker->SetRelativeLocation(FVector(120.0f, 0.0f, 0.0f));

	StorageComponent = CreateDefaultSubobject<UPangeaMiningTransportStorageComponent>(TEXT("StorageComponent"));
	if (StorageComponent != nullptr)
	{
		StorageComponent->SetMaxInventorySlots(1000);
		StorageComponent->SetMaxInventoryWeight(1000000);
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> ChestAsset(TEXT("/Game/Environment/Oppidam/Decoration/SM_Oppi_Deco_Chest_AD.SM_Oppi_Deco_Chest_AD"));
	if (ChestAsset.Succeeded())
	{
		StockpileMesh->SetStaticMesh(ChestAsset.Object);
		StockpileMesh->SetRelativeScale3D(FVector(1.5f));
	}
}

void AMiningSettlementStockpileActor::BeginPlay()
{
	Super::BeginPlay();
	if (StockpileLabel) StockpileLabel->SetVisibility(false);

	InteractionSphere->OnComponentBeginOverlap.AddDynamic(this, &ThisClass::HandleInteractionBegin);
	InteractionSphere->OnComponentEndOverlap.AddDynamic(this, &ThisClass::HandleInteractionEnd);
	RefreshLocalInteractionRegistration();

	if (StorageComponent)
	{
		StorageComponent->OnInventoryChanged.AddDynamic(this, &ThisClass::UpdateStatusText);
	}

	if (!HasAuthority() || !StorageComponent || !StorageComponent->GetInventory().IsEmpty())
	{
		UpdateStatusText();
		return;
	}

	for (const FBaseItem& Item : InitialStock)
	{
		if (Item.ItemClass && Item.Count > 0)
		{
			StorageComponent->AddItem(Item);
		}
	}

	UE_LOG(LogPangeaMiningStockpile, Log, TEXT("Settlement stockpile initialized. Actor=%s"), *GetNameSafe(this));
	UpdateStatusText();
}

void AMiningSettlementStockpileActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	MiningScreenHost::Close(ActiveStockpileWidget.Get());
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

TArray<UActorComponent*> AMiningSettlementStockpileActor::GetComponentsToSave_Implementation() const
{
	TArray<UActorComponent*> ComponentsToSave;

	if (StorageComponent)
	{
		ComponentsToSave.Add(StorageComponent);
	}

	return ComponentsToSave;
}

void AMiningSettlementStockpileActor::OnLoaded_Implementation()
{
	RefreshLocalInteractionRegistration();
	UpdateStatusText();
}

FVector AMiningSettlementStockpileActor::GetCourierUnloadLocation() const
{
	return CourierUnloadMarker ? CourierUnloadMarker->GetComponentLocation() : GetActorLocation();
}

void AMiningSettlementStockpileActor::OpenStockpileMenu(APawn* InteractingPawn)
{
	if (!InteractingPawn)
	{
		return;
	}

	if (ActiveStockpileWidget.IsValid())
	{
		MiningScreenHost::Close(ActiveStockpileWidget.Get());
	}
	ActiveStockpileWidget = nullptr;

	APlayerController* PlayerController = Cast<APlayerController>(InteractingPawn->GetController());
	if (!PlayerController || !PlayerController->IsLocalController() || !PlayerController->GetLocalPlayer())
	{
		return;
	}

	ActiveStockpileWidget = MiningScreenHost::Open(PlayerController, TEXT("UI.Widget.Pangea.Stockpile"));
	if (!ActiveStockpileWidget.IsValid())
	{
		return;
	}

	if (UFunction* InitializeFunction = ActiveStockpileWidget.Get()->FindFunction(TEXT("InitializeFromStockpile")))
	{
		struct FInitializeFromStockpileParams
		{
			AMiningSettlementStockpileActor* InStockpileActor;
			APawn* InInteractingPawn;
		};

		FInitializeFromStockpileParams Params{ this, InteractingPawn };
		ActiveStockpileWidget.Get()->ProcessEvent(InitializeFunction, &Params);
	}


}

void AMiningSettlementStockpileActor::ClearActiveStockpileWidget(UUserWidget* Widget)
{
	if (!Widget || ActiveStockpileWidget.Get() == Widget)
	{
		ActiveStockpileWidget = nullptr;
	}
}

void AMiningSettlementStockpileActor::ServerWithdrawAllToPawn_Implementation(APawn* InteractingPawn)
{
	if (!InteractingPawn || !StorageComponent)
	{
		return;
	}

	if (UACFInventoryComponent* InventoryComponent = InteractingPawn->FindComponentByClass<UACFInventoryComponent>())
	{
		InventoryComponent->MoveItemsFromInventory(StorageComponent->GetInventory(), StorageComponent);
		UpdateStatusText();
	}
}

void AMiningSettlementStockpileActor::ServerDepositAllFromPawn_Implementation(APawn* InteractingPawn)
{
	if (!InteractingPawn || !StorageComponent)
	{
		return;
	}

	if (UACFInventoryComponent* InventoryComponent = InteractingPawn->FindComponentByClass<UACFInventoryComponent>())
	{
		StorageComponent->MoveItemsFromInventory(InventoryComponent->GetInventory(), InventoryComponent);
		UpdateStatusText();
	}
}

void AMiningSettlementStockpileActor::ServerTransferItemToPawn_Implementation(APawn* InteractingPawn, TSubclassOf<UACFItem> ItemClass, int32 Count)
{
	if (!InteractingPawn || !StorageComponent || !ItemClass || Count <= 0)
	{
		return;
	}

	if (UACFInventoryComponent* InventoryComponent = InteractingPawn->FindComponentByClass<UACFInventoryComponent>())
	{
		for (const FInventoryItem& Item : StorageComponent->GetInventory())
		{
			if (Item.ItemClass != ItemClass || Item.Count <= 0)
			{
				continue;
			}

			FInventoryItem TransferItem = Item;
			TransferItem.Count = FMath::Min(Item.Count, Count);
			TArray<FInventoryItem> TransferItems{ TransferItem };
			InventoryComponent->MoveItemsFromInventory(TransferItems, StorageComponent);
			UpdateStatusText();
			return;
		}
	}
}

void AMiningSettlementStockpileActor::ServerTransferItemFromPawn_Implementation(APawn* InteractingPawn, TSubclassOf<UACFItem> ItemClass, int32 Count)
{
	if (!InteractingPawn || !StorageComponent || !ItemClass || Count <= 0)
	{
		return;
	}

	if (UACFInventoryComponent* InventoryComponent = InteractingPawn->FindComponentByClass<UACFInventoryComponent>())
	{
		for (const FInventoryItem& Item : InventoryComponent->GetInventory())
		{
			if (Item.ItemClass != ItemClass || Item.Count <= 0)
			{
				continue;
			}

			FInventoryItem TransferItem = Item;
			TransferItem.Count = FMath::Min(Item.Count, Count);
			TArray<FInventoryItem> TransferItems{ TransferItem };
			StorageComponent->MoveItemsFromInventory(TransferItems, InventoryComponent);
			UpdateStatusText();
			return;
		}
	}
}

void AMiningSettlementStockpileActor::OnInteractedByPawn_Implementation(APawn* Pawn, const FString& interactionType)
{
	UE_LOG(LogPangeaMiningStockpile, Log, TEXT("Settlement stockpile interacted. Actor=%s Pawn=%s"),
		*GetNameSafe(this),
		*GetNameSafe(Pawn));
}

void AMiningSettlementStockpileActor::OnLocalInteractedByPawn_Implementation(APawn* Pawn, const FString& interactionType)
{
	UE_LOG(LogPangeaMiningStockpile, Log, TEXT("Settlement stockpile locally interacted. Actor=%s Pawn=%s"),
		*GetNameSafe(this),
		*GetNameSafe(Pawn));
	OpenStockpileMenu(Pawn);
}

void AMiningSettlementStockpileActor::OnInteractableRegisteredByPawn_Implementation(APawn* Pawn)
{
	UE_LOG(LogPangeaMiningStockpile, Log, TEXT("Settlement stockpile registered. Actor=%s Pawn=%s"),
		*GetNameSafe(this),
		*GetNameSafe(Pawn));
}

void AMiningSettlementStockpileActor::OnInteractableUnregisteredByPawn_Implementation(APawn* Pawn)
{
	UE_LOG(LogPangeaMiningStockpile, Log, TEXT("Settlement stockpile unregistered. Actor=%s Pawn=%s"),
		*GetNameSafe(this),
		*GetNameSafe(Pawn));
}

FText AMiningSettlementStockpileActor::GetInteractableName_Implementation()
{
	return FText::FromString(TEXT("Open Settlement Stockpile"));
}

bool AMiningSettlementStockpileActor::CanBeInteracted_Implementation(APawn* Pawn)
{
	return Pawn != nullptr;
}

void AMiningSettlementStockpileActor::UpdateStatusText()
{
	MiningScreenHost::Refresh(ActiveStockpileWidget.Get(), this, TEXT("InitializeFromStockpile"));
	if (!StockpileLabel || !StorageComponent)
	{
		return;
	}

	int32 TotalCount = 0;
	for (const FInventoryItem& Item : StorageComponent->GetInventory())
	{
		TotalCount += Item.Count;
	}

	StockpileLabel->SetText(FText::FromString(FString::Printf(TEXT("Settlement Stockpile\n%d items"), TotalCount)));
}

void AMiningSettlementStockpileActor::RefreshLocalInteractionRegistration()
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

void AMiningSettlementStockpileActor::RegisterWithInteractionComponent(UACFInteractionComponent* InteractionComponent)
{
	if (!InteractionComponent)
	{
		return;
	}

	InteractionComponent->RegisterInteractable(this);
	RegisteredInteractionComponents.AddUnique(InteractionComponent);
}

void AMiningSettlementStockpileActor::UnregisterFromInteractionComponent(UACFInteractionComponent* InteractionComponent)
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

void AMiningSettlementStockpileActor::HandleInteractionBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (APawn* OverlappingPawn = Cast<APawn>(OtherActor))
	{
		RegisterWithInteractionComponent(OverlappingPawn->FindComponentByClass<UACFInteractionComponent>());
	}
}

void AMiningSettlementStockpileActor::HandleInteractionEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (APawn* OverlappingPawn = Cast<APawn>(OtherActor))
	{
		UnregisterFromInteractionComponent(OverlappingPawn->FindComponentByClass<UACFInteractionComponent>());
	}
}
