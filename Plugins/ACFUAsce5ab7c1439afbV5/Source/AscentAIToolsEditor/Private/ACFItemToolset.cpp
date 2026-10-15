// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "ACFItemToolset.h"

#include "ACFAIToolsUtils.h"
#include "Items/ACFItem.h"
#include "Items/ACFItemFragment.h"
#include "Kismet/KismetSystemLibrary.h"

namespace ACFItemToolsetImpl
{
	static const FName FragmentsProperty(TEXT("Fragments"));

	static FACFToolReport Run(const FACFItemSpec& Spec, bool bSave)
	{
		ACFAIToolsUtils::FBlueprintRequest Request;
		Request.AssetPath = Spec.AssetPath;
		Request.ParentClass = Spec.ParentClass;
		Request.RequiredBase = UACFItem::StaticClass();
		Request.bOverwriteExisting = Spec.bOverwriteExisting;
		Request.Properties = &Spec.Properties;
		Request.InstancedArrayProperty = FragmentsProperty;
		Request.InstancedBase = UACFItemFragment::StaticClass();
		Request.InstancedSpecs = &Spec.Fragments;
		Request.bClearInstanced = Spec.bClearFragments;

		return ACFAIToolsUtils::RunBlueprintRequest(Request, bSave, [](UObject* Defaults, FACFToolReport& Report)
		{
			const UACFItem* Item = Cast<UACFItem>(Defaults);
			if (!Item)
			{
				return;
			}
			const UClass* DefaultsClass = Item->GetClass();
			const UClass* EffectiveParent = DefaultsClass->ClassGeneratedBy ? DefaultsClass->GetSuperClass() : DefaultsClass;
			if (EffectiveParent == UACFItem::StaticClass())
			{
				Report.Warnings.Add(TEXT("the item derives directly from ACFItem; a specific type (weapon, armor, consumable...) is usually wanted"));
			}
			if (Item->GetItemInfo().Name.IsEmpty())
			{
				Report.Warnings.Add(TEXT("ItemInfo.Name is empty"));
			}
			if (!Item->GetItemInfo().ThumbNail)
			{
				Report.Warnings.Add(TEXT("ItemInfo.ThumbNail is not set; the item has no inventory icon"));
			}
		});
	}
}

TArray<FACFToolClassInfo> UACFItemToolset::ListItemClasses()
{
	return ACFAIToolsUtils::CollectClasses(UACFItem::StaticClass());
}

TArray<FACFToolClassInfo> UACFItemToolset::ListItemFragmentClasses()
{
	return ACFAIToolsUtils::CollectClasses(UACFItemFragment::StaticClass());
}

TArray<FACFToolPropertyInfo> UACFItemToolset::DescribeItemClass(const FString& Class)
{
	FString Error;
	UClass* Resolved = ACFAIToolsUtils::ResolveClass(UACFItem::StaticClass(), Class, Error);
	if (!Resolved)
	{
		FString FragmentError;
		Resolved = ACFAIToolsUtils::ResolveClass(UACFItemFragment::StaticClass(), Class, FragmentError);
	}
	if (!Resolved)
	{
		UKismetSystemLibrary::RaiseScriptError(Error);
		return {};
	}
	return ACFAIToolsUtils::DescribeProperties(Resolved, { ACFItemToolsetImpl::FragmentsProperty });
}

FACFItemSpec UACFItemToolset::ExportItem(const FString& ItemPath)
{
	FACFItemSpec Spec;
	FString Error;
	if (!ACFAIToolsUtils::ExportBlueprint(ItemPath, UACFItem::StaticClass(), ACFItemToolsetImpl::FragmentsProperty,
			Spec.AssetPath, Spec.ParentClass, Spec.Properties, Spec.Fragments, Error))
	{
		UKismetSystemLibrary::RaiseScriptError(Error);
		return Spec;
	}
	Spec.bOverwriteExisting = true;
	return Spec;
}

FACFToolReport UACFItemToolset::ValidateItem(const FACFItemSpec& Spec)
{
	return ACFAIToolsUtils::Finish(ACFItemToolsetImpl::Run(Spec, false), false);
}

FACFToolReport UACFItemToolset::SaveItem(const FACFItemSpec& Spec)
{
	return ACFAIToolsUtils::SaveChecked([&Spec](bool bSave) { return ACFItemToolsetImpl::Run(Spec, bSave); });
}
