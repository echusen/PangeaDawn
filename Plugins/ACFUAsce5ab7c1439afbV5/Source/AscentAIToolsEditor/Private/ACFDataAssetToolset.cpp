// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "ACFDataAssetToolset.h"

#include "ACFAIToolsUtils.h"
#include "ACFAbilitySet.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Data/ACFCharacterDataAsset.h"
#include "Engine/Blueprint.h"
#include "Engine/DataAsset.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/PackageName.h"
#include "UObject/UObjectIterator.h"

namespace ACFDataAssetToolsetImpl
{
	static bool HasACFAncestor(const UClass* NativeClass)
	{
		for (const UClass* Class = NativeClass; Class && Class != UDataAsset::StaticClass(); Class = Class->GetSuperClass())
		{
			if (Class->GetName().StartsWith(TEXT("ACF")))
			{
				return true;
			}
		}
		return false;
	}

	/** Returns the toolset to use instead, or nullptr when the class is handled here. */
	static const TCHAR* DedicatedToolFor(const UClass* Class)
	{
		if (Class->IsChildOf(UACFCharacterDataAsset::StaticClass()))
		{
			return TEXT("ACFCharacterToolset (SaveCharacter)");
		}
		if (Class->IsChildOf(UACFAbilitySet::StaticClass()))
		{
			return TEXT("ACFAbilityToolset (SaveAbilitySet)");
		}
		return nullptr;
	}

	static bool IsListedDataAssetClass(const UClass* NativeClass)
	{
		return HasACFAncestor(NativeClass) && !DedicatedToolFor(NativeClass);
	}

	static UClass* LoadClassNoWarn(const FString& Path)
	{
		return LoadObject<UClass>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
	}

	/** Any class by native name (with or without U/A prefix), Blueprint name, or path. */
	static UClass* ResolveAnyClass(const FString& NameOrPath, FString& OutError)
	{
		const FString Wanted = NameOrPath.TrimStartAndEnd();
		if (Wanted.IsEmpty())
		{
			OutError = TEXT("no class given");
			return nullptr;
		}

		if (Wanted.StartsWith(TEXT("/")))
		{
			if (UClass* Class = LoadClassNoWarn(Wanted))
			{
				return Class;
			}
			const FString Generated = Wanted.Contains(TEXT("."))
				? Wanted + TEXT("_C")
				: FString::Printf(TEXT("%s.%s_C"), *Wanted, *FPackageName::GetShortName(Wanted));
			if (UClass* Class = LoadClassNoWarn(Generated))
			{
				return Class;
			}
			OutError = FString::Printf(TEXT("no class at '%s'"), *Wanted);
			return nullptr;
		}

		const bool bHasPrefix = Wanted.Len() > 1 && (Wanted[0] == TCHAR('U') || Wanted[0] == TCHAR('A'));
		const FString BlueprintName = Wanted.EndsWith(TEXT("_C")) ? Wanted.LeftChop(2) : Wanted;
		for (TObjectIterator<UClass> It; It; ++It)
		{
			UClass* Class = *It;
			const FString Name = Class->GetName();
			if (Class->HasAnyClassFlags(CLASS_NewerVersionExists) || Name.StartsWith(TEXT("SKEL_")) || Name.StartsWith(TEXT("REINST_")))
			{
				continue;
			}
			if (Name.Equals(Wanted, ESearchCase::IgnoreCase) || Name.Equals(BlueprintName + TEXT("_C"), ESearchCase::IgnoreCase)
				|| (bHasPrefix && !Class->ClassGeneratedBy && Name.Equals(Wanted.RightChop(1), ESearchCase::IgnoreCase)))
			{
				return Class;
			}
		}

		const FAssetRegistryModule& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		TArray<FAssetData> Blueprints;
		Registry.Get().GetAssetsByClass(UBlueprint::StaticClass()->GetClassPathName(), Blueprints, true);
		for (const FAssetData& Asset : Blueprints)
		{
			if (Asset.AssetName.ToString().Equals(BlueprintName, ESearchCase::IgnoreCase))
			{
				if (UClass* Class = LoadClassNoWarn(FString::Printf(TEXT("%s.%s_C"), *Asset.PackageName.ToString(), *Asset.AssetName.ToString())))
				{
					return Class;
				}
			}
		}

		OutError = FString::Printf(TEXT("unknown class '%s' (see ListDataAssetClasses or ListInlineObjectClasses)"), *Wanted);
		return nullptr;
	}

	static bool CheckDataAssetClass(const UClass* Class, FString& OutError)
	{
		if (!Class->IsChildOf(UDataAsset::StaticClass()))
		{
			OutError = FString::Printf(TEXT("%s is not a Data Asset class"), *Class->GetName());
			return false;
		}
		if (const TCHAR* Dedicated = DedicatedToolFor(Class))
		{
			OutError = FString::Printf(TEXT("%s assets are authored with %s"), *Class->GetName(), Dedicated);
			return false;
		}
		return true;
	}

	static FACFToolReport Run(const FACFDataAssetSpec& Spec, bool bSave)
	{
		FACFToolReport Report;
		FString Error;
		UClass* Class = nullptr;
		if (!Spec.DataAssetClass.IsEmpty())
		{
			Class = ResolveAnyClass(Spec.DataAssetClass, Error);
		}
		else
		{
			FString Package, ObjectPath;
			if (ACFAIToolsUtils::SplitAssetPath(Spec.AssetPath, Package, ObjectPath, Error))
			{
				const UObject* Existing = ACFAIToolsUtils::FindExistingAsset(Package, ObjectPath);
				Class = Existing ? Existing->GetClass() : nullptr;
				if (!Existing)
				{
					Error = FString::Printf(TEXT("DataAssetClass is required to create '%s'"), *ObjectPath);
				}
			}
		}
		if (!Class || !CheckDataAssetClass(Class, Error))
		{
			Report.Errors.Add(Error);
			return Report;
		}

		return ACFAIToolsUtils::RunDataAssetRequest(Spec.AssetPath, Class, Spec.bOverwriteExisting, bSave, [&Spec](UObject* Target, FACFToolReport& Report)
		{
			ACFAIToolsUtils::ClearProperties(Target, Spec.ClearProperties, {}, Report.AppliedProperties, Report.Errors);
			ACFAIToolsUtils::ApplyProperties(Target, Spec.Properties, {}, FString(), Report.AppliedProperties, Report.Errors);
			ACFAIToolsUtils::ApplyInlineObjects(Target, Spec.InlineObjects, {}, Report.AppliedProperties, Report.Errors);
			if (Report.bCreatesAsset && Spec.Properties.IsEmpty() && Spec.InlineObjects.IsEmpty())
			{
				Report.Warnings.Add(TEXT("no values given; the new asset keeps the class defaults"));
			}
		});
	}
}

TArray<FACFToolClassInfo> UACFDataAssetToolset::ListDataAssetClasses(const FString& Filter)
{
	TArray<FACFToolClassInfo> Classes = ACFAIToolsUtils::CollectClasses(UDataAsset::StaticClass(), &ACFDataAssetToolsetImpl::IsListedDataAssetClass);
	const FString Wanted = Filter.TrimStartAndEnd();
	if (!Wanted.IsEmpty())
	{
		Classes.RemoveAll([&Wanted](const FACFToolClassInfo& Info) { return !Info.Name.Contains(Wanted, ESearchCase::IgnoreCase); });
	}
	return Classes;
}

TArray<FACFToolClassInfo> UACFDataAssetToolset::ListInlineObjectClasses(const FString& BaseClass)
{
	FString Error;
	UClass* Base = ACFDataAssetToolsetImpl::ResolveAnyClass(BaseClass, Error);
	if (!Base)
	{
		UKismetSystemLibrary::RaiseScriptError(Error);
		return {};
	}
	return ACFAIToolsUtils::CollectClasses(Base);
}

TArray<FACFToolPropertyInfo> UACFDataAssetToolset::DescribeClass(const FString& Class)
{
	FString Error;
	const UClass* Resolved = ACFDataAssetToolsetImpl::ResolveAnyClass(Class, Error);
	if (!Resolved)
	{
		UKismetSystemLibrary::RaiseScriptError(Error);
		return {};
	}
	return ACFAIToolsUtils::DescribeProperties(Resolved, {}, true);
}

FACFDataAssetSpec UACFDataAssetToolset::ExportDataAsset(const FString& AssetPath)
{
	FACFDataAssetSpec Spec;
	FString Package, Error;
	if (!ACFAIToolsUtils::SplitAssetPath(AssetPath, Package, Spec.AssetPath, Error))
	{
		UKismetSystemLibrary::RaiseScriptError(Error);
		return Spec;
	}
	const UDataAsset* Asset = Cast<UDataAsset>(ACFAIToolsUtils::FindExistingAsset(Package, Spec.AssetPath));
	if (!Asset)
	{
		UKismetSystemLibrary::RaiseScriptError(FString::Printf(TEXT("'%s' is not a Data Asset"), *AssetPath));
		return Spec;
	}
	if (!ACFDataAssetToolsetImpl::CheckDataAssetClass(Asset->GetClass(), Error))
	{
		UKismetSystemLibrary::RaiseScriptError(Error);
		return Spec;
	}

	Spec.DataAssetClass = Asset->GetClass()->GetPathName();
	Spec.bOverwriteExisting = true;
	Spec.Properties = ACFAIToolsUtils::ExportChangedProperties(Asset, Asset->GetClass()->GetDefaultObject(), {}, &Spec.InlineObjects, &Spec.ClearProperties);
	return Spec;
}

FACFToolReport UACFDataAssetToolset::ValidateDataAsset(const FACFDataAssetSpec& Spec)
{
	return ACFAIToolsUtils::Finish(ACFDataAssetToolsetImpl::Run(Spec, false), false);
}

FACFToolReport UACFDataAssetToolset::SaveDataAsset(const FACFDataAssetSpec& Spec)
{
	return ACFAIToolsUtils::SaveChecked([&Spec](bool bSave) { return ACFDataAssetToolsetImpl::Run(Spec, bSave); });
}
