#pragma once

#include "CoreMinimal.h"
#include "Types/BreedingTypes.h"
#include "Engine/DataAsset.h"
#include "PangeaCreatureDefinition.generated.h"

class AActor;
class UPangeaDefinitionFragment;

UCLASS(BlueprintType)
class PANGEACORE_API UPangeaCreatureDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Definition")
	FName SpeciesId = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Definition")
	TSoftClassPtr<AActor> CreatureClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Definition")
	ECreatureGenderAssignmentMode GenderAssignmentMode = ECreatureGenderAssignmentMode::Randomize;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Definition", meta=(EditCondition="GenderAssignmentMode == ECreatureGenderAssignmentMode::UseDefault", EditConditionHides))
	ECreatureGender DefaultGender = ECreatureGender::Female;

	UPROPERTY(EditDefaultsOnly, Instanced, BlueprintReadOnly, Category="Definition")
	TArray<TObjectPtr<UPangeaDefinitionFragment>> Fragments;

	UFUNCTION(BlueprintPure, Category="Definition")
	UPangeaDefinitionFragment* GetFragmentByClass(TSubclassOf<UPangeaDefinitionFragment> FragmentClass) const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

	template <typename T>
	T* GetFragment() const
	{
		return Cast<T>(GetFragmentByClass(T::StaticClass()));
	}
};
