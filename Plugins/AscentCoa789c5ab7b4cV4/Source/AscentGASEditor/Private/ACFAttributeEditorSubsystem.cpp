// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2025. All Rights Reserved.


#include "ACFAttributeEditorSubsystem.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Engine/Blueprint.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor.h"
#include "ACFBaseAttributeSet.h"


void UACFAttributeEditorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	RefreshCache();
}
void UACFAttributeEditorSubsystem::Deinitialize()
{
	AttributeCache.Empty();
	Super::Deinitialize();
}

void UACFAttributeEditorSubsystem::RefreshCache()
{
	AttributeCache.Empty();

	for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
	{
		UClass* Class = *ClassIt;

		if (!Class || Class->HasAnyClassFlags(CLASS_Abstract)) {
			continue;
		}


		if (Class->GetName().StartsWith("SKEL_") || Class->GetName().StartsWith("REINST_")) {
			continue;
		}

		// AttributeSets
		if (Class->IsChildOf(UACFBaseAttributeSet::StaticClass()))
		{
			for (TFieldIterator<FProperty> PropIt(Class, EFieldIteratorFlags::ExcludeSuper); PropIt; ++PropIt)
			{
				FProperty* Property = *PropIt;

				FStructProperty* StructProp = CastField<FStructProperty>(Property);
				if (StructProp && StructProp->Struct == FGameplayAttributeData::StaticStruct())
				{
					FString ClassName = Class->GetName();
					if (ClassName.EndsWith("_C"))
						ClassName = ClassName.LeftChop(2);

					FString Key = FString::Printf(TEXT("%s.%s"), *ClassName, *Property->GetName());

					// Store both the property AND the class for later lookup
					AttributeCache.Add(Key, Property);
					// Also store class reference separately
					ClassCache.Add(Key, Class);
				}
			}
		}


		// System attributes from AbilitySystemComponent
		if (Class->IsChildOf(UAbilitySystemComponent::StaticClass()) && !Class->ClassGeneratedBy)
		{
			for (TFieldIterator<FProperty> PropIt(Class, EFieldIteratorFlags::ExcludeSuper); PropIt; ++PropIt)
			{
				FProperty* Property = *PropIt;
				if (Property->HasMetaData(TEXT("SystemGameplayAttribute")))
				{
					FString Key = FString::Printf(TEXT("%s.%s"), *Class->GetName(), *Property->GetName());
					AttributeCache.Add(Key, Property);
				}
			}
		}
	}

	//  AttributeSet Blueprint
	FAssetRegistryModule& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

	FARFilter Filter;
	Filter.ClassPaths.Add(UACFBaseAttributeSet::StaticClass()->GetClassPathName());
	Filter.bRecursiveClasses = true;

	TArray<FAssetData> Blueprints;
	AssetRegistry.Get().GetAssets(Filter, Blueprints);

	for (const FAssetData& AssetData : Blueprints)
	{
		if (UBlueprint* Blueprint = Cast<UBlueprint>(AssetData.GetAsset()))
		{
			if (Blueprint->GeneratedClass && Blueprint->GeneratedClass->IsChildOf(UAttributeSet::StaticClass()))
			{

				UClass* Class = Blueprint->GeneratedClass;

				for (TFieldIterator<FProperty> PropIt(Class, EFieldIteratorFlags::ExcludeSuper); PropIt; ++PropIt)
				{
					FProperty* Property = *PropIt;

					FStructProperty* StructProp = CastField<FStructProperty>(Property);
					if (StructProp && StructProp->Struct == FGameplayAttributeData::StaticStruct())
					{
						FString ClassName = Class->GetName();
						if (ClassName.EndsWith("_C"))
							ClassName = ClassName.LeftChop(2);

						FString Key = FString::Printf(TEXT("%s.%s"), *ClassName, *Property->GetName());
						AttributeCache.Add(Key, Property);
					}
				}
			}
		}
	}


}

TArray<TSharedPtr<FString>> UACFAttributeEditorSubsystem::GetAttributesList()
{
	RefreshCache();

	TArray<TSharedPtr<FString>> Result;
	Result.Add(MakeShared<FString>("None"));

	for (const auto& Pair : AttributeCache)
	{
		Result.Add(MakeShared<FString>(Pair.Key));
	}

	Result.Sort([](const TSharedPtr<FString>& A, const TSharedPtr<FString>& B)
		{
			return *A < *B;
		});

	return Result;
}

FGameplayAttribute UACFAttributeEditorSubsystem::StringToAttribute(const FString& AttributeString)
{
	if (AttributeString == "None" || AttributeString.IsEmpty())
		return FGameplayAttribute();

	// Check cache first
	if (FProperty** PropPtr = AttributeCache.Find(AttributeString))
	{
		return FGameplayAttribute(*PropPtr);
	}

	// Manual search by class name only
	FString ClassName, PropName;
	if (AttributeString.Split(TEXT("."), &ClassName, &PropName))
	{
		// Search all loaded classes by name
		for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
		{
			UClass* Class = *ClassIt;
			if (!Class) {
				continue;
			}

			FString CurrentClassName = Class->GetName();
			if (CurrentClassName.EndsWith("_C")) {
				CurrentClassName = CurrentClassName.LeftChop(2);
			}


			// Match by name only, not full path
			if (CurrentClassName == ClassName)
			{
				if (FProperty* Property = Class->FindPropertyByName(*PropName))
				{
					// Verify it's a valid attribute property
					FStructProperty* StructProp = CastField<FStructProperty>(Property);
					if (StructProp && StructProp->Struct == FGameplayAttributeData::StaticStruct())
					{
						return FGameplayAttribute(Property);
					}
				}
			}
		}
	}

	return FGameplayAttribute();
}

FString UACFAttributeEditorSubsystem::AttributeToString(const FGameplayAttribute& Attribute)
{
	if (!Attribute.IsValid())
		return "None";

	const FProperty* Property = Attribute.GetUProperty();
	if (Property && Property->GetOwnerClass())
	{
		FString ClassName = Property->GetOwnerClass()->GetName();
		if (ClassName.EndsWith("_C"))
			ClassName = ClassName.LeftChop(2);

		return FString::Printf(TEXT("%s.%s"), *ClassName, *Property->GetName());
	}

	return "None";
}

TArray<FString> UACFAttributeEditorSubsystem::GetAllAttributeNames() const
{

	// Use existing cache instead of re-iterating

	TArray<FString> AttributeNames;
	AttributeCache.GetKeys(AttributeNames);

	return AttributeNames;
}


void UACFAttributeEditorSubsystem::TryReinitializeCache()
{
	if (!bIsCacheInitialized) {
		RefreshCache();
		bIsCacheInitialized = true;
	}
}

bool UACFAttributeEditorSubsystem::IsValidAttributeName(const FString& AttributeName) const
{
	const TArray<FString> ValidNames = GetAllAttributeNames();
	return ValidNames.Contains(AttributeName);
}