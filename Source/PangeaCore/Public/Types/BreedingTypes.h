#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "BreedingTypes.generated.h"

class UPackageMap;

UENUM(BlueprintType)
enum class ECreatureGender : uint8
{
	Unspecified UMETA(DisplayName="Unspecified"),
	Male UMETA(DisplayName="Male"),
	Female UMETA(DisplayName="Female")
};

UENUM(BlueprintType)
enum class ECreatureGenderAssignmentMode : uint8
{
	UseDefault UMETA(DisplayName="Use Default"),
	Randomize UMETA(DisplayName="Randomize")
};

USTRUCT(BlueprintType)
struct PANGEACORE_API FGeneticTrait
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	FName Name = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	float Value = 0.f;
};

USTRUCT(BlueprintType)
struct PANGEACORE_API FGeneticTraitSet
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	TArray<FGeneticTrait> Traits;

	float GetValue(FName InName, float Default = 0.f) const
	{
		if (const FGeneticTrait* Found = Traits.FindByPredicate([&](const FGeneticTrait& T) { return T.Name == InName; }))
		{
			return Found->Value;
		}
		return Default;
	}

	void SetValue(FName InName, float InValue)
	{
		if (FGeneticTrait* Found = Traits.FindByPredicate([&](const FGeneticTrait& T) { return T.Name == InName; }))
		{
			Found->Value = InValue;
		}
		else
		{
			Traits.Add({InName, InValue});
		}
	}
};

USTRUCT(BlueprintType)
struct PANGEACORE_API FParentSnapshot
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	FName SpeciesID = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	FGuid CreatureId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	FGeneticTraitSet Traits;

	/** Optional runtime convenience only; lineage is the durable GUID, not an actor path. */
	UPROPERTY(Transient, NotReplicated)
	TSoftObjectPtr<AActor> ParentActor;

	/** Skip default member replication only; NetSerialize below explicitly transmits every map. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, NotReplicated)
	TMap<FName, FLinearColor> VisualData;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, NotReplicated)
	TMap<FName, FLinearColor> MaterialParams;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, NotReplicated)
	TMap<FName, float> ScalarParams;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, NotReplicated)
	TMap<FGameplayTag, float> InheritedStatistics;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, NotReplicated)
	TMap<FGameplayTag, float> InheritedPrimaryAttributes;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, NotReplicated)
	TMap<FGameplayTag, float> InheritedAttributes;

	/** Explicit bounded wire format: Unreal's default replicated struct path cannot replicate raw maps. */
	bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess);
};

template<> struct TStructOpsTypeTraits<FParentSnapshot> : TStructOpsTypeTraitsBase2<FParentSnapshot>
{
	enum { WithNetSerializer = true };
};

USTRUCT(BlueprintType)
struct PANGEACORE_API FPangeaInheritedColor
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Appearance")
	FName Parameter;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Appearance")
	FLinearColor Value = FLinearColor::White;
};

USTRUCT(BlueprintType)
struct PANGEACORE_API FPangeaInheritedScalar
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Appearance")
	FName Parameter;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Appearance")
	float Value = 0.f;
};

/** Array-based generated appearance is both SaveGame-serializable and normally replicable. */
USTRUCT(BlueprintType)
struct PANGEACORE_API FPangeaInheritedAppearance
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Appearance")
	int32 MaterialSlot = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Appearance")
	TArray<FPangeaInheritedColor> Colors;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Appearance")
	TArray<FPangeaInheritedScalar> Scalars;
	bool IsEmpty() const { return Colors.IsEmpty() && Scalars.IsEmpty(); }
};

USTRUCT(BlueprintType)
struct PANGEACORE_API FIncubationConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float IncubationSeconds = 120.f;
};

USTRUCT(BlueprintType)
struct PANGEACORE_API FFertilityConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Breeding")
	float FertilityCooldownSeconds = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Breeding")
	bool bAffectsBothParents = true;
};
