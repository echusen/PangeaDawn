// Fill out your copyright notice in the Description page of Project Settings.

#include "Objects/Requirements/Req_HasItems.h"

#include "GameFramework/Pawn.h"

#include "Components/ACFInventoryComponent.h"

bool UReq_HasItems::IsRequirementMetForContext(const FBaseUpgradeContext& Context) const
{
	return IsRequirementMet(Context.InteractingPawn != nullptr ? static_cast<UObject*>(Context.InteractingPawn) : Context.GetPreferredRequirementObject());
}

bool UReq_HasItems::IsRequirementMet_Implementation(UObject* ContextObject) const
{
	APawn* const Pawn = Cast<APawn>(ContextObject);
	if (Pawn == nullptr)
	{
		return false;
	}

	UACFInventoryComponent* const Inv = Pawn->FindComponentByClass<UACFInventoryComponent>();
	if (Inv == nullptr)
	{
		return false;
	}

	for (const FRequiredACFItem& Req : RequiredItems)
	{
		if (!Req.ItemClass || Req.Count <= 0)
		{
			return false;
		}

		const int32 Count = Inv->GetTotalCountOfItemsByClass(Req.ItemClass);

		if (Count < Req.Count)
		{
			return false;
		}
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
		return FText::FromString(TEXT("Missing Required Items"));
	}

	const FRequiredACFItem& Item = RequiredItems[0];

	if (!Item.ItemClass)
	{
		return FText::FromString(TEXT("Invalid Item Requirement"));
	}

	const UACFItem* const DefaultItem = Item.ItemClass->GetDefaultObject<UACFItem>();
	const FText ItemName = DefaultItem != nullptr ? DefaultItem->GetItemName() : FText::FromString(TEXT("Unknown Item"));

	return FText::Format(FText::FromString(TEXT("{0} x{1}")), ItemName, FText::AsNumber(Item.Count));
}
