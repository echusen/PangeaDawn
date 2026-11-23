// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "ACFAbilityToolset.h"

#include "ACFAIToolsUtils.h"
#include "ACFAbilitySet.h"
#include "ACFGameplayAbility.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayTagsManager.h"
#include "Kismet/KismetSystemLibrary.h"

namespace ACFAbilityToolsetImpl
{
	static FGameplayTag ParseTag(const FString& Name, const FString& Prefix, bool bRequired, FACFToolReport& Report)
	{
		const FString Trimmed = Name.TrimStartAndEnd();
		if (Trimmed.IsEmpty())
		{
			if (bRequired)
			{
				Report.Errors.Add(Prefix + TEXT("TriggeringTag is required"));
			}
			return FGameplayTag();
		}
		const FGameplayTag Tag = UGameplayTagsManager::Get().RequestGameplayTag(FName(*Trimmed), false);
		if (!Tag.IsValid())
		{
			Report.Errors.Add(Prefix + FString::Printf(TEXT("GameplayTag '%s' is not registered"), *Trimmed));
		}
		return Tag;
	}

	static void CheckLevel(int32 Level, const FString& Prefix, FACFToolReport& Report)
	{
		if (Level < 1)
		{
			Report.Errors.Add(Prefix + TEXT("AbilityLevel must be at least 1"));
		}
	}

	static void ApplyAbilities(UACFAbilitySet* Set, const FACFAbilitySetSpec& Spec, FACFToolReport& Report)
	{
		TSet<FGameplayTag> Seen;
		TMap<UClass*, int32> UntaggedOccurrences;
		for (int32 Index = 0; Index < Spec.Abilities.Num(); ++Index)
		{
			const FACFAbilityGrantSpec& Entry = Spec.Abilities[Index];
			const FString Prefix = FString::Printf(TEXT("Abilities[%d]: "), Index);
			const FGameplayTag Tag = ParseTag(Entry.TriggeringTag, Prefix, false, Report);
			if (Tag.IsValid() && Seen.Contains(Tag))
			{
				Report.Errors.Add(Prefix + FString::Printf(TEXT("tag %s is listed twice"), *Tag.ToString()));
				continue;
			}
			Seen.Add(Tag);
			CheckLevel(Entry.AbilityLevel, Prefix, Report);

			FString Error;
			UClass* AbilityClass = ACFAIToolsUtils::ResolveClass(UGameplayAbility::StaticClass(), Entry.AbilityClass, Error);
			if (!AbilityClass)
			{
				Report.Errors.Add(Prefix + Error);
				continue;
			}
			if (!Tag.IsValid())
			{
				Report.Warnings.Add(Prefix + TEXT("no TriggeringTag; the ability is granted but cannot be triggered by tag"));
			}

			FAbilityConfig Config;
			Config.TriggeringTag = Tag;
			Config.GameplayAbility = AbilityClass;
			Config.AbilityLevel = Entry.AbilityLevel;
			FAbilityConfig* Current = nullptr;
			if (Tag.IsValid())
			{
				Current = Set->Abilities.FindByPredicate([&Tag](const FAbilityConfig& Existing) { return Existing.TriggeringTag == Tag; });
			}
			else
			{
				const int32 Occurrence = UntaggedOccurrences.FindOrAdd(AbilityClass)++;
				int32 Matches = 0;
				Current = Set->Abilities.FindByPredicate([AbilityClass, Occurrence, &Matches](const FAbilityConfig& Existing)
				{
					return !Existing.TriggeringTag.IsValid() && Existing.GameplayAbility == AbilityClass && Matches++ == Occurrence;
				});
			}
			if (Current)
			{
				*Current = Config;
			}
			else
			{
				Set->Abilities.Add(Config);
			}
			Report.AppliedProperties.Add(FString::Printf(TEXT("Abilities[%s]"), *Tag.ToString()));
		}
	}

	static void ApplyActionAbilities(UACFAbilitySet* Set, const FACFAbilitySetSpec& Spec, FACFToolReport& Report)
	{
		TSet<FGameplayTag> Seen;
		for (int32 Index = 0; Index < Spec.ActionAbilities.Num(); ++Index)
		{
			const FACFActionAbilitySpec& Entry = Spec.ActionAbilities[Index];
			const FString Prefix = FString::Printf(TEXT("ActionAbilities[%d]: "), Index);
			const FGameplayTag Tag = ParseTag(Entry.TriggeringTag, Prefix, true, Report);
			if (!Tag.IsValid())
			{
				continue;
			}
			if (Seen.Contains(Tag))
			{
				Report.Errors.Add(Prefix + FString::Printf(TEXT("tag %s is listed twice"), *Tag.ToString()));
				continue;
			}
			Seen.Add(Tag);
			CheckLevel(Entry.AbilityLevel, Prefix, Report);

			FActionAbilityConfig* Current = Set->ActionAbilities.FindByPredicate([&Tag](const FActionAbilityConfig& Existing) { return Existing.TriggeringTag == Tag; });
			UACFGameplayAbility* Action = nullptr;
			if (Entry.ActionClass.IsEmpty())
			{
				if (!Current || !Current->Action)
				{
					Report.Errors.Add(Prefix + FString::Printf(TEXT("ActionClass is required: no action is registered under %s yet"), *Tag.ToString()));
					continue;
				}
				Action = Current->Action;
				Action->Modify();
				ACFAIToolsUtils::ClearProperties(Action, Entry.ClearedProperties, {}, Report.AppliedProperties, Report.Errors);
				ACFAIToolsUtils::ApplyInlineObjects(Action, Entry.InlineObjects, {}, Report.AppliedProperties, Report.Errors);
				ACFAIToolsUtils::ApplyProperties(Action, Entry.Properties, {}, Prefix, Report.AppliedProperties, Report.Errors);
			}
			else
			{
				FACFToolObjectSpec ObjectSpec;
				ObjectSpec.Class = Entry.ActionClass;
				ObjectSpec.Properties = Entry.Properties;
				ObjectSpec.InlineObjects = Entry.InlineObjects;
				ObjectSpec.ClearedProperties = Entry.ClearedProperties;
				TArray<UObject*> Objects;
				if (!ACFAIToolsUtils::BuildInstancedObjects(Set, UACFGameplayAbility::StaticClass(), { ObjectSpec }, Prefix.LeftChop(2),
						Objects, Report.AppliedProperties, Report.Errors))
				{
					continue;
				}
				Action = CastChecked<UACFGameplayAbility>(Objects[0]);
			}

			FActionAbilityConfig Config;
			Config.TriggeringTag = Tag;
			Config.Action = Action;
			Config.AbilityLevel = Entry.AbilityLevel;
			if (Current)
			{
				*Current = Config;
			}
			else
			{
				Set->ActionAbilities.Add(Config);
			}
			Report.AppliedProperties.Add(FString::Printf(TEXT("ActionAbilities[%s]"), *Tag.ToString()));
		}
	}

	static void RemoveTags(UACFAbilitySet* Set, const FACFAbilitySetSpec& Spec, FACFToolReport& Report)
	{
		for (const FString& Name : Spec.RemoveTags)
		{
			const FGameplayTag Tag = ParseTag(Name, TEXT("RemoveTags: "), true, Report);
			if (!Tag.IsValid())
			{
				continue;
			}
			const int32 Removed = Set->Abilities.RemoveAll([&Tag](const FAbilityConfig& Entry) { return Entry.TriggeringTag == Tag; })
				+ Set->ActionAbilities.RemoveAll([&Tag](const FActionAbilityConfig& Entry) { return Entry.TriggeringTag == Tag; });
			if (Removed == 0)
			{
				Report.Warnings.Add(FString::Printf(TEXT("RemoveTags: nothing is registered under %s"), *Tag.ToString()));
			}
			else
			{
				Report.AppliedProperties.Add(FString::Printf(TEXT("Removed[%s]"), *Tag.ToString()));
			}
		}
	}

	static FACFToolReport RunSet(const FACFAbilitySetSpec& Spec, bool bSave)
	{
		return ACFAIToolsUtils::RunDataAssetRequest(Spec.AssetPath, UACFAbilitySet::StaticClass(), Spec.bOverwriteExisting, bSave,
			[&Spec](UObject* Target, FACFToolReport& Report)
		{
			UACFAbilitySet* Set = CastChecked<UACFAbilitySet>(Target);
			RemoveTags(Set, Spec, Report);
			ApplyAbilities(Set, Spec, Report);
			ApplyActionAbilities(Set, Spec, Report);
			if (Set->Abilities.IsEmpty() && Set->ActionAbilities.IsEmpty())
			{
				Report.Warnings.Add(TEXT("the ability set grants nothing"));
			}
		});
	}

	static FACFToolReport RunAbility(const FACFAbilitySpec& Spec, bool bSave)
	{
		ACFAIToolsUtils::FBlueprintRequest Request;
		Request.AssetPath = Spec.AssetPath;
		Request.ParentClass = Spec.ParentClass;
		Request.RequiredBase = UGameplayAbility::StaticClass();
		Request.bOverwriteExisting = Spec.bOverwriteExisting;
		Request.Properties = &Spec.Properties;

		return ACFAIToolsUtils::RunBlueprintRequest(Request, bSave, [](UObject* Defaults, FACFToolReport& Report)
		{
			if (!Defaults->IsA<UACFGameplayAbility>())
			{
				Report.Warnings.Add(TEXT("the ability does not derive from ACFGameplayAbility, so it has no ACF ActionConfig or montage"));
			}
		});
	}
}

TArray<FACFToolClassInfo> UACFAbilityToolset::ListAbilityClasses()
{
	return ACFAIToolsUtils::CollectClasses(UGameplayAbility::StaticClass());
}

TArray<FACFToolClassInfo> UACFAbilityToolset::ListActionClasses()
{
	return ACFAIToolsUtils::CollectClasses(UACFGameplayAbility::StaticClass());
}

TArray<FACFToolPropertyInfo> UACFAbilityToolset::DescribeAbilityClass(const FString& Class)
{
	FString Error;
	UClass* Resolved = ACFAIToolsUtils::ResolveClass(UGameplayAbility::StaticClass(), Class, Error);
	if (!Resolved)
	{
		UKismetSystemLibrary::RaiseScriptError(Error);
		return {};
	}
	return ACFAIToolsUtils::DescribeProperties(Resolved, {});
}

FACFAbilitySpec UACFAbilityToolset::ExportAbility(const FString& AbilityPath)
{
	FACFAbilitySpec Spec;
	FString Error;
	TArray<FACFToolObjectSpec> Unused;
	if (!ACFAIToolsUtils::ExportBlueprint(AbilityPath, UGameplayAbility::StaticClass(), NAME_None,
			Spec.AssetPath, Spec.ParentClass, Spec.Properties, Unused, Error))
	{
		UKismetSystemLibrary::RaiseScriptError(Error);
		return Spec;
	}
	Spec.bOverwriteExisting = true;
	return Spec;
}

FACFToolReport UACFAbilityToolset::ValidateAbility(const FACFAbilitySpec& Spec)
{
	return ACFAIToolsUtils::Finish(ACFAbilityToolsetImpl::RunAbility(Spec, false), false);
}

FACFToolReport UACFAbilityToolset::SaveAbility(const FACFAbilitySpec& Spec)
{
	return ACFAIToolsUtils::SaveChecked([&Spec](bool bSave) { return ACFAbilityToolsetImpl::RunAbility(Spec, bSave); });
}

FACFAbilitySetSpec UACFAbilityToolset::ExportAbilitySet(const FString& AbilitySetPath)
{
	FACFAbilitySetSpec Spec;
	FString Package, Error;
	if (!ACFAIToolsUtils::SplitAssetPath(AbilitySetPath, Package, Spec.AssetPath, Error))
	{
		UKismetSystemLibrary::RaiseScriptError(Error);
		return Spec;
	}
	const UACFAbilitySet* Set = Cast<UACFAbilitySet>(ACFAIToolsUtils::FindExistingAsset(Package, Spec.AssetPath));
	if (!Set)
	{
		UKismetSystemLibrary::RaiseScriptError(FString::Printf(TEXT("'%s' is not an Ability Set"), *AbilitySetPath));
		return Spec;
	}

	Spec.bOverwriteExisting = true;
	for (const FAbilityConfig& Config : Set->Abilities)
	{
		FACFAbilityGrantSpec Entry;
		Entry.TriggeringTag = Config.TriggeringTag.IsValid() ? Config.TriggeringTag.ToString() : FString();
		Entry.AbilityClass = Config.GameplayAbility ? Config.GameplayAbility->GetPathName() : FString();
		Entry.AbilityLevel = Config.AbilityLevel;
		Spec.Abilities.Add(Entry);
	}
	for (const FActionAbilityConfig& Config : Set->ActionAbilities)
	{
		FACFActionAbilitySpec Entry;
		Entry.TriggeringTag = Config.TriggeringTag.IsValid() ? Config.TriggeringTag.ToString() : FString();
		Entry.AbilityLevel = Config.AbilityLevel;
		if (Config.Action)
		{
			const FACFToolObjectSpec Object = ACFAIToolsUtils::ExportInstancedObject(Config.Action);
			Entry.ActionClass = Object.Class;
			Entry.Properties = Object.Properties;
			Entry.InlineObjects = Object.InlineObjects;
			Entry.ClearedProperties = Object.ClearedProperties;
		}
		Spec.ActionAbilities.Add(Entry);
	}
	return Spec;
}

FACFToolReport UACFAbilityToolset::ValidateAbilitySet(const FACFAbilitySetSpec& Spec)
{
	return ACFAIToolsUtils::Finish(ACFAbilityToolsetImpl::RunSet(Spec, false), false);
}

FACFToolReport UACFAbilityToolset::SaveAbilitySet(const FACFAbilitySetSpec& Spec)
{
	return ACFAIToolsUtils::SaveChecked([&Spec](bool bSave) { return ACFAbilityToolsetImpl::RunSet(Spec, bSave); });
}
