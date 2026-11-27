#pragma once

#include "CoreMinimal.h"
#include "Definitions/PangeaDefinitionFragment.h"
#include "GameplayEffect.h"
#include "GameplayTagContainer.h"
#include "Types/BreedingTypes.h"
#include "PangeaBreedingFragment.generated.h"

class AActor;
class APangeaEggActor;
class UGameplayEffect;
class UPangeaCreatureDefinition;
class UPangeaGeneticStrategy;

/** Which ARS stat family an inherited stat tag belongs to. Decides how the value is read from a parent and applied to the offspring. */
UENUM(BlueprintType)
enum class EPangeaInheritedStatType : uint8
{
	/** ARS statistic. The parent's maximum value of the statistic is inherited. */
	Statistic UMETA(DisplayName = "Statistic"),
	/** ARS primary attribute, applied to the offspring as a primary attribute modifier. */
	PrimaryAttribute UMETA(DisplayName = "Primary Attribute"),
	/** ARS (secondary) attribute, applied to the offspring as an attribute modifier. */
	Attribute UMETA(DisplayName = "Attribute")
};

/** Designer rule describing how one stat is passed from two parents to their offspring when an egg is initialized. */
USTRUCT(BlueprintType)
struct FPangeaInheritedStatRule
{
	GENERATED_BODY()

	/** Set-by-caller tag of the stat to inherit. Rules with an invalid tag are skipped. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breeding|Stats")
	FGameplayTag StatTag;

	/** Stat family StatTag belongs to. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breeding|Stats")
	EPangeaInheritedStatType StatType = EPangeaInheritedStatType::Statistic;

	/** Blend between the parents' average (0) and the better parent's value (1). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breeding|Stats", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BestParentBias = 0.65f;

	/** Probability that the inherited value is mutated by a random percentage. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breeding|Stats", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MutationChance = 0.15f;

	/** Lower bound of the random mutation, as a fraction of the inherited value. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breeding|Stats")
	float MutationPercentMin = -0.05f;

	/** Upper bound of the random mutation, as a fraction of the inherited value. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breeding|Stats")
	float MutationPercentMax = 0.08f;

	/** Lower limit of the final value. Used with MaxValue as a range when MaxValue is greater, otherwise as a floor when above zero. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breeding|Stats")
	float MinValue = 0.f;

	/** Upper limit of the final value. Only applied when greater than MinValue. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breeding|Stats")
	float MaxValue = 0.f;
};

/** One generated stat value of an offspring, stored so it is rolled once and survives save/load. */
USTRUCT(BlueprintType)
struct FPangeaInheritedStatValue
{
	GENERATED_BODY()

	/** Set-by-caller tag of the inherited stat. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Breeding|Stats")
	FGameplayTag StatTag;

	/** Stat family StatTag belongs to. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Breeding|Stats")
	EPangeaInheritedStatType StatType = EPangeaInheritedStatType::Statistic;

	/** Value generated for the offspring from its parents. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Breeding|Stats")
	float Value = 0.f;
};

/** Full set of inherited stat values generated for one offspring. Cached on the egg and kept by the hatched creature. */
USTRUCT(BlueprintType)
struct FPangeaInheritedStatProfile
{
	GENERATED_BODY()

	/** One entry per valid inheritance rule of the breeding fragment. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Breeding|Stats")
	TArray<FPangeaInheritedStatValue> Values;

	bool IsEmpty() const
	{
		return Values.Num() == 0;
	}
};

/** Named group of vector (color) material parameters that are inherited from the parents. */
USTRUCT(BlueprintType)
struct FMaterialColorGeneticGroup
{
	GENERATED_BODY()

	/** Name identifying this group of parameters. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName GroupName;

	/** Vector material parameters read from each parent and blended for the offspring. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FName> ParameterNames;
};

/** Named group of scalar material parameters that are inherited from the parents. */
USTRUCT(BlueprintType)
struct FMaterialScalarGeneticGroup
{
	GENERATED_BODY()

	/** Name identifying this group of parameters. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName GroupName;

	/** Scalar material parameters read from each parent and blended for the offspring. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FName> ParameterNames;
};

/** Creature definition fragment holding the per-species breeding configuration: egg, offspring, incubation, fertility, stat and appearance inheritance. */
UCLASS(BlueprintType, EditInlineNew)
class PANGEABREEDINGSYSTEM_API UPangeaBreedingFragment : public UPangeaDefinitionFragment
{
	GENERATED_BODY()

public:
	/** Egg actor spawned when this species breeds. The farm's default egg class is used when unset. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breeding|Egg")
	TSubclassOf<APangeaEggActor> EggActorClass;

	/** Definition given to the hatched creature. The parents' definition is used when unset. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breeding|Egg")
	TObjectPtr<UPangeaCreatureDefinition> OffspringCreatureDefinition;

	/** Strategy class that combines the parents' genetic traits. The farm's strategy is used when unset. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breeding|Genetics")
	TSubclassOf<UPangeaGeneticStrategy> GeneticStrategyClass;

	/** Gameplay effect that applies the inherited stat profile to the offspring. Must have an Infinite duration. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breeding|Stats")
	TSubclassOf<UGameplayEffect> InheritedStatsGameplayEffect;

	/** Stats passed from the parents to the offspring and how each one is blended and mutated. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breeding|Stats")
	TArray<FPangeaInheritedStatRule> InheritedStatRules;

	/** Incubation timing of eggs of this species. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breeding")
	FIncubationConfig Incubation;

	/** Fertility cooldown applied to the parents after a successful breeding. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breeding")
	FFertilityConfig Fertility;

	/** Whether offspring inherit blended material parameters from their parents. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breeding|Appearance")
	bool bInheritParentMaterials = true;

	/** Skeletal mesh material slot the inherited parameters are read from and written to. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breeding|Appearance")
	int32 InheritanceMaterialSlot = 1;

	/** Probability that a blended color or scalar receives a random mutation. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Breeding|Appearance")
	float VisualMutationChance = 0.25f;

	/** Exponent applied to the random blend factor between the two parents' values. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Breeding|Appearance")
	float ParentBiasPower = 0.8f;

	/** Upper bound of the strength of a visual mutation. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Breeding|Appearance")
	float VisualMutationIntensity = 0.1f;

	/** Color parameter groups inherited by the offspring. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breeding|Appearance")
	TArray<FMaterialColorGeneticGroup> MaterialColorGroups;

	/** Scalar parameter groups inherited by the offspring. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breeding|Appearance")
	TArray<FMaterialScalarGeneticGroup> MaterialScalarGroups;

	/** Legacy color groups, only read while MaterialColorGroups is empty. */
	UPROPERTY(EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "Breeding|Appearance",
		meta = (DeprecatedProperty, DeprecationMessage = "Use MaterialColorGroups instead."))
	TArray<FMaterialColorGeneticGroup> MaterialGeneticGroups;
};
