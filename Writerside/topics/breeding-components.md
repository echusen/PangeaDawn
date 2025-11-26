# Breeding Components

This page documents the core components used in the Breeding System.

## UPangeaBreedableComponent

Marks a creature as breedable and stores its genetic information.

### Properties

#### Gender and Fertility

```cpp
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Breeding")
ECreatureGender Gender;
```
**Values**: `Male` or `Female`

**Purpose**: Determines breeding compatibility. Only opposite genders can breed.

```cpp
UPROPERTY(EditAnywhere, BlueprintReadWrite)
bool bIsFertile;
```
**Purpose**: Whether the creature can currently breed. Set to `false` during cooldown.

```cpp
UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Breeding")
bool bIsOnFertilityCooldown;
```
**Purpose**: Indicates if creature is in post-breeding cooldown period.

```cpp
UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Breeding")
float FertilityCooldownRemaining;
```
**Purpose**: Seconds remaining until fertility is restored.

#### Genetic Data

```cpp
UPROPERTY(EditAnywhere, BlueprintReadWrite)
FGeneticTraitSet GeneticTraits;
```
**Purpose**: The creature's genetic traits (stats that can be inherited).

**Structure**:
```cpp
struct FGeneticTraitSet
{
    TArray<FGeneticTrait> Traits;
    
    float GetValue(FName InName, float Default = 0.f) const;
    void SetValue(FName InName, float InValue);
};

struct FGeneticTrait
{
    FName Name;   // e.g., "Strength", "Speed"
    float Value;  // Numeric value
};
```

#### Configuration

```cpp
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Breeding|Setup")
TObjectPtr<UPangeaSpeciesDataAsset> SpeciesData;
```
**Purpose**: Species-specific breeding configuration (incubation time, fertility cooldown, etc.).

```cpp
UPROPERTY(EditAnywhere, BlueprintReadOnly)
TObjectPtr<UARSStatisticsComponent> ACFAttributes;
```
**Purpose**: Reference to the creature's attribute component (for pulling stats into traits).

### Events

#### OnBred
```cpp
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBred, APangeaEggActor*, Egg, AActor*, OtherParentActor);

UPROPERTY(BlueprintAssignable)
FOnBred OnBred;
```

**Purpose**: Fired when this creature successfully breeds.

**Parameters**:
- `Egg`: The spawned egg actor
- `OtherParentActor`: The other parent in the breeding pair

**Usage Example**:
```cpp
// Bind in Blueprint or C++
BreedableComponent->OnBred.AddDynamic(this, &AMyController::HandleDinosaurBred);

void AMyController::HandleDinosaurBred(APangeaEggActor* Egg, AActor* OtherParent)
{
    // Show notification, play animation, etc.
}
```

#### OnFertilityStateChanged
```cpp
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFertilityStateChanged, bool, bNowFertile);

UPROPERTY(BlueprintAssignable, Category="Breeding|Events")
FFertilityStateChanged OnFertilityStateChanged;
```

**Purpose**: Fired when fertility changes (becomes fertile or infertile).

#### OnFertilityCooldownTick
```cpp
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFertilityCooldownTick, float, RemainingTime);

UPROPERTY(BlueprintAssignable, Category="Breeding|Events")
FFertilityCooldownTick OnFertilityCooldownTick;
```

**Purpose**: Fired periodically during cooldown (useful for UI countdown timers).

### Core Functions

#### BreedWith
```cpp
UFUNCTION(BlueprintCallable)
APangeaEggActor* BreedWith(UPangeaBreedableComponent* OtherParent, UPangeaBreedingFarmComponent* Farm);
```

**Purpose**: Breed this creature with another.

**Parameters**:
- `OtherParent`: The other breedable component
- `Farm`: The farm managing the breeding (handles spawning and genetics)

**Returns**: The spawned egg actor, or `nullptr` if breeding failed.

**Validation**:
- Both must be fertile
- Must be opposite genders
- Must have compatible species data

**Usage Example**:
```cpp
APangeaEggActor* Egg = MaleBreedable->BreedWith(FemaleBreedable, MyFarm->BreedingFarmComponent);
if (Egg)
{
    UE_LOG(LogTemp, Log, TEXT("Breeding successful! Egg: %s"), *Egg->GetName());
}
else
{
    UE_LOG(LogTemp, Warning, TEXT("Breeding failed - check fertility and compatibility"));
}
```

#### BuildParentSnapshot
```cpp
UFUNCTION(BlueprintCallable)
FParentSnapshot BuildParentSnapshot() const;
```

**Purpose**: Creates a snapshot of this creature's genetic information.

**Returns**: `FParentSnapshot` containing:
- Species ID
- Creature unique ID
- Genetic traits
- Material color parameters

**When Called**: Automatically called during breeding to capture parent data.

#### CollectMaterialGenetics
```cpp
UFUNCTION(BlueprintCallable)
TMap<FName, FLinearColor> CollectMaterialGenetics() const;
```

**Purpose**: Extracts material parameter values for genetic inheritance.

**Returns**: Map of parameter names to color values.

**Usage**: Called internally by `BuildParentSnapshot()`, but exposed for debugging.

#### StartFertilityCooldown
```cpp
void StartFertilityCooldown(float CooldownDuration);
```

**Purpose**: Begin fertility cooldown after breeding.

**Behavior**:
1. Sets `bIsFertile = false`
2. Sets `bIsOnFertilityCooldown = true`
3. Starts timer for `CooldownDuration` seconds
4. Fires `OnFertilityStateChanged` event
5. Starts tick timer for UI updates

**Called Automatically**: By the breeding farm after successful breeding.

#### EndFertilityCooldown
```cpp
void EndFertilityCooldown();
```

**Purpose**: Restore fertility after cooldown completes.

**Behavior**:
1. Sets `bIsFertile = true`
2. Sets `bIsOnFertilityCooldown = false`
3. Clears timers
4. Fires `OnFertilityStateChanged` event

**Called Automatically**: When cooldown timer expires.

### Interface Implementation

#### IPangeaBreedingInterface

The component implements this interface for polymorphic access:

```cpp
virtual bool IsFertile_Implementation() const override;
virtual bool SetFertile_Implementation(bool bNewFertile) override;
virtual FParentSnapshot GetParentSnapshot_Implementation() const override;
virtual UPangeaSpeciesDataAsset* GetSpeciesData_Implementation() const override;
```

---

## UPangeaBreedingFarmComponent

Manages a breeding zone and handles breeding attempts.

### Properties

#### Breeding Zone

```cpp
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Breeding|References")
TObjectPtr<UBoxComponent> BreedingZone;
```

**Purpose**: Defines the area where creatures can breed.

**Setup**: Create a `UBoxComponent` on your farm actor and assign it here.

#### Contained Breedables

```cpp
UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Breeding|Farm")
TArray<UPangeaBreedableComponent*> ContainedBreedables;
```

**Purpose**: List of all breedable components currently in the breeding zone.

**Updated**: Automatically via overlap events.

#### Genetic Strategy

```cpp
UPROPERTY(EditAnywhere, Instanced, BlueprintReadWrite, Category="Breeding|Farm")
TObjectPtr<UPangeaGeneticStrategy> GeneticStrategy;
```

**Purpose**: The strategy used to combine parent traits.

**Default**: If not set, uses default averaging strategy.

**Customization**: Create custom `UPangeaGeneticStrategy` subclass and assign instance.

#### Egg Configuration

```cpp
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Breeding|Farm")
TSubclassOf<APangeaEggActor> EggClass;
```

**Purpose**: The egg actor class to spawn when breeding succeeds.

### Events

#### OnEggSpawned
```cpp
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEggSpawned, APangeaEggActor*, Egg);

UPROPERTY(BlueprintAssignable, Category="Breeding|Farm")
FOnEggSpawned OnEggSpawned;
```

**Purpose**: Fired when an egg is successfully spawned.

**Usage Example**:
```cpp
FarmComponent->OnEggSpawned.AddDynamic(this, &AFarmActor::HandleEggSpawned);

void AFarmActor::HandleEggSpawned(APangeaEggActor* Egg)
{
    // Play VFX, play sound, update UI
    SpawnBreedingParticles(Egg->GetActorLocation());
}
```

### Core Functions

#### TryBreed
```cpp
UFUNCTION(BlueprintCallable, Category="Breeding|Farm")
APangeaEggActor* TryBreed(UPangeaBreedableComponent* Male, UPangeaBreedableComponent* Female);
```

**Purpose**: Attempt to breed two creatures.

**Parameters**:
- `Male`: Male breedable component
- `Female`: Female breedable component

**Returns**: Spawned egg on success, `nullptr` on failure.

**Validation**:
- Both must be fertile
- Must be opposite genders
- Must have same species
- Species data must be valid

**Process**:
1. Validate creatures
2. Build parent snapshots
3. Use genetic strategy to combine traits
4. Spawn egg with combined genetics
5. Apply fertility cooldowns to parents
6. Fire events

**Usage Example**:
```cpp
UPangeaBreedableComponent* Male = GetMaleFromPen();
UPangeaBreedableComponent* Female = GetFemaleFromPen();

APangeaEggActor* Egg = BreedingFarm->TryBreed(Male, Female);

if (Egg)
{
    ShowBreedingSuccessUI(Egg);
}
else
{
    ShowBreedingFailedUI("Cannot breed at this time");
}
```

#### GetBreedablesByGender
```cpp
UFUNCTION(BlueprintCallable, Category="Breeding|Farm")
void GetBreedablesByGender(TArray<UPangeaBreedableComponent*>& OutMales, TArray<UPangeaBreedableComponent*>& OutFemales) const;
```

**Purpose**: Separate contained breedables by gender.

**Usage Example**:
```cpp
TArray<UPangeaBreedableComponent*> Males, Females;
Farm->GetBreedablesByGender(Males, Females);

// Auto-breed first available pair
if (Males.Num() > 0 && Females.Num() > 0)
{
    Farm->TryBreed(Males[0], Females[0]);
}
```

#### RefreshContainedBreedables
```cpp
UFUNCTION(BlueprintCallable, Category="Breeding|Farm")
void RefreshContainedBreedables();
```

**Purpose**: Manually re-scan the breeding zone for breedables.

**When to Use**:
- After teleporting creatures
- After programmatically adding/removing creatures
- For debugging

**Normal Usage**: Overlap events handle this automatically.

#### GetContainedBreedables
```cpp
UFUNCTION(BlueprintCallable, Category="Breeding|Farm")
TArray<UPangeaBreedableComponent*> GetContainedBreedables() const;
```

**Purpose**: Get all breedables currently in the farm.

**Usage Example**:
```cpp
TArray<UPangeaBreedableComponent*> Breedables = Farm->GetContainedBreedables();
for (UPangeaBreedableComponent* Breedable : Breedables)
{
    // Display in UI, check fertility, etc.
}
```

### Automatic Tracking

The farm automatically tracks creatures via overlap events:

```cpp
void UPangeaBreedingFarmComponent::OnOverlapBegin(...)
{
    // Creature entered zone - add to ContainedBreedables
}

void UPangeaBreedingFarmComponent::OnOverlapEnd(...)
{
    // Creature left zone - remove from ContainedBreedables
}
```

---

## Setup Example

### Complete Breeding Setup

```cpp
// 1. Create species data asset (in editor)
UPangeaSpeciesDataAsset* RaptorSpecies = /* ... */;
RaptorSpecies->SpeciesID = "Raptor";
RaptorSpecies->Incubation.IncubationSeconds = 120.0f;
RaptorSpecies->Fertility.FertilityCooldownSeconds = 300.0f;

// 2. Add breedable component to dinosaurs
APDDinosaurBase* MaleDinosaur = SpawnDinosaur(RaptorClass);
UPangeaBreedableComponent* MaleBreedable = MaleDinosaur->FindComponentByClass<UPangeaBreedableComponent>();
MaleBreedable->Gender = ECreatureGender::Male;
MaleBreedable->SpeciesData = RaptorSpecies;
MaleBreedable->GeneticTraits.SetValue("Strength", 75.0f);
MaleBreedable->GeneticTraits.SetValue("Speed", 85.0f);

APDDinosaurBase* FemaleDinosaur = SpawnDinosaur(RaptorClass);
UPangeaBreedableComponent* FemaleBreedable = FemaleDinosaur->FindComponentByClass<UPangeaBreedableComponent>();
FemaleBreedable->Gender = ECreatureGender::Female;
FemaleBreedable->SpeciesData = RaptorSpecies;
FemaleBreedable->GeneticTraits.SetValue("Strength", 80.0f);
FemaleBreedable->GeneticTraits.SetValue("Speed", 90.0f);

// 3. Set up breeding farm
APangeaBreedingFarmActor* Farm = SpawnActor<APangeaBreedingFarmActor>();
Farm->BreedingFarmComponent->BreedingZone = Farm->BoxComponent;
Farm->BreedingFarmComponent->EggClass = APangeaEggActor::StaticClass();

// 4. Move dinosaurs into zone
MaleDinosaur->SetActorLocation(Farm->GetActorLocation());
FemaleDinosaur->SetActorLocation(Farm->GetActorLocation() + FVector(100, 0, 0));

// 5. Attempt breeding
APangeaEggActor* Egg = Farm->BreedingFarmComponent->TryBreed(MaleBreedable, FemaleBreedable);

// 6. Egg will hatch after incubation time
// Offspring will have blended traits and visuals
```

---

## See Also

- [Genetics System](breeding-genetics.md) - How traits are inherited
- [Breeding Farm](breeding-farm.md) - Setting up breeding zones
- [Integration Guide](breeding-integration.md) - Connecting to your game

