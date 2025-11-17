// Fill out your copyright notice in the Description page of Project Settings.


#include "Actions/UA_GrantInventory.h"

#include "Components/ACFInventoryComponent.h"

void UUA_GrantInventory::Execute_Implementation(UObject* ContextObject)
{
	if (!ContextObject) return;

	APawn* Pawn = Cast<APawn>(ContextObject);
	if (!Pawn)
	{
		UE_LOG(LogTemp, Warning, TEXT("UA_GrantInventory: Context is not a pawn"));
		return;
	}

	UACFInventoryComponent* Inv = Pawn->FindComponentByClass<UACFInventoryComponent>();
	if (!Inv)
	{
		UE_LOG(LogTemp, Warning, TEXT("UA_GrantInventory: No inventory component found on pawn %s"), *GetNameSafe(Pawn));
		return;
	}

	// Iterate through items array and add each one
	for (const FBaseItem& Item : ItemsToGrant)
	{
		Inv->AddItemToInventory(Item, false); // Set bAutoEquip as needed
		UE_LOG(LogTemp, Log, TEXT("UA_GrantInventory: Granted item to %s"), *Pawn->GetName());
	}
}
