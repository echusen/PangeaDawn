// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.

#include "Items/ACFWorldItem.h"
#include "ACFItemSystemFunctionLibrary.h"
#include "Components/ACFCurrencyComponent.h"
#include "Components/ACFEquipmentComponent.h"
#include "Components/ACFStorageComponent.h"
#include "Items/ACFItem.h"
#include "Net/UnrealNetwork.h"
#include <Components/SphereComponent.h>
#include <Components/StaticMeshComponent.h>
#include <GameFramework/Pawn.h>
#include <Logging.h>
#include <UObject/SoftObjectPtr.h>

AACFWorldItem::AACFWorldItem()
{
	ObjectMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Object Mesh"));
	StorageComponent = CreateDefaultSubobject<UACFStorageComponent>(TEXT("StorageComponent"));
	bReplicates = true;
	RootComp = CreateDefaultSubobject<USceneComponent>(TEXT("RootComp"));
	SetRootComponent(RootComp);

	ObjectMesh->SetupAttachment(RootComp);

	/*	InteractableArea->SetCollisionEnabled(ECollisionEnabled::NoCollision);*/
	ObjectMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AACFWorldItem::BeginPlay()
{
	Super::BeginPlay();
	if (GetItems().IsValidIndex(0) && bUseWorldMeshFromFirstItem) {
		SetItemMesh(UACFItemSystemFunctionLibrary::MakeBaseItemFromInventory(GetItems()[0]));
	}
	if (HasAuthority() && !StorageComponent->OnItemRemoved.IsAlreadyBound(this, &AACFWorldItem::HandleStorageChanged)) {
		StorageComponent->OnItemRemoved.AddDynamic(this, &AACFWorldItem::HandleStorageChanged);
	}
}

void AACFWorldItem::SetItemMesh(const FBaseItem& inItem)
{
	if (ObjectMesh && inItem.ItemClass && StorageComponent) {
		if (UACFItemSystemFunctionLibrary::GetItemData(inItem.ItemClass, ItemInfo)) {

			if (ItemInfo.WorldMesh) {
				// put this Async?
				ObjectMesh->SetStaticMesh(ItemInfo.WorldMesh.LoadSynchronous());
			}
		}
	}
	else {
		UE_LOG(ACFInventoryLog, Log, TEXT("Trying to assign a wrong Item to the World Item!! - ACFWorldItem"));
	}
}

void AACFWorldItem::AddItem(const FBaseItem& inItem)
{
	if (StorageComponent) {
		StorageComponent->AddItem(inItem);
		SetItemMesh(inItem);
	}
}

void AACFWorldItem::AddCurrency(float currencyAmount)
{
	if (StorageComponent) {
		StorageComponent->AddCurrency(currencyAmount);
	}
}

void AACFWorldItem::OnInteractedByPawn_Implementation(APawn* Pawn, const FString& interactionType)
{
	if (Pawn) {
		TObjectPtr<UACFEquipmentComponent> equipComp = Pawn->FindComponentByClass<UACFEquipmentComponent>();
		if (equipComp && StorageComponent) {
			equipComp->MoveItemsFromInventory(StorageComponent->GetInventory(), StorageComponent);
			equipComp->GatherCurrency(StorageComponent->GetCurrentCurrencyAmount(), StorageComponent);
		}
		if (bDestroyOnGather) {
			Destroy();
		}
	}
}

void AACFWorldItem::OnLocalInteractedByPawn_Implementation(class APawn* Pawn, const FString& interactionType /*= ""*/)
{
}

void AACFWorldItem::OnInteractableRegisteredByPawn_Implementation(class APawn* Pawn)
{
}

void AACFWorldItem::OnInteractableUnregisteredByPawn_Implementation(class APawn* Pawn)
{
}

FText AACFWorldItem::GetInteractableName_Implementation()
{
	if (GetItems().IsValidIndex(0) && GetItems()[0].ItemClass)
	{
		FItemDescriptor itemData;
		UACFItemSystemFunctionLibrary::GetItemData(GetItems()[0].ItemClass, itemData);

		return FText::Format(
			FText::FromString("{0} x{1}"),
			itemData.Name,
			FText::AsNumber(GetItems()[0].Count)
		);
	}
	else if (StorageComponent && StorageComponent->GetCurrentCurrencyAmount() > 0.f)
	{
		return FText::Format(
			FText::FromString("{0} x{1}"),
			UACFItemSystemFunctionLibrary::GetDefaultCurrencyName(),
			FText::AsNumber(FMath::RoundToInt(StorageComponent->GetCurrentCurrencyAmount()))
		);
	}

	return FText::GetEmpty();
}

bool AACFWorldItem::CanBeInteracted_Implementation(class APawn* Pawn)
{
	return true;
}

TArray<FInventoryItem> AACFWorldItem::GetItems() const
{
	return StorageComponent->GetInventory();
}

void AACFWorldItem::HandleStorageChanged_Implementation(const FBaseItem& items)
{
	if (bDestroyOnGather && StorageComponent->IsStorageEmpty()) {
		Destroy(true);
	}
}

void AACFWorldItem::OnLoaded_Implementation()
{
	if (GetItems().IsValidIndex(0) && bUseWorldMeshFromFirstItem) {
		SetItemMesh(GetItems()[0]);
	}
}
