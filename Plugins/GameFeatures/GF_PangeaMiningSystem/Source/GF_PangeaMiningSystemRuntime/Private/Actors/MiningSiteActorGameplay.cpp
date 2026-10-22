#include "Actors/MiningSiteActor.h"

#include "Actors/ACFCharacter.h"

#include "Components/ACFInteractionComponent.h"
#include "Components/ACFStorageComponent.h"
#include "Components/MiningSiteComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/TextRenderComponent.h"
#include "DataAssets/MiningSiteDefinition.h"
#include "Items/ACFItem.h"
#include "Blueprint/UserWidget.h"
#include "Actors/MiningScreenHost.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

DEFINE_LOG_CATEGORY_STATIC(LogPangeaMiningSiteActorInteraction, Log, All);

void AMiningSiteActor::HandleSiteLevelChanged(int32 OldLevel, int32 NewLevel)
{
	UE_LOG(LogPangeaMiningSiteActorInteraction, Log, TEXT("Mining site actor received level change. Actor=%s OldLevel=%d NewLevel=%d"),
		*GetNameSafe(this),
		OldLevel,
		NewLevel);
	RefreshLevelVisuals();
	RefreshSiteChest();
	ConfigureSmartObjectComponents();
	RefreshPresentationActors();
	UpdatePresentationActorMovement();
	UpdateStatusText();
}

void AMiningSiteActor::HandleShipmentResolved(const FMiningItemQuantity& RequestedShipment, int32 DeliveredQuantity, bool bLost)
{
	UACFStorageComponent* SettlementStorage = SettlementResourceActor ? SettlementResourceActor->FindComponentByClass<UACFStorageComponent>() : nullptr;
	const TSubclassOf<UACFItem> DeliveredItemClass = RequestedShipment.ItemClass && RequestedShipment.ItemClass->IsChildOf(UACFItem::StaticClass())
		? RequestedShipment.ItemClass.Get()
		: nullptr;

	if (!bLost && DeliveredQuantity > 0 && SettlementStorage && DeliveredItemClass)
	{
		SettlementStorage->AddItem(FBaseItem(DeliveredItemClass, DeliveredQuantity));
	}

	UE_LOG(LogPangeaMiningSiteActorInteraction, Log, TEXT("Mining site shipment handled. Actor=%s Settlement=%s ItemClass=%s Delivered=%d Lost=%s"),
		*GetNameSafe(this),
		*GetNameSafe(SettlementResourceActor),
		*GetNameSafe(DeliveredItemClass.Get()),
		DeliveredQuantity,
		bLost ? TEXT("true") : TEXT("false"));
}

void AMiningSiteActor::OnInteractedByPawn_Implementation(APawn* Pawn, const FString& interactionType)
{
	if (interactionType == TEXT("Mining.RecruitWorker"))
	{
		// Routed by the pawn-owned ACF InteractionComponent, not an RPC on the unowned site.
		if (HasAuthority() && CanRecruitFromInteraction(Pawn))
			MiningSiteComponent->RecruitWorker(ResolveUpgradeCostContext(Pawn));
		return;
	}
	UE_LOG(LogPangeaMiningSiteActorInteraction, Log, TEXT("Mining site interaction accepted. Actor=%s Interactor=%s CanUpgrade=%s Level=%d"),
		*GetNameSafe(this),
		*GetNameSafe(Pawn),
		CanUpgradeFromInteraction(Pawn) ? TEXT("true") : TEXT("false"),
		MiningSiteComponent ? MiningSiteComponent->GetCurrentLevel() : INDEX_NONE);
}

void AMiningSiteActor::OnLocalInteractedByPawn_Implementation(APawn* Pawn, const FString& interactionType)
{
	if (interactionType == TEXT("Mining.RecruitWorker")) return;
	UE_LOG(LogPangeaMiningSiteActorInteraction, Log, TEXT("Mining site locally interacted. Actor=%s Pawn=%s Level=%d"),
		*GetNameSafe(this),
		*GetNameSafe(Pawn),
		MiningSiteComponent ? MiningSiteComponent->GetCurrentLevel() : INDEX_NONE);
	OpenInteractionMenu(Pawn);
}

void AMiningSiteActor::UpdateStatusText()
{
	if (!StatusText || !MiningSiteComponent)
	{
		return;
	}

	StatusText->SetText(FText::FromString(FString::Printf(TEXT("Mining Site L%d\nStored %d / %d"),
		MiningSiteComponent->GetCurrentLevel(),
		MiningSiteComponent->GetStoredUnits(),
		MiningSiteComponent->GetStorageCapacity())));
}

void AMiningSiteActor::OnInteractableRegisteredByPawn_Implementation(APawn* Pawn)
{
	UE_LOG(LogPangeaMiningSiteActorInteraction, Log, TEXT("Mining site interactable registered. Actor=%s Pawn=%s"),
		*GetNameSafe(this),
		*GetNameSafe(Pawn));
}

void AMiningSiteActor::OnInteractableUnregisteredByPawn_Implementation(APawn* Pawn)
{
	UE_LOG(LogPangeaMiningSiteActorInteraction, Log, TEXT("Mining site interactable unregistered. Actor=%s Pawn=%s"),
		*GetNameSafe(this),
		*GetNameSafe(Pawn));
}

FText AMiningSiteActor::GetInteractableName_Implementation()
{
	if (!MiningSiteComponent || !MiningSiteComponent->IsEstablished())
	{
		return FText::GetEmpty();
	}

	if (MiningSiteComponent->CanUpgradeSite())
	{
		return FText::Format(FText::FromString(TEXT("Upgrade Mining Site (Level {0})")), FText::AsNumber(MiningSiteComponent->GetCurrentLevel()));
	}

	return FText::Format(FText::FromString(TEXT("Mining Site (Level {0})")), FText::AsNumber(MiningSiteComponent->GetCurrentLevel()));
}

bool AMiningSiteActor::CanBeInteracted_Implementation(APawn* Pawn)
{
	return Pawn != nullptr && MiningSiteComponent && MiningSiteComponent->IsEstablished();
}

void AMiningSiteActor::OpenInteractionMenu(APawn* InteractingPawn)
{
	if (!InteractingPawn)
	{
		return;
	}

	if (ActiveMenuWidget.IsValid())
	{
		MiningScreenHost::Close(ActiveMenuWidget.Get());
	}
	ActiveMenuWidget = nullptr;

	APlayerController* PlayerController = Cast<APlayerController>(InteractingPawn->GetController());
	if (!PlayerController || !PlayerController->IsLocalController() || !PlayerController->GetLocalPlayer())
	{
		return;
	}

	ActiveMenuWidget = MiningScreenHost::Open(PlayerController, TEXT("UI.Widget.Pangea.MiningSite"));
	if (!ActiveMenuWidget.IsValid())
	{
		return;
	}

	if (UFunction* InitializeFunction = ActiveMenuWidget.Get()->FindFunction(TEXT("InitializeFromSite")))
	{
		struct FInitializeFromSiteParams
		{
			AMiningSiteActor* InSiteActor;
			APawn* InInteractingPawn;
		};

		FInitializeFromSiteParams Params{ this, InteractingPawn };
		ActiveMenuWidget.Get()->ProcessEvent(InitializeFunction, &Params);
	}


}

void AMiningSiteActor::ClearActiveMenuWidget(UUserWidget* Widget)
{
	if (!Widget || ActiveMenuWidget.Get() == Widget)
	{
		ActiveMenuWidget = nullptr;
	}
}

bool AMiningSiteActor::CanUpgradeFromInteraction(APawn* InteractingPawn) const
{
	if (!MiningSiteComponent || !MiningSiteComponent->IsEstablished() || !MiningSiteComponent->CanUpgradeSite())
	{
		return false;
	}

	TArray<FMiningItemQuantity> UpgradeCost;
	MiningSiteComponent->GetNextUpgradeCost(UpgradeCost);
	if (UpgradeCost.IsEmpty())
	{
		return true;
	}

	UObject* CostContext = const_cast<UObject*>(ResolveUpgradeCostContext(InteractingPawn));
	const bool bCanUpgrade = MiningSiteComponent->CanPurchaseUpgrade(CostContext);
	UE_LOG(LogPangeaMiningSiteActorInteraction, Verbose, TEXT("CanUpgradeFromInteraction evaluated. Actor=%s Interactor=%s CostContext=%s Settlement=%s Result=%s Level=%d"),
		*GetNameSafe(this),
		*GetNameSafe(InteractingPawn),
		*GetNameSafe(CostContext),
		*GetNameSafe(SettlementResourceActor),
		bCanUpgrade ? TEXT("true") : TEXT("false"),
		MiningSiteComponent->GetCurrentLevel());
	return bCanUpgrade;
}

void AMiningSiteActor::ServerPurchaseNextUpgrade_Implementation(APawn* InteractingPawn)
{
	if (!MiningSiteComponent || !MiningSiteComponent->IsEstablished())
	{
		return;
	}

	UObject* CostContext = ResolveUpgradeCostContext(InteractingPawn);
	const bool bResult = MiningSiteComponent->PurchaseUpgrade(CostContext);
	UE_LOG(LogPangeaMiningSiteActorInteraction, Log, TEXT("ServerPurchaseNextUpgrade resolved. Actor=%s Interactor=%s CostContext=%s Result=%s Level=%d"),
		*GetNameSafe(this),
		*GetNameSafe(InteractingPawn),
		*GetNameSafe(CostContext),
		bResult ? TEXT("true") : TEXT("false"),
		MiningSiteComponent->GetCurrentLevel());
}

void AMiningSiteActor::ServerSyncProduction_Implementation()
{
	if (!MiningSiteComponent)
	{
		return;
	}

	MiningSiteComponent->SyncProductionFromWorldTime();
	UpdateStatusText();
	UE_LOG(LogPangeaMiningSiteActorInteraction, Log, TEXT("ServerSyncProduction resolved. Actor=%s Stored=%d/%d Level=%d"),
		*GetNameSafe(this),
		MiningSiteComponent->GetStoredUnits(),
		MiningSiteComponent->GetStorageCapacity(),
		MiningSiteComponent->GetCurrentLevel());
}

void AMiningSiteActor::ServerAdvanceOneSimulatedDay_Implementation()
{
	if (!MiningSiteComponent || !MiningSiteComponent->SiteDefinition)
	{
		return;
	}

	const float SimulatedDaySeconds = FMath::Max(1.0f, MiningSiteComponent->SiteDefinition->SimulatedDaySeconds);
	const int32 ProducedQuantity = MiningSiteComponent->ProduceForElapsedSeconds(SimulatedDaySeconds);
	MiningSiteComponent->TryAutoShipment();
	UpdateStatusText();

	UE_LOG(LogPangeaMiningSiteActorInteraction, Log, TEXT("ServerAdvanceOneSimulatedDay resolved. Actor=%s Produced=%d Stored=%d/%d Level=%d"),
		*GetNameSafe(this),
		ProducedQuantity,
		MiningSiteComponent->GetStoredUnits(),
		MiningSiteComponent->GetStorageCapacity(),
		MiningSiteComponent->GetCurrentLevel());
}

void AMiningSiteActor::RefreshLocalInteractionRegistration()
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

void AMiningSiteActor::RegisterWithInteractionComponent(UACFInteractionComponent* InteractionComponent)
{
	if (!InteractionComponent)
	{
		return;
	}

	InteractionComponent->RegisterInteractable(this);
	RegisteredInteractionComponents.AddUnique(InteractionComponent);
}

void AMiningSiteActor::UnregisterFromInteractionComponent(UACFInteractionComponent* InteractionComponent)
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

void AMiningSiteActor::RegisterChestWithInteractionComponent(UACFInteractionComponent* InteractionComponent)
{
	if (!InteractionComponent || !SpawnedSiteChest)
	{
		return;
	}

	InteractionComponent->RegisterInteractable(SpawnedSiteChest);
}

void AMiningSiteActor::UnregisterChestFromInteractionComponent(UACFInteractionComponent* InteractionComponent)
{
	if (!InteractionComponent || !SpawnedSiteChest)
	{
		return;
	}

	InteractionComponent->UnregisterInteractable(SpawnedSiteChest);
}

UObject* AMiningSiteActor::ResolveUpgradeCostContext(APawn* InteractingPawn) const
{
	if (SettlementResourceActor)
	{
		return SettlementResourceActor;
	}

	return InteractingPawn;
}

void AMiningSiteActor::HandleInteractionBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (APawn* OverlappingPawn = Cast<APawn>(OtherActor))
	{
		RegisterWithInteractionComponent(OverlappingPawn->FindComponentByClass<UACFInteractionComponent>());
		UE_LOG(LogPangeaMiningSiteActorInteraction, Log, TEXT("Mining site overlap begin. Actor=%s Interactor=%s"), *GetNameSafe(this), *GetNameSafe(OverlappingPawn));
	}
}

void AMiningSiteActor::HandleInteractionEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (APawn* OverlappingPawn = Cast<APawn>(OtherActor))
	{
		UnregisterFromInteractionComponent(OverlappingPawn->FindComponentByClass<UACFInteractionComponent>());
		UE_LOG(LogPangeaMiningSiteActorInteraction, Log, TEXT("Mining site overlap end. Actor=%s Interactor=%s"), *GetNameSafe(this), *GetNameSafe(OverlappingPawn));
	}
}

void AMiningSiteActor::HandleSiteChestInteractionBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (APawn* OverlappingPawn = Cast<APawn>(OtherActor))
	{
		RegisterChestWithInteractionComponent(OverlappingPawn->FindComponentByClass<UACFInteractionComponent>());
		UE_LOG(LogPangeaMiningSiteActorInteraction, Log, TEXT("Mining site chest overlap begin. Site=%s Chest=%s Interactor=%s"),
			*GetNameSafe(this),
			*GetNameSafe(SpawnedSiteChest),
			*GetNameSafe(OverlappingPawn));
	}
}

void AMiningSiteActor::HandleSiteChestInteractionEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (APawn* OverlappingPawn = Cast<APawn>(OtherActor))
	{
		UnregisterChestFromInteractionComponent(OverlappingPawn->FindComponentByClass<UACFInteractionComponent>());
		UE_LOG(LogPangeaMiningSiteActorInteraction, Log, TEXT("Mining site chest overlap end. Site=%s Chest=%s Interactor=%s"),
			*GetNameSafe(this),
			*GetNameSafe(SpawnedSiteChest),
			*GetNameSafe(OverlappingPawn));
	}
}
