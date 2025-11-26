# Genetics System

This page explains how genetic trait inheritance works in the Breeding System.

## Overview

When two creatures breed, their genetic traits combine to produce offspring traits. The system supports:
- Numeric trait blending
- Random mutations
- Parent bias
- Visual characteristic inheritance
- Custom genetic strategies

## Core Structures

### FGeneticTrait

A single named trait value:

```cpp
USTRUCT(BlueprintType)
struct FGeneticTrait
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName Name;    // e.g., "Strength", "Speed", "Size"

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Value;   // Numeric value
};
```

**Examples**:
- `{"Strength", 75.0}`
- `{"Speed", 85.5}`
- `{"MaxHealth", 100.0}`

### FGeneticTraitSet

Container for multiple traits with helper functions:

```cpp
USTRUCT(BlueprintType)
struct FGeneticTraitSet
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FGeneticTrait> Traits;

    // Get a trait value by name
    float GetValue(FName InName, float Default = 0.f) const;

    // Set a trait value by name (adds if doesn't exist)
    void SetValue(FName InName, float InValue);
};
```

**Usage Example**:
```cpp
FGeneticTraitSet Traits;
Traits.SetValue("Strength", 80.0f);
Traits.SetValue("Speed", 90.0f);

float Strength = Traits.GetValue("Strength"); // Returns 80.0
float Intelligence = Traits.GetValue("Intelligence", 50.0f); // Returns 50.0 (default)
```

### FParentSnapshot

Complete genetic snapshot of a parent creature:

```cpp
USTRUCT(BlueprintType)
struct FParentSnapshot
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName SpeciesID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FGuid CreatureId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FGeneticTraitSet Traits;

    UPROPERTY()
    TSoftObjectPtr<AActor> ParentActor;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TMap<FName, FLinearColor> VisualData;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TMap<FName, FLinearColor> MaterialParams;
};
```

**Captured During Breeding**:
- Species identity
- Unique creature ID for lineage tracking
- All genetic traits (stats)
- Visual appearance data
- Material color parameters

---

## UPangeaGeneticStrategy

The genetic strategy defines how parent traits combine. It's a pluggable `UObject` that can be customized per farm or globally.

### Base Class

```cpp
UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced)
class UPangeaGeneticStrategy : public UObject
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintNativeEvent, Category="Pangea|BreedingSystem|Genetics")
    FGeneticTraitSet CombineTraits(const FParentSnapshot& ParentA, const FParentSnapshot& ParentB) const;
};
```

### Default Implementation

The default strategy performs simple averaging with optional mutation:

```cpp
FGeneticTraitSet UPangeaGeneticStrategy::CombineTraits_Implementation(
    const FParentSnapshot& ParentA,
    const FParentSnapshot& ParentB) const
{
    FGeneticTraitSet Result;

    // Collect all unique trait names from both parents
    TSet<FName> AllTraitNames;
    for (const FGeneticTrait& Trait : ParentA.Traits.Traits)
        AllTraitNames.Add(Trait.Name);
    for (const FGeneticTrait& Trait : ParentB.Traits.Traits)
        AllTraitNames.Add(Trait.Name);

    // Combine each trait
    for (FName TraitName : AllTraitNames)
    {
        float ValueA = ParentA.Traits.GetValue(TraitName, 0.0f);
        float ValueB = ParentB.Traits.GetValue(TraitName, 0.0f);

        // Simple average
        float Combined = (ValueA + ValueB) / 2.0f;

        // Optional: Add small random variance (mutation)
        float Mutation = FMath::RandRange(-2.0f, 2.0f);
        Combined += Mutation;

        // Clamp to reasonable range
        Combined = FMath::Clamp(Combined, 0.0f, 100.0f);

        Result.SetValue(TraitName, Combined);
    }

    return Result;
}
```

---

## Custom Genetic Strategies

### Example: Dominant/Recessive Genes

```cpp
UCLASS()
class UDominantGeneStrategy : public UPangeaGeneticStrategy
{
    GENERATED_BODY()

public:
    // For each trait, 75% chance to inherit from dominant parent
    UPROPERTY(EditAnywhere, Category="Genetics")
    float DominanceChance = 0.75f;

    virtual FGeneticTraitSet CombineTraits_Implementation(
        const FParentSnapshot& ParentA,
        const FParentSnapshot& ParentB) const override
    {
        FGeneticTraitSet Result;

        TSet<FName> AllTraitNames;
        for (const FGeneticTrait& Trait : ParentA.Traits.Traits)
            AllTraitNames.Add(Trait.Name);
        for (const FGeneticTrait& Trait : ParentB.Traits.Traits)
            AllTraitNames.Add(Trait.Name);

        for (FName TraitName : AllTraitNames)
        {
            float ValueA = ParentA.Traits.GetValue(TraitName, 0.0f);
            float ValueB = ParentB.Traits.GetValue(TraitName, 0.0f);

            // Determine dominant parent (higher value = dominant)
            bool bAIsDominant = ValueA > ValueB;
            float DominantValue = bAIsDominant ? ValueA : ValueB;
            float RecessiveValue = bAIsDominant ? ValueB : ValueA;

            // Roll for dominance
            float Result = FMath::FRand() < DominanceChance ? DominantValue : RecessiveValue;

            // Small mutation
            Result += FMath::RandRange(-1.0f, 1.0f);
            Result = FMath::Clamp(Result, 0.0f, 100.0f);

            Result.SetValue(TraitName, Result);
        }

        return Result;
    }
};
```

### Example: Weighted Average Strategy

```cpp
UCLASS()
class UWeightedAverageStrategy : public UPangeaGeneticStrategy
{
    GENERATED_BODY()

public:
    // Weight toward parent A (0.5 = equal, 0.7 = 70% A, 30% B)
    UPROPERTY(EditAnywhere, Category="Genetics", meta=(ClampMin=0, ClampMax=1))
    float ParentAWeight = 0.5f;

    // Mutation strength
    UPROPERTY(EditAnywhere, Category="Genetics")
    float MutationRange = 3.0f;

    virtual FGeneticTraitSet CombineTraits_Implementation(
        const FParentSnapshot& ParentA,
        const FParentSnapshot& ParentB) const override
    {
        FGeneticTraitSet Result;

        TSet<FName> AllTraitNames;
        for (const FGeneticTrait& Trait : ParentA.Traits.Traits)
            AllTraitNames.Add(Trait.Name);
        for (const FGeneticTrait& Trait : ParentB.Traits.Traits)
            AllTraitNames.Add(Trait.Name);

        float ParentBWeight = 1.0f - ParentAWeight;

        for (FName TraitName : AllTraitNames)
        {
            float ValueA = ParentA.Traits.GetValue(TraitName, 0.0f);
            float ValueB = ParentB.Traits.GetValue(TraitName, 0.0f);

            // Weighted average
            float Combined = (ValueA * ParentAWeight) + (ValueB * ParentBWeight);

            // Apply mutation
            if (MutationRange > 0.0f)
            {
                Combined += FMath::RandRange(-MutationRange, MutationRange);
            }

            Combined = FMath::Clamp(Combined, 0.0f, 100.0f);
            Result.SetValue(TraitName, Combined);
        }

        return Result;
    }
};
```

### Blueprint Genetic Strategy

You can also create strategies entirely in Blueprint:

1. Create Blueprint class inheriting from `PangeaGeneticStrategy`
2. Override the `Combine Traits` event
3. Implement your logic using Blueprint nodes
4. Assign to farm's `GeneticStrategy` property

**Blueprint Example Logic**:
```
For Each Trait Name:
  - Get Parent A value
  - Get Parent B value
  - Roll dice to determine inheritance pattern
  - Apply custom formula
  - Add to result trait set
Return result
```

---

## Visual Inheritance

Beyond numeric traits, offspring inherit visual characteristics through material parameter blending.

### Material Genetic Groups

Defined in `UPangeaSpeciesDataAsset`:

```cpp
USTRUCT(BlueprintType)
struct FMaterialGeneticGroup
{
    GENERATED_BODY()

    // Category name (e.g., "Pattern Color", "Skin Color")
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName GroupName;

    // The specific material parameter names to blend
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<FName> ParameterNames;
};
```

**Example Configuration**:
| Group Name | Parameter Names |
|------------|----------------|
| Skin Color | `Skin_BaseColor`, `Skin_Tint` |
| Pattern Color | `Pattern_Primary`, `Pattern_Secondary` |
| Eye Color | `Eye_IrisColor` |

### Visual Inheritance Process

When an egg hatches:

1. **Extract Parent Colors**: Read material parameters from parent snapshots
2. **Blend Colors**: Mix parent colors using configured blend factors
3. **Apply Mutations**: Randomly shift hue/saturation/value
4. **Set on Offspring**: Apply blended colors to offspring's materials

### Color Blending

```cpp
FLinearColor MixParentColors(const FLinearColor& ColorA, const FLinearColor& ColorB) const
{
    // Base blend
    float BlendFactor = FMath::FRand(); // Random mix between parents
    FLinearColor Blended = FMath::Lerp(ColorA, ColorB, BlendFactor);

    // Apply mutation
    if (SpeciesData->VisualMutationChance > FMath::FRand())
    {
        // Shift hue slightly
        FLinearColor HSV = Blended.LinearRGBToHSV();
        HSV.R += FMath::RandRange(-10.0f, 10.0f); // Hue shift
        HSV.G *= FMath::RandRange(0.9f, 1.1f);    // Saturation variance
        Blended = HSV.HSVToLinearRGB();
    }

    return Blended;
}
```

### Species Configuration

In `UPangeaSpeciesDataAsset`:

```cpp
// Whether offspring blend material colors between parents
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Breeding|Appearance")
bool bInheritParentMaterials = true;

// Which material slot to modify (usually 0 or 1)
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance")
int32 InheritanceMaterialSlot = 1;

// Chance for visual mutations (0-1)
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance")
float VisualMutationChance = 0.25f;

// How biased toward one parent (0.5 = equal, 0.8 = biased)
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance")
float ParentBiasPower = 0.8f;

// How intense mutations are (0-1)
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance")
float VisualMutationIntensity = 0.1f;

// Which material parameters to inherit
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Breeding|Appearance")
TArray<FMaterialGeneticGroup> MaterialGeneticGroups;
```

---

## Pulling Traits from Attributes

Connect genetic traits to your game's attribute system by overriding `PullAttributesIntoTraits`:

### C++ Example

```cpp
void UPangeaBreedableComponent::PullAttributesIntoTraits_Implementation(FGeneticTraitSet& InOutTraits) const
{
    if (!ACFAttributes)
        return;

    // Pull GAS attributes into traits
    InOutTraits.SetValue("Strength", ACFAttributes->GetAttributeValue("Attributes.Strength"));
    InOutTraits.SetValue("Dexterity", ACFAttributes->GetAttributeValue("Attributes.Dexterity"));
    InOutTraits.SetValue("Constitution", ACFAttributes->GetAttributeValue("Attributes.Constitution"));
    InOutTraits.SetValue("MaxHealth", ACFAttributes->GetAttributeValue("Attributes.MaxHealth"));
    InOutTraits.SetValue("MaxStamina", ACFAttributes->GetAttributeValue("Attributes.MaxStamina"));
}
```

### Blueprint Example

Override `Pull Attributes Into Traits` event:
```
Event PullAttributesIntoTraits (InOutTraits)
  - Get Attribute Value: Strength
  - Set Value in TraitSet: "Strength"
  - Get Attribute Value: Speed
  - Set Value in TraitSet: "Speed"
  - ... etc
```

### When It's Called

Called during `BuildParentSnapshot()`:
```cpp
FParentSnapshot UPangeaBreedableComponent::BuildParentSnapshot() const
{
    FParentSnapshot Snapshot;
    Snapshot.SpeciesID = SpeciesData->SpeciesID;
    Snapshot.Traits = GeneticTraits;

    // Pull fresh attribute values
    PullAttributesIntoTraits(Snapshot.Traits);

    // ... capture visual data ...

    return Snapshot;
}
```

---

## Applying Traits to Offspring

After hatching, apply genetic traits back to the creature:

```cpp
void ApplyTraitsToCreature(AActor* Creature, const FGeneticTraitSet& Traits)
{
    UPangeaBreedableComponent* Breedable = Creature->FindComponentByClass<UPangeaBreedableComponent>();
    if (!Breedable)
        return;

    // Store traits
    Breedable->GeneticTraits = Traits;

    // Apply to attributes
    if (Breedable->ACFAttributes)
    {
        Breedable->ACFAttributes->SetAttributeValue("Attributes.Strength", Traits.GetValue("Strength"));
        Breedable->ACFAttributes->SetAttributeValue("Attributes.Speed", Traits.GetValue("Speed"));
        // ... etc
    }
}
```

---

## Advanced Patterns

### Trait Categories

Group traits by category for different inheritance rules:

```cpp
UCLASS()
class UCategorizedGeneticStrategy : public UPangeaGeneticStrategy
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere)
    TArray<FName> PhysicalTraits = {"Strength", "Constitution", "Size"};

    UPROPERTY(EditAnywhere)
    TArray<FName> MentalTraits = {"Intelligence", "Willpower"};

    virtual FGeneticTraitSet CombineTraits_Implementation(...) const override
    {
        // Physical traits: Average + high mutation
        // Mental traits: Dominant gene inheritance
        // ...
    }
};
```

### Lineage Tracking

Use the `CreatureId` in parent snapshots to track ancestry:

```cpp
struct FCreatureLineage
{
    FGuid CreatureId;
    TArray<FGuid> ParentIds;
    TArray<FGuid> GrandparentIds;
    int32 Generation;
};

void TrackLineage(APangeaEggActor* Egg)
{
    FCreatureLineage Lineage;
    Lineage.ParentIds.Add(Egg->ParentA.CreatureId);
    Lineage.ParentIds.Add(Egg->ParentB.CreatureId);
    // Store in database or save system
}
```

### Breeding Quality Score

Calculate offspring "quality" based on combined traits:

```cpp
float CalculateBreedingQuality(const FGeneticTraitSet& Traits)
{
    float TotalScore = 0.0f;
    int32 NumTraits = 0;

    for (const FGeneticTrait& Trait : Traits.Traits)
    {
        TotalScore += Trait.Value;
        NumTraits++;
    }

    return NumTraits > 0 ? (TotalScore / NumTraits) : 0.0f;
}
```

---

## See Also

- [Breeding Components](breeding-components.md) - Component API reference
- [Breeding Farm](breeding-farm.md) - Setting up breeding zones
- [Integration Guide](breeding-integration.md) - Connecting to your game

