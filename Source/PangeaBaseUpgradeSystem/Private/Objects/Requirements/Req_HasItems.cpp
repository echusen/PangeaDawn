// Fill out your copyright notice in the Description page of Project Settings.


#include "Objects/Requirements/Req_HasItems.h"

#include "Components/ACFInventoryComponent.h"

bool UReq_HasItems::IsRequirementMet_Implementation(UObject* ContextObject) const
{
	APawn* Pawn = Cast<APawn>(ContextObject);
	if (!Pawn)
		return false;

	UACFInventoryComponent* Inv = Pawn->FindComponentByClass<UACFInventoryComponent>();
	if (!Inv)
		return false;

	for (const FRequiredACFItem& Req : RequiredItems)
	{
		if (!Req.ItemClass)
			continue;

		const int32 Count = Inv->GetTotalCountOfItemsByClass(Req.ItemClass);

		if (Count < Req.Count)
			return false;
	}

	return true;
}

FText UReq_HasItems::GetFailureMessage() const
{
	// Simple message - designers can make better ones in a blueprint subclass.
	return FText::FromString(TEXT("Required items are missing."));
}

FText UReq_HasItems::GetRequirementDescription_Implementation() const
{
	if (RequiredItems.Num() == 0)
	{
		return FText::FromString("Missing Required Items");
	}

	const auto& Item = RequiredItems[0];

	if (!Item.ItemClass)
	{
		return FText::FromString("Invalid Item Requirement");
	}

	const UACFItem* DefaultItem = Item.ItemClass->GetDefaultObject<UACFItem>();
	const FText ItemName = DefaultItem ? DefaultItem->GetItemName() : FText::FromString("Unknown Item");

	return FText::Format(
		FText::FromString("{0} x{1}"),
		ItemName,
		FText::AsNumber(Item.Count)
	);
}
