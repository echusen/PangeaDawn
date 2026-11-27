#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "BaseUpgradeDefinition.generated.h"

class UBaseUpgradeFragment;

/**
 * Root data asset that describes how one base (village) upgrades.
 * The data lives in inline fragments so each base only authors the parts it uses.
 */
UCLASS(BlueprintType)
class PANGEABASEUPGRADESYSTEM_API UBaseUpgradeDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Tag that identifies the village this definition belongs to. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrade")
	FGameplayTag VillageTag;

	/** Inline data blocks of the definition. One fragment per class, and exactly one progression fragment. */
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category = "Upgrade")
	TArray<TObjectPtr<UBaseUpgradeFragment>> Fragments;

	/** Fills OutErrors with every authoring problem found. An empty array means the definition is usable. Read-only. */
	UFUNCTION(BlueprintCallable, Category = "Validation")
	void GetConfigurationErrors(TArray<FText>& OutErrors) const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

	/** Returns the first fragment that is of FragmentClass or a child class, or null. */
	UFUNCTION(BlueprintPure, Category = "Upgrade")
	UBaseUpgradeFragment* GetFragmentByClass(TSubclassOf<UBaseUpgradeFragment> FragmentClass) const;

	/** Typed C++ version of GetFragmentByClass. */
	template <typename T>
	T* GetFragment() const
	{
		return Cast<T>(GetFragmentByClass(T::StaticClass()));
	}
};
