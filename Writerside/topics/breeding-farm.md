# Breeding Farm Setup

This page covers how to set up breeding farms and manage the breeding process.

## Farm Actor Setup

### Creating a Breeding Farm

```cpp
UCLASS()
class APangeaBreedingFarmActor : public AActor
{
    GENERATED_BODY()

public:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    UPangeaBreedingFarmComponent* BreedingFarmComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    UBoxComponent* BreedingZone;

    APangeaBreedingFarmActor()
    {
        BreedingFarmComponent = CreateDefaultSubobject<UPangeaBreedingFarmComponent>(TEXT("BreedingFarm"));
        BreedingZone = CreateDefaultSubobject<UBoxComponent>(TEXT("BreedingZone"));
        
        RootComponent = BreedingZone;
        BreedingZone->SetBoxExtent(FVector(500, 500, 200));
        
        BreedingFarmComponent->BreedingZone = BreedingZone;
    }
};
```

### Editor Setup

1. Create Blueprint based on `PangeaBreedingFarmActor` or similar
2. Add `PangeaBreedingFarmComponent`
3. Add `BoxComponent` for breeding zone
4. Link box component to farm component's `BreedingZone` property
5. Set `EggClass` to your egg actor class
6. Optionally assign custom `GeneticStrategy`

## Egg Incubation

### APangeaEggActor

The egg actor handles incubation timing and hatching:

**Key Properties**:
```cpp
UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
FParentSnapshot ParentA, ParentB;

UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
FGeneticTraitSet ChildTraits;

UPROPERTY(BlueprintAssignable)
FOnEggProgress OnEggProgress;

UPROPERTY(BlueprintAssignable)
FOnEggHatched OnEggHatched;
```

**Incubation Process**:
1. Egg initialized with parent snapshots and combined traits
2. Timer starts based on `SpeciesData->Incubation.IncubationSeconds`
3. Progress events fire periodically for UI updates
4. On completion, `Hatch()` is called
5. Creature is spawned with inherited traits and visuals
6. `OnEggHatched` event fires

### Manual Breeding Workflow

```cpp
// Player interacts with farm to breed selected dinosaurs
void AFarmActor::OnPlayerInteract(APlayerController* Player)
{
    // Show UI to select male and female
    ShowBreedingSelectionUI(Player);
}

void AFarmActor::OnBreedingPairSelected(AActor* MaleActor, AActor* FemaleActor)
{
    UPangeaBreedableComponent* Male = MaleActor->FindComponentByClass<UPangeaBreedableComponent>();
    UPangeaBreedableComponent* Female = FemaleActor->FindComponentByClass<UPangeaBreedableComponent>();

    if (!Male || !Female)
        return;

    // Attempt breeding
    APangeaEggActor* Egg = BreedingFarmComponent->TryBreed(Male, Female);

    if (Egg)
    {
        ShowBreedingSuccessNotification();
        // Egg will auto-hatch after incubation time
    }
    else
    {
        ShowBreedingFailureMessage("Creatures cannot breed at this time");
    }
}
```

### Automatic Breeding

```cpp
// Auto-breed first available compatible pair
void AFarmActor::AutoBreed()
{
    TArray<UPangeaBreedableComponent*> Males, Females;
    BreedingFarmComponent->GetBreedablesByGender(Males, Females);

    for (UPangeaBreedableComponent* Male : Males)
    {
        if (!Male->IsFertile())
            continue;

        for (UPangeaBreedableComponent* Female : Females)
        {
            if (!Female->IsFertile())
                continue;

            // Check species compatibility
            if (Male->GetSpeciesData() != Female->GetSpeciesData())
                continue;

            // Attempt breeding
            APangeaEggActor* Egg = BreedingFarmComponent->TryBreed(Male, Female);
            if (Egg)
            {
                return; // Success, done
            }
        }
    }

    // No compatible pairs found
}
```

## UI Integration

### Breeding Selection Widget

```cpp
UCLASS()
class UBreedingSelectionWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintReadWrite, meta=(BindWidget))
    UListView* MaleCreaturesList;

    UPROPERTY(BlueprintReadWrite, meta=(BindWidget))
    UListView* FemaleCreaturesList;

    UPROPERTY(BlueprintReadWrite, meta=(BindWidget))
    UButton* BreedButton;

    void Initialize(UPangeaBreedingFarmComponent* Farm)
    {
        TArray<UPangeaBreedableComponent*> Males, Females;
        Farm->GetBreedablesByGender(Males, Females);

        // Populate lists
        for (UPangeaBreedableComponent* Male : Males)
        {
            MaleCreaturesList->AddItem(CreateCreatureListEntry(Male));
        }

        for (UPangeaBreedableComponent* Female : Females)
        {
            FemaleCreaturesList->AddItem(CreateCreatureListEntry(Female));
        }

        UpdateBreedButton();
    }

    void OnSelectionChanged()
    {
        UpdateBreedButton();
    }

    void UpdateBreedButton()
    {
        UPangeaBreedableComponent* SelectedMale = GetSelectedMale();
        UPangeaBreedableComponent* SelectedFemale = GetSelectedFemale();

        bool bCanBreed = SelectedMale && SelectedFemale &&
                         SelectedMale->IsFertile() && SelectedFemale->IsFertile();

        BreedButton->SetIsEnabled(bCanBreed);
    }
};
```

### Egg Incubation Progress Widget

```cpp
UCLASS()
class UEggIncubationWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintReadWrite, meta=(BindWidget))
    UProgressBar* IncubationProgress;

    UPROPERTY(BlueprintReadWrite, meta=(BindWidget))
    UTextBlock* TimeRemainingText;

    UPROPERTY()
    APangeaEggActor* TrackedEgg;

    void TrackEgg(APangeaEggActor* Egg)
    {
        if (TrackedEgg)
        {
            TrackedEgg->OnEggProgress.RemoveDynamic(this, &UEggIncubationWidget::HandleProgress);
        }

        TrackedEgg = Egg;

        if (TrackedEgg)
        {
            TrackedEgg->OnEggProgress.AddDynamic(this, &UEggIncubationWidget::HandleProgress);
            TrackedEgg->OnEggHatched.AddDynamic(this, &UEggIncubationWidget::HandleHatched);
        }
    }

    UFUNCTION()
    void HandleProgress(float Progress)
    {
        IncubationProgress->SetPercent(Progress);

        float TotalTime = TrackedEgg->GetSpeciesData()->Incubation.IncubationSeconds;
        float Remaining = TotalTime * (1.0f - Progress);

        int32 Minutes = FMath::FloorToInt(Remaining / 60.0f);
        int32 Seconds = FMath::FloorToInt(Remaining) % 60;

        TimeRemainingText->SetText(FText::Format(
            INVTEXT("{0}:{1:02}"),
            FText::AsNumber(Minutes),
            FText::AsNumber(Seconds)
        ));
    }

    UFUNCTION()
    void HandleHatched(AActor* NewCreature)
    {
        // Show hatch animation, close widget, etc.
        PlayHatchAnimation();
        RemoveFromParent();
    }
};
```

## Fertility Management

### Displaying Fertility Status

```cpp
FText GetFertilityStatusText(UPangeaBreedableComponent* Breedable)
{
    if (!Breedable)
        return INVTEXT("Unknown");

    if (Breedable->bIsOnFertilityCooldown)
    {
        int32 Minutes = FMath::FloorToInt(Breedable->FertilityCooldownRemaining / 60.0f);
        int32 Seconds = FMath::FloorToInt(Breedable->FertilityCooldownRemaining) % 60;

        return FText::Format(
            INVTEXT("Cooldown: {0}:{1:02}"),
            FText::AsNumber(Minutes),
            FText::AsNumber(Seconds)
        );
    }

    if (Breedable->bIsFertile)
    {
        return INVTEXT("Fertile");
    }

    return INVTEXT("Infertile");
}
```

### Fertility Cooldown Widget

```cpp
UCLASS()
class UFertilityCooldownWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintReadWrite, meta=(BindWidget))
    UProgressBar* CooldownBar;

    UPROPERTY(BlueprintReadWrite, meta=(BindWidget))
    UTextBlock* CooldownText;

    UPROPERTY()
    UPangeaBreedableComponent* TrackedBreedable;

    void TrackBreedable(UPangeaBreedableComponent* Breedable)
    {
        if (TrackedBreedable)
        {
            TrackedBreedable->OnFertilityCooldownTick.RemoveDynamic(this, &UFertilityCooldownWidget::HandleTick);
            TrackedBreedable->OnFertilityStateChanged.RemoveDynamic(this, &UFertilityCooldownWidget::HandleStateChanged);
        }

        TrackedBreedable = Breedable;

        if (TrackedBreedable)
        {
            TrackedBreedable->OnFertilityCooldownTick.AddDynamic(this, &UFertilityCooldownWidget::HandleTick);
            TrackedBreedable->OnFertilityStateChanged.AddDynamic(this, &UFertilityCooldownWidget::HandleStateChanged);
        }

        UpdateDisplay();
    }

    UFUNCTION()
    void HandleTick(float RemainingTime)
    {
        UpdateDisplay();
    }

    UFUNCTION()
    void HandleStateChanged(bool bNowFertile)
    {
        UpdateDisplay();
    }

    void UpdateDisplay()
    {
        if (!TrackedBreedable || !TrackedBreedable->bIsOnFertilityCooldown)
        {
            SetVisibility(ESlateVisibility::Collapsed);
            return;
        }

        SetVisibility(ESlateVisibility::Visible);

        float TotalCooldown = TrackedBreedable->GetSpeciesData()->Fertility.FertilityCooldownSeconds;
        float Progress = 1.0f - (TrackedBreedable->FertilityCooldownRemaining / TotalCooldown);

        CooldownBar->SetPercent(Progress);
        CooldownText->SetText(FText::Format(
            INVTEXT("Cooldown: {0}s"),
            FText::AsNumber(FMath::CeilToInt(TrackedBreedable->FertilityCooldownRemaining))
        ));
    }
};
```

## See Also

- [Breeding Components](breeding-components.md) - Component API
- [Genetics System](breeding-genetics.md) - Trait inheritance
- [Integration Guide](breeding-integration.md) - Full game integration

