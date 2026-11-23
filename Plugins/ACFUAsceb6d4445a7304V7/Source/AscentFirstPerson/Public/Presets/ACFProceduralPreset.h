// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ACFProceduralPreset.generated.h"

UENUM(BlueprintType)
enum class EACFPPOutResultPins : uint8
{
	IsValid,
	IsNotValid
};

UCLASS(Abstract, DefaultToInstanced, EditInlineNew, BlueprintType, Blueprintable)
class ASCENTFIRSTPERSON_API UACFProceduralPreset : public UObject
{
	GENERATED_BODY()

public:
	UACFProceduralPreset();

	UFUNCTION(BlueprintPure, Category="Preset")
	FString GetPresetName() const;

#if WITH_EDITOR
	/**
	 * Backs meta=(GetOptions=...) on every curve-name property, turning them into dropdowns.
	 *
	 * Non-static on purpose: the details panel resolves an unqualified GetOptions name against the object
	 * being edited, which avoids depending on object-path syntax for a static lookup.
	 */
	UFUNCTION()
	TArray<FString> GetCurveNameOptions() const;
#endif

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Procedural Preset", AdvancedDisplay)
	bool bShowInConfigurator = true;

protected:
	FString PresetName;
};

UCLASS(BlueprintType, Blueprintable, meta=(DisplayName="Procedural Preset Data"))
class ASCENTFIRSTPERSON_API UACFProceduralPresetData : public UDataAsset
{
	GENERATED_BODY()

public:
	UACFProceduralPresetData();

	UFUNCTION(BlueprintPure, Category="Preset", meta=(DeterminesOutputType="PresetClass"))
	UACFProceduralPreset* GetPresetByClass(const TSubclassOf<UACFProceduralPreset> PresetClass) const;

	template<typename T>
	static T* GetPresetOrDefault(UACFProceduralPresetData* PresetData);

	UFUNCTION(BlueprintCallable, Category="Preset", meta=(DeterminesOutputType="PresetClass", ExpandEnumAsExecs="OutResult"))
	static UACFProceduralPreset* GetPreset(UACFProceduralPresetData* PresetData, TSubclassOf<UACFProceduralPreset> PresetClass, EACFPPOutResultPins& OutResult);

	template<typename T>
	T* GetPreset() const;

	template<typename T>
	T* GetPresetOrDefault() const;

private:
	/** Depth-first over Parents, visiting each asset once so a cycle cannot hang the lookup. */
	UACFProceduralPreset* FindPresetByClass(const TSubclassOf<UACFProceduralPreset>& PresetClass, TArray<const UACFProceduralPresetData*, TInlineAllocator<8>>& Visited) const;

public:

	/** Read-only view for tooling that has to enumerate presets rather than ask for one by class. */
	const TArray<TObjectPtr<UACFProceduralPreset>>& GetPresets() const { return Presets; }

#if WITH_EDITOR
	/** Adds an instance of PresetClass if this asset has none. Returns it, or the existing one. */
	UACFProceduralPreset* AddPresetOfClass(TSubclassOf<UACFProceduralPreset> PresetClass);
#endif

#if WITH_EDITORONLY_DATA
	/**
	 * Optional. Narrows the curve-name dropdowns on this asset's presets to curves this skeleton can see.
	 *
	 * Editor-only and read by nothing at runtime. Without it the dropdowns list every curve in the project,
	 * which still beats free text but can offer names belonging to an unrelated rig.
	 */
	UPROPERTY(EditAnywhere, Category="Preset", meta=(DisplayName="Curve Name Source"))
	TSoftObjectPtr<USkeleton> CurveNameSkeleton;
#endif

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Instanced, Category="Preset")
	TArray<TObjectPtr<UACFProceduralPreset>> Presets;

	/**
	 * Assets to fall back to for presets this one does not contain, searched in order.
	 *
	 * Lets a shared asset carry the studio-wide feel while a per-weapon asset overrides only what differs.
	 * Cycles are tolerated: each asset is visited at most once per lookup.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Preset")
	TArray<TObjectPtr<UACFProceduralPresetData>> Parents;
};

template<typename T>
T* UACFProceduralPresetData::GetPresetOrDefault(UACFProceduralPresetData* PresetData)
{
	if (IsValid(PresetData))
	{
		return PresetData->GetPresetOrDefault<T>();
	}

	return GetMutableDefault<T>();
}

template<typename T>
T* UACFProceduralPresetData::GetPreset() const
{
	static_assert(TIsDerivedFrom<T, UACFProceduralPreset>::Value, "T must inherit UACFProceduralPreset");
	return Cast<T>(GetPresetByClass(T::StaticClass()));
}

template<typename T>
T* UACFProceduralPresetData::GetPresetOrDefault() const
{
	if (T* P = GetPreset<T>()) return P;
	return GetMutableDefault<T>();
}
