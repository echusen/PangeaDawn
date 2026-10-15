// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "ACFCharacterToolset.h"

#include "ACFAIToolsUtils.h"
#include "Components/ACFInventoryComponent.h"
#include "Data/ACFCharacterDataAsset.h"
#include "Data/ACFCharacterFragment.h"
#include "Items/ACFItem.h"
#include "Kismet/KismetSystemLibrary.h"

namespace ACFCharacterToolsetImpl
{
	static const FName StartingItemsProperty = GET_MEMBER_NAME_CHECKED(UACFCharacterDataAsset, StartingItems);
	static const FName FragmentsProperty = GET_MEMBER_NAME_CHECKED(UACFCharacterDataAsset, Fragments);

	static TSet<FName> TypedProperties()
	{
		return { StartingItemsProperty, FragmentsProperty };
	}

	static void ApplyStartingItems(UACFCharacterDataAsset* Character, const FACFCharacterSpec& Spec, FACFToolReport& Report)
	{
		if (Spec.StartingItems.IsEmpty() && !Spec.bClearStartingItems)
		{
			return;
		}

		TArray<FStartingItem> Items;
		for (int32 Index = 0; Index < Spec.StartingItems.Num(); ++Index)
		{
			const FACFStartingItemSpec& ItemSpec = Spec.StartingItems[Index];
			const FString Prefix = FString::Printf(TEXT("StartingItems[%d]: "), Index);
			FString Error;
			UClass* ItemClass = ACFAIToolsUtils::ResolveClass(UACFItem::StaticClass(), ItemSpec.ItemClass, Error);
			if (!ItemClass)
			{
				Report.Errors.Add(Prefix + Error);
				continue;
			}
			if (ItemSpec.Count < 1)
			{
				Report.Errors.Add(Prefix + TEXT("Count must be at least 1"));
			}
			if (ItemSpec.DropChancePercentage < 0.f || ItemSpec.DropChancePercentage > 100.f)
			{
				Report.Errors.Add(Prefix + TEXT("DropChancePercentage must be between 0 and 100"));
			}

			FStartingItem Item;
			Item.ItemClass = ItemClass;
			Item.Count = ItemSpec.Count;
			Item.bAutoEquip = ItemSpec.bAutoEquip;
			Item.DropChancePercentage = ItemSpec.DropChancePercentage;
			Items.Add(Item);
			Report.AppliedProperties.Add(FString::Printf(TEXT("StartingItems[%d]"), Index));
		}

		Character->StartingItems = Items;
		FPropertyChangedEvent ChangedEvent(Character->GetClass()->FindPropertyByName(StartingItemsProperty), EPropertyChangeType::ValueSet);
		Character->PostEditChangeProperty(ChangedEvent);
	}

	static void ApplyFragments(UACFCharacterDataAsset* Character, const FACFCharacterSpec& Spec, FACFToolReport& Report)
	{
		if (Spec.Fragments.IsEmpty() && !Spec.bClearFragments)
		{
			return;
		}
		TArray<UObject*> Objects;
		FString Error;
		if (ACFAIToolsUtils::BuildInstancedObjects(Character, UACFCharacterFragment::StaticClass(), Spec.Fragments, TEXT("Fragments"),
				Objects, Report.AppliedProperties, Report.Errors)
			&& !ACFAIToolsUtils::SetObjectArray(Character, FragmentsProperty, Objects, Error))
		{
			Report.Errors.Add(Error);
		}
	}

	static FACFToolReport Run(const FACFCharacterSpec& Spec, bool bSave)
	{
		UClass* Class = UACFCharacterDataAsset::StaticClass();
		if (!Spec.DataAssetClass.IsEmpty())
		{
			FString Error;
			Class = ACFAIToolsUtils::ResolveClass(UACFCharacterDataAsset::StaticClass(), Spec.DataAssetClass, Error);
			if (!Class)
			{
				FACFToolReport Report;
				Report.Errors.Add(Error);
				return Report;
			}
		}

		return ACFAIToolsUtils::RunDataAssetRequest(Spec.AssetPath, Class, Spec.bOverwriteExisting, bSave, [&Spec](UObject* Target, FACFToolReport& Report)
		{
			UACFCharacterDataAsset* Character = CastChecked<UACFCharacterDataAsset>(Target);
			ACFAIToolsUtils::ApplyProperties(Character, Spec.Properties, TypedProperties(), FString(), Report.AppliedProperties, Report.Errors);
			ApplyStartingItems(Character, Spec, Report);
			ApplyFragments(Character, Spec, Report);

			if (!Character->Team.IsValid())
			{
				Report.Warnings.Add(TEXT("Team is not set"));
			}
			if (!Character->DefaultAbilitySet)
			{
				Report.Warnings.Add(TEXT("DefaultAbilitySet is not set; the character gets no abilities from this asset"));
			}
			if (Character->MeshComponents.IsEmpty())
			{
				Report.Warnings.Add(TEXT("MeshComponents is empty; the character keeps the mesh of its Blueprint"));
			}
		});
	}
}

TArray<FACFToolPropertyInfo> UACFCharacterToolset::DescribeCharacterData()
{
	return ACFAIToolsUtils::DescribeProperties(UACFCharacterDataAsset::StaticClass(), ACFCharacterToolsetImpl::TypedProperties());
}

TArray<FACFToolClassInfo> UACFCharacterToolset::ListCharacterFragmentClasses()
{
	return ACFAIToolsUtils::CollectClasses(UACFCharacterFragment::StaticClass());
}

TArray<FACFToolPropertyInfo> UACFCharacterToolset::DescribeCharacterFragment(const FString& Class)
{
	FString Error;
	UClass* Resolved = ACFAIToolsUtils::ResolveClass(UACFCharacterFragment::StaticClass(), Class, Error);
	if (!Resolved)
	{
		UKismetSystemLibrary::RaiseScriptError(Error);
		return {};
	}
	return ACFAIToolsUtils::DescribeProperties(Resolved, {});
}

FACFCharacterSpec UACFCharacterToolset::ExportCharacter(const FString& CharacterPath)
{
	FACFCharacterSpec Spec;
	FString Package, Error;
	if (!ACFAIToolsUtils::SplitAssetPath(CharacterPath, Package, Spec.AssetPath, Error))
	{
		UKismetSystemLibrary::RaiseScriptError(Error);
		return Spec;
	}
	const UACFCharacterDataAsset* Character = Cast<UACFCharacterDataAsset>(ACFAIToolsUtils::FindExistingAsset(Package, Spec.AssetPath));
	if (!Character)
	{
		UKismetSystemLibrary::RaiseScriptError(FString::Printf(TEXT("'%s' is not a Character Data Asset"), *CharacterPath));
		return Spec;
	}

	if (Character->GetClass() != UACFCharacterDataAsset::StaticClass())
	{
		Spec.DataAssetClass = Character->GetClass()->GetPathName();
	}
	Spec.bOverwriteExisting = true;
	Spec.Properties = ACFAIToolsUtils::ExportChangedProperties(Character, Character->GetClass()->GetDefaultObject(), ACFCharacterToolsetImpl::TypedProperties());
	for (const FStartingItem& Item : Character->StartingItems)
	{
		FACFStartingItemSpec ItemSpec;
		ItemSpec.ItemClass = Item.ItemClass ? Item.ItemClass->GetPathName() : FString();
		ItemSpec.Count = Item.Count;
		ItemSpec.bAutoEquip = Item.bAutoEquip;
		ItemSpec.DropChancePercentage = Item.DropChancePercentage;
		Spec.StartingItems.Add(ItemSpec);
	}
	for (const UACFCharacterFragment* Fragment : Character->Fragments)
	{
		if (Fragment)
		{
			Spec.Fragments.Add(ACFAIToolsUtils::ExportInstancedObject(Fragment));
		}
	}
	return Spec;
}

FACFToolReport UACFCharacterToolset::ValidateCharacter(const FACFCharacterSpec& Spec)
{
	return ACFAIToolsUtils::Finish(ACFCharacterToolsetImpl::Run(Spec, false), false);
}

FACFToolReport UACFCharacterToolset::SaveCharacter(const FACFCharacterSpec& Spec)
{
	return ACFAIToolsUtils::SaveChecked([&Spec](bool bSave) { return ACFCharacterToolsetImpl::Run(Spec, bSave); });
}
