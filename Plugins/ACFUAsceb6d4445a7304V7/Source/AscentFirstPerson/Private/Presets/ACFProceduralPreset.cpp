// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "Presets/ACFProceduralPreset.h"

#include "Misc/AssetRegistryInterface.h"
#include "Utility/ACFFirstPersonFunctionLibrary.h"

UACFProceduralPreset::UACFProceduralPreset()
{
	PresetName = FString("Preset");
}

FString UACFProceduralPreset::GetPresetName() const
{
	// Get the specific class of this instance (Parent or Child)
	const UClass* Class = this->GetClass();

#if WITH_EDITOR
	// Check if the metadata exists to avoid crashes
	if (Class->HasMetaData(TEXT("DisplayName")))
	{
		return Class->GetMetaData(TEXT("DisplayName"));
	}
#endif

	// Fallback for runtime/shipping builds where metadata is stripped
	return Class->GetName();
}

#if WITH_EDITOR
UACFProceduralPreset* UACFProceduralPresetData::AddPresetOfClass(TSubclassOf<UACFProceduralPreset> PresetClass)
{
	if (!PresetClass)
	{
		return nullptr;
	}

	if (UACFProceduralPreset* Existing = GetPresetByClass(PresetClass))
	{
		return Existing;
	}

	Modify();

	// Instanced subobject of this asset, so it saves with it and is not shared with other preset data.
	UACFProceduralPreset* Preset = NewObject<UACFProceduralPreset>(this, PresetClass, NAME_None, RF_Transactional);
	Presets.Add(Preset);

	MarkPackageDirty();

	return Preset;
}

TArray<FString> UACFProceduralPreset::GetCurveNameOptions() const
{
	// Presets are instanced subobjects, so the asset holding this one is where the skeleton hint lives.
	const UACFProceduralPresetData* OwningData = GetTypedOuter<UACFProceduralPresetData>();
	const USkeleton* Skeleton = OwningData ? OwningData->CurveNameSkeleton.LoadSynchronous() : nullptr;

	return UACFFirstPersonFunctionLibrary::GetCurveNamesForSkeleton(Skeleton);
}
#endif

UACFProceduralPresetData::UACFProceduralPresetData()
{

}

UACFProceduralPreset* UACFProceduralPresetData::GetPresetByClass(const TSubclassOf<UACFProceduralPreset> PresetClass) const
{
	TArray<const UACFProceduralPresetData*, TInlineAllocator<8>> Visited;
	return FindPresetByClass(PresetClass, Visited);
}

UACFProceduralPreset* UACFProceduralPresetData::FindPresetByClass(const TSubclassOf<UACFProceduralPreset>& PresetClass, TArray<const UACFProceduralPresetData*, TInlineAllocator<8>>& Visited) const
{
	if (!PresetClass || Visited.Contains(this))
	{
		return nullptr;
	}

	Visited.Add(this);

	for (const TObjectPtr<UACFProceduralPreset>& Preset : Presets)
	{
		if (Preset && Preset->IsA(PresetClass))
		{
			return Preset;
		}
	}

	// Own presets win; parents fill the gaps, in order.
	for (const TObjectPtr<UACFProceduralPresetData>& Parent : Parents)
	{
		if (Parent)
		{
			if (UACFProceduralPreset* Inherited = Parent->FindPresetByClass(PresetClass, Visited))
			{
				return Inherited;
			}
		}
	}

	return nullptr;
}

UACFProceduralPreset* UACFProceduralPresetData::GetPreset(UACFProceduralPresetData* PresetData, TSubclassOf<UACFProceduralPreset> PresetClass, EACFPPOutResultPins& OutResult)
{
	if (IsValid(PresetData))
	{
		if (UACFProceduralPreset* Preset = PresetData->GetPresetByClass(PresetClass))
		{
			OutResult = EACFPPOutResultPins::IsValid;
			return Preset;
		}
	}

	if (UACFProceduralPreset* Preset = PresetClass->GetDefaultObject<UACFProceduralPreset>())
	{
		OutResult = EACFPPOutResultPins::IsValid;
		return Preset;
	}

	OutResult = EACFPPOutResultPins::IsNotValid;
	return nullptr;
}

template ASCENTFIRSTPERSON_API UACFProceduralPreset* UACFProceduralPresetData::GetPreset<UACFProceduralPreset>() const;