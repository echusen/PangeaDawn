# Breeding System Integration

Quick integration guide for connecting the Breeding System to your game.

## Basic Setup

### 1. Add Components to Dinosaurs

```cpp
// In APDDinosaurBase or your creature class
UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
TObjectPtr<UPangeaBreedableComponent> BreedableComponent;

APDDinosaurBase::APDDinosaurBase()
{
    BreedableComponent = CreateDefaultSubobject<UPangeaBreedableComponent>(TEXT("Breedable"));
}
```

### 2. Create Species Data Assets

Create `DA_RaptorSpecies`:
- Set SpeciesID: "Raptor"
- Set IncubationSeconds: 120.0
- Set FertilityCooldownSeconds: 300.0
- Set CreatureClass to your dinosaur class
- Configure MaterialGeneticGroups

### 3. Assign to Creatures

```cpp
void InitializeDinosaur(APDDinosaurBase* Dino, bool bIsMale)
{
    Dino->BreedableComponent->SpeciesData = RaptorSpeciesData;
    Dino->BreedableComponent->Gender = bIsMale ? ECreatureGender::Male : ECreatureGender::Female;
    Dino->BreedableComponent->bIsFertile = true;
}
```

## Save System Integration

The breeding components use `SaveGame` tags:

```cpp
// Auto-saved properties
UPROPERTY(SaveGame)
ECreatureGender Gender;

UPROPERTY(SaveGame)
bool bIsFertile;

UPROPERTY(SaveGame)
FGeneticTraitSet GeneticTraits;
```

If using ALS or custom save system, these will be saved automatically.

## Event Handling

### Listen to Breeding Events

```cpp
void AMyGameMode::BeginPlay()
{
    Super::BeginPlay();

    // Find all breedable creatures
    TArray<AActor*> Dinosaurs;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), APDDinosaurBase::StaticClass(), Dinosaurs);

    for (AActor* Actor : Dinosaurs)
    {
        APDDinosaurBase* Dino = Cast<APDDinosaurBase>(Actor);
        if (Dino && Dino->BreedableComponent)
        {
            Dino->BreedableComponent->OnBred.AddDynamic(this, &AMyGameMode::HandleDinosaurBred);
        }
    }
}

void AMyGameMode::HandleDinosaurBred(APangeaEggActor* Egg, AActor* OtherParent)
{
    // Achievement tracking
    IncrementBreedingCount();

    // Notification
    ShowNotification(TEXT("Dinosaurs have bred! Egg incubating..."));

    // Listen to egg hatch
    Egg->OnEggHatched.AddDynamic(this, &AMyGameMode::HandleEggHatched);
}

void AMyGameMode::HandleEggHatched(AActor* NewCreature)
{
    ShowNotification(TEXT("An egg has hatched!"));

    // Auto-tame the offspring
    APDDinosaurBase* Baby = Cast<APDDinosaurBase>(NewCreature);
    if (Baby && Baby->TamingComponent)
    {
        Baby->TamingComponent->OnTameResolved(true, ETamedRole::Companion);
    }
}
```

## UI Examples

### Creature Status Display

```cpp
void UCreatureStatusWidget::UpdateBreedingInfo(UPangeaBreedableComponent* Breedable)
{
    if (!Breedable)
        return;

    // Gender
    GenderText->SetText(Breedable->Gender == ECreatureGender::Male ? 
        INVTEXT("Male") : INVTEXT("Female"));

    // Fertility Status
    if (Breedable->bIsOnFertilityCooldown)
    {
        FertilityText->SetText(FText::Format(
            INVTEXT("Cooldown: {0}s"),
            FText::AsNumber(FMath::CeilToInt(Breedable->FertilityCooldownRemaining))
        ));
        FertilityText->SetColorAndOpacity(FLinearColor::Red);
    }
    else if (Breedable->bIsFertile)
    {
        FertilityText->SetText(INVTEXT("Fertile"));
        FertilityText->SetColorAndOpacity(FLinearColor::Green);
    }
    else
    {
        FertilityText->SetText(INVTEXT("Not Fertile"));
        FertilityText->SetColorAndOpacity(FLinearColor::Gray);
    }

    // Display Traits
    TraitsList->ClearChildren();
    for (const FGeneticTrait& Trait : Breedable->GeneticTraits.Traits)
    {
        UTextBlock* TraitEntry = NewObject<UTextBlock>(this);
        TraitEntry->SetText(FText::Format(
            INVTEXT("{0}: {1}"),
            FText::FromName(Trait.Name),
            FText::AsNumber(FMath::RoundToInt(Trait.Value))
        ));
        TraitsList->AddChild(TraitEntry);
    }
}
```

## Console Commands for Testing

```cpp
UFUNCTION(Exec)
void DebugBreedSelected()
{
    // Breed first two selected dinosaurs
    TArray<AActor*> Selected = GetSelectedActors();
    if (Selected.Num() < 2)
        return;

    APDDinosaurBase* DinoA = Cast<APDDinosaurBase>(Selected[0]);
    APDDinosaurBase* DinoB = Cast<APDDinosaurBase>(Selected[1]);

    if (!DinoA || !DinoB)
        return;

    // Find a farm or create temp one
    APangeaBreedingFarmActor* Farm = FindOrCreateFarm();

    APangeaEggActor* Egg = Farm->BreedingFarmComponent->TryBreed(
        DinoA->BreedableComponent,
        DinoB->BreedableComponent
    );

    UE_LOG(LogTemp, Log, TEXT("Debug breed result: %s"), Egg ? TEXT("Success") : TEXT("Failed"));
}

UFUNCTION(Exec)
void DebugSkipIncubation()
{
    // Find all eggs and hatch them immediately
    TArray<AActor*> Eggs;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), APangeaEggActor::StaticClass(), Eggs);

    for (AActor* Actor : Eggs)
    {
        APangeaEggActor* Egg = Cast<APangeaEggActor>(Actor);
        if (Egg)
        {
            Egg->Hatch();
        }
    }
}
```

## Best Practices

- Always validate species compatibility before breeding
- Handle fertility cooldowns in UI to prevent player confusion  
- Auto-tame offspring or provide clear ownership mechanics
- Consider preventing inbreeding by tracking lineage
- Balance fertility cooldowns based on gameplay pacing
- Provide clear visual feedback during incubation
- Test with different genetic strategies for variety

## See Also

- [Breeding Components](breeding-components.md)
- [Genetics System](breeding-genetics.md)
- [Breeding Farm](breeding-farm.md)

