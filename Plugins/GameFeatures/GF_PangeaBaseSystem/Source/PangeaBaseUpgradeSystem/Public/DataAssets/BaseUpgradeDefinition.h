#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/DataAsset.h"
#include "BaseUpgradeDefinition.generated.h"

class UBaseUpgradeFragment;

UCLASS(BlueprintType)
class PANGEABASEUPGRADESYSTEM_API UBaseUpgradeDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade")
	FGameplayTag VillageTag;

	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category="Upgrade")
	TArray<TObjectPtr<UBaseUpgradeFragment>> Fragments;

	UFUNCTION(BlueprintCallable, Category="Validation")
	void GetConfigurationErrors(TArray<FText>& OutErrors) const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

	UFUNCTION(BlueprintPure, Category="Upgrade")
	UBaseUpgradeFragment* GetFragmentByClass(TSubclassOf<UBaseUpgradeFragment> FragmentClass) const;

	template <typename T>
	T* GetFragment() const
	{
		return Cast<T>(GetFragmentByClass(T::StaticClass()));
	}
};
