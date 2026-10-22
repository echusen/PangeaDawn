#pragma once

#include "CoreMinimal.h"
#include "Definitions/PangeaDefinitionFragment.h"
#include "GameplayEffect.h"
#include "GameplayTagContainer.h"
#include "Types/BreedingTypes.h"
#include "PangeaBreedingFragment.generated.h"

class AActor;
class APangeaEggActor;
class UPangeaGeneticStrategy;
class UPangeaCreatureDefinition;
class UGameplayEffect;

UENUM(BlueprintType)
enum class EPangeaInheritedStatType : uint8
{
	Statistic UMETA(DisplayName="Statistic"),
	PrimaryAttribute UMETA(DisplayName="Primary Attribute"),
	Attribute UMETA(DisplayName="Attribute")
};

USTRUCT(BlueprintType)
struct FPangeaInheritedStatRule
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Breeding|Stats")
	FGameplayTag StatTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Breeding|Stats")
	EPangeaInheritedStatType StatType = EPangeaInheritedStatType::Statistic;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Breeding|Stats", meta=(ClampMin="0.0", ClampMax="1.0"))
	float BestParentBias = 0.65f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Breeding|Stats", meta=(ClampMin="0.0", ClampMax="1.0"))
	float MutationChance = 0.15f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Breeding|Stats")
	float MutationPercentMin = -0.05f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Breeding|Stats")
	float MutationPercentMax = 0.08f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Breeding|Stats")
	float MinValue = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Breeding|Stats")
	float MaxValue = 0.f;
};

USTRUCT(BlueprintType)
struct FPangeaInheritedStatValue
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Breeding|Stats")
	FGameplayTag StatTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Breeding|Stats")
	EPangeaInheritedStatType StatType = EPangeaInheritedStatType::Statistic;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Breeding|Stats")
	float Value = 0.f;
};

USTRUCT(BlueprintType)
struct FPangeaInheritedStatProfile
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Breeding|Stats")
	TArray<FPangeaInheritedStatValue> Values;

	bool IsEmpty() const
	{
		return Values.Num() == 0;
	}
};

USTRUCT(BlueprintType)
struct FMaterialColorGeneticGroup
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName GroupName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FName> ParameterNames;
};

USTRUCT(BlueprintType)
struct FMaterialScalarGeneticGroup
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName GroupName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FName> ParameterNames;
};

UCLASS(BlueprintType, EditInlineNew)
class PANGEABREEDINGSYSTEM_API UPangeaBreedingFragment : public UPangeaDefinitionFragment
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Breeding|Egg")
	TSubclassOf<APangeaEggActor> EggActorClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Breeding|Egg")
	TObjectPtr<UPangeaCreatureDefinition> OffspringCreatureDefinition;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Breeding|Genetics")
	TSubclassOf<UPangeaGeneticStrategy> GeneticStrategyClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Breeding|Stats")
	TSubclassOf<UGameplayEffect> InheritedStatsGameplayEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Breeding|Stats")
	TArray<FPangeaInheritedStatRule> InheritedStatRules;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Breeding")
	FIncubationConfig Incubation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Breeding")
	FFertilityConfig Fertility;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Breeding|Appearance")
	bool bInheritParentMaterials = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Breeding|Appearance")
	int32 InheritanceMaterialSlot = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Breeding|Appearance")
	float VisualMutationChance = 0.25f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Breeding|Appearance")
	float ParentBiasPower = 0.8f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Breeding|Appearance")
	float VisualMutationIntensity = 0.1f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Breeding|Appearance")
	TArray<FMaterialColorGeneticGroup> MaterialColorGroups;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Breeding|Appearance")
	TArray<FMaterialScalarGeneticGroup> MaterialScalarGroups;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Breeding|Appearance", meta=(DeprecatedProperty, DeprecationMessage="Use MaterialColorGroups instead."))
	TArray<FMaterialColorGeneticGroup> MaterialGeneticGroups;
};
