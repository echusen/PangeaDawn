# Dinosaur System Integration

Complete integration guide for dinosaur systems in your game.

## Complete Setup Workflow

### 1. Create Data Assets

#### Species Data (Breeding)
```
DA_RaptorSpecies:
- SpeciesID: "Raptor"
- CreatureClass: BP_Raptor
- IncubationSeconds: 120
- FertilityCooldownSeconds: 300
- MaterialGeneticGroups: [Skin Color, Pattern]
```

#### Tame Config (Taming)
```
DA_RaptorTameConfig:
- RequiredItem: Meat x5
- StatRequirements: Dexterity >= 15
- TameDuration: 5.0
- bCanBeMount: true
- bCanBeCompanion: true
- TamedMountAIController: BP_RaptorMountAI
- TamedCompanionAIController: BP_RaptorCompanionAI
```

### 2. Create Dinosaur Blueprint

1. Create Blueprint: `BP_Raptor` inheriting from `PDDinosaurBase`
2. Configure components in Constructor:

```
Construction Script:
- Set TamingComponent->TameSpeciesConfig = DA_RaptorTameConfig
- Set BreedableComponent->SpeciesData = DA_RaptorSpecies
- Set MountComponent socket locations
- Configure VaultComponent for obstacle detection
```

### 3. Initialize on Spawn

```cpp
APDDinosaurBase* SpawnDinosaur(TSubclassOf<APDDinosaurBase> DinoClass, FVector Location, bool bIsWild)
{
    APDDinosaurBase* Dino = GetWorld()->SpawnActor<APDDinosaurBase>(DinoClass, Location, FRotator::ZeroRotator);

    if (!Dino)
        return nullptr;

    // Initialize taming
    if (bIsWild)
    {
        Dino->TamingComponent->InitializeWild();
    }
    else
    {
        Dino->TamingComponent->InitializeHostile();
    }

    // Initialize breeding
    Dino->BreedableComponent->Gender = FMath::RandBool() ? ECreatureGender::Male : ECreatureGender::Female;
    Dino->BreedableComponent->bIsFertile = true;

    // Generate random traits
    FGeneticTraitSet Traits;
    Traits.SetValue("Strength", FMath::RandRange(60.0f, 90.0f));
    Traits.SetValue("Speed", FMath::RandRange(70.0f, 100.0f));
    Traits.SetValue("Health", FMath::RandRange(80.0f, 120.0f));
    Dino->BreedableComponent->GeneticTraits = Traits;

    return Dino;
}
```

## Player Interaction Flow

### Context-Sensitive Interaction

```cpp
void APlayerCharacter::OnInteractPressed()
{
    AActor* LookedAt = GetLookedAtActor();
    APDDinosaurBase* Dino = Cast<APDDinosaurBase>(LookedAt);
    
    if (!Dino)
        return;

    // Determine interaction based on state
    if (!Dino->TamingComponent)
    {
        ShowMessage("Cannot interact with this dinosaur");
        return;
    }

    ETameState State = Dino->TamingComponent->GetTameState();

    switch (State)
    {
        case ETameState::Wild:
            TryTameDinosaur(Dino);
            break;

        case ETameState::Hostile:
            ShowMessage("Dinosaur is hostile!");
            break;

        case ETameState::Tamed:
            InteractWithTamedDinosaur(Dino);
            break;
    }
}

void APlayerCharacter::TryTameDinosaur(APDDinosaurBase* Dino)
{
    // Check requirements
    if (!HasTamingItems(Dino))
    {
        ShowMessage("You need taming items");
        return;
    }

    if (!MeetsTamingStatRequirements(Dino))
    {
        ShowMessage("Your stats are too low");
        return;
    }

    // Start taming
    Dino->TamingComponent->StartTameAttempt(this);
}

void APlayerCharacter::InteractWithTamedDinosaur(APDDinosaurBase* Dino)
{
    ETamedRole Role = Dino->TamingComponent->GetTamedRole();

    if (Role == ETamedRole::Mount)
    {
        // Mount the dinosaur
        if (Dino->MountComponent)
        {
            Dino->MountComponent->StartMount(this);
        }
    }
    else if (Role == ETamedRole::Companion)
    {
        // Open command menu
        OpenCompanionCommandMenu(Dino);
    }
}
```

## UI Integration

### Dinosaur Status HUD

```cpp
UCLASS()
class UDinosaurStatusHUD : public UUserWidget
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintReadWrite, meta=(BindWidget))
    UTextBlock* DinosaurNameText;

    UPROPERTY(BlueprintReadWrite, meta=(BindWidget))
    UTextBlock* StateText;

    UPROPERTY(BlueprintReadWrite, meta=(BindWidget))
    UTextBlock* RoleText;

    UPROPERTY(BlueprintReadWrite, meta=(BindWidget))
    UProgressBar* HealthBar;

    UPROPERTY(BlueprintReadWrite, meta=(BindWidget))
    UTextBlock* GenderText;

    UPROPERTY(BlueprintReadWrite, meta=(BindWidget))
    UTextBlock* FertilityText;

    void UpdateForDinosaur(APDDinosaurBase* Dino)
    {
        if (!Dino)
            return;

        // Name
        DinosaurNameText->SetText(FText::FromString(Dino->GetName()));

        // Taming state
        if (Dino->TamingComponent)
        {
            FText StateStr = GetTameStateText(Dino->TamingComponent->GetTameState());
            StateText->SetText(StateStr);

            if (Dino->TamingComponent->GetTameState() == ETameState::Tamed)
            {
                FText RoleStr = GetTameRoleText(Dino->TamingComponent->GetTamedRole());
                RoleText->SetText(RoleStr);
                RoleText->SetVisibility(ESlateVisibility::Visible);
            }
            else
            {
                RoleText->SetVisibility(ESlateVisibility::Collapsed);
            }
        }

        // Health
        if (Dino->GetAbilitySystemComponent())
        {
            float Health = Dino->GetAbilitySystemComponent()->GetNumericAttribute(UACFAttributeSet::GetHealthAttribute());
            float MaxHealth = Dino->GetAbilitySystemComponent()->GetNumericAttribute(UACFAttributeSet::GetMaxHealthAttribute());
            HealthBar->SetPercent(Health / MaxHealth);
        }

        // Breeding info
        if (Dino->BreedableComponent)
        {
            GenderText->SetText(Dino->BreedableComponent->Gender == ECreatureGender::Male ?
                INVTEXT("Male") : INVTEXT("Female"));

            if (Dino->BreedableComponent->bIsOnFertilityCooldown)
            {
                FertilityText->SetText(FText::Format(
                    INVTEXT("Cooldown: {0}s"),
                    FText::AsNumber(FMath::CeilToInt(Dino->BreedableComponent->FertilityCooldownRemaining))
                ));
            }
            else
            {
                FertilityText->SetText(Dino->BreedableComponent->bIsFertile ?
                    INVTEXT("Fertile") : INVTEXT("Not Fertile"));
            }
        }
    }
};
```

## Breeding Farm Integration

```cpp
void AFarmManager::SetupBreedingFarm()
{
    // Create farm actor
    BreedingFarm = GetWorld()->SpawnActor<APangeaBreedingFarmActor>(BreedingFarmClass);

    // Listen to egg spawns
    BreedingFarm->BreedingFarmComponent->OnEggSpawned.AddDynamic(
        this,
        &AFarmManager::HandleEggSpawned
    );
}

void AFarmManager::HandleEggSpawned(APangeaEggActor* Egg)
{
    UE_LOG(LogTemp, Log, TEXT("Egg spawned: %s"), *Egg->GetName());

    // Track the egg
    TrackedEggs.Add(Egg);

    // Listen to hatch
    Egg->OnEggHatched.AddDynamic(this, &AFarmManager::HandleEggHatched);

    // Show notification
    ShowBreedingNotification(Egg);
}

void AFarmManager::HandleEggHatched(AActor* NewCreature)
{
    APDDinosaurBase* Baby = Cast<APDDinosaurBase>(NewCreature);
    if (!Baby)
        return;

    UE_LOG(LogTemp, Log, TEXT("Dinosaur hatched: %s"), *Baby->GetName());

    // Auto-tame offspring
    if (Baby->TamingComponent)
    {
        Baby->TamingComponent->OnTameResolved(true, ETamedRole::Companion);
    }

    // Register with player's herd
    RegisterTamedDinosaur(Baby);

    // Show notification
    ShowHatchNotification(Baby);
}
```

## Event System Integration

```cpp
UCLASS()
class UDinosaurEventManager : public UObject
{
    GENERATED_BODY()

public:
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDinosaurTamed, APDDinosaurBase*, Dinosaur);
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDinosaursBreed, APDDinosaurBase*, Parent1, APDDinosaurBase*, Parent2);
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDinosaurHatched, APDDinosaurBase*, Baby);

    UPROPERTY(BlueprintAssignable)
    FOnDinosaurTamed OnDinosaurTamed;

    UPROPERTY(BlueprintAssignable)
    FOnDinosaursBreed OnDinosaursBreed;

    UPROPERTY(BlueprintAssignable)
    FOnDinosaurHatched OnDinosaurHatched;

    void RegisterDinosaur(APDDinosaurBase* Dino)
    {
        if (!Dino)
            return;

        // Bind to taming events
        if (Dino->TamingComponent)
        {
            Dino->TamingComponent->OnTameStateChanged.AddDynamic(
                this,
                &UDinosaurEventManager::HandleTameStateChanged
            );
        }

        // Bind to breeding events
        if (Dino->BreedableComponent)
        {
            Dino->BreedableComponent->OnBred.AddDynamic(
                this,
                &UDinosaurEventManager::HandleDinosaurBred
            );
        }
    }

    void HandleTameStateChanged(ETameState NewState)
    {
        if (NewState == ETameState::Tamed)
        {
            APDDinosaurBase* Dino = Cast<APDDinosaurBase>(/* ... */);
            OnDinosaurTamed.Broadcast(Dino);
        }
    }

    void HandleDinosaurBred(APangeaEggActor* Egg, AActor* OtherParent)
    {
        // Track parents and broadcast event
        // ...
    }
};
```

## Save/Load System

```cpp
// Properties are marked SaveGame automatically
UPROPERTY(SaveGame)
TObjectPtr<UPangeaBreedableComponent> BreedableComponent;

UPROPERTY(SaveGame)
TObjectPtr<UPangeaTamingComponent> TamingComponent;

// On save
void SaveDinosaurs()
{
    TArray<AActor*> Dinosaurs;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), APDDinosaurBase::StaticClass(), Dinosaurs);

    for (AActor* Actor : Dinosaurs)
    {
        APDDinosaurBase* Dino = Cast<APDDinosaurBase>(Actor);
        if (Dino && Dino->ALSLoadAndSaveComponent)
        {
            // ALS handles saving automatically
            Dino->ALSLoadAndSaveComponent->SaveActor();
        }
    }
}

// On load
void LoadDinosaurs()
{
    // ALS loads actors automatically
    // Call OnLoaded on each
    TArray<AActor*> Dinosaurs;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), APDDinosaurBase::StaticClass(), Dinosaurs);

    for (AActor* Actor : Dinosaurs)
    {
        APDDinosaurBase* Dino = Cast<APDDinosaurBase>(Actor);
        if (Dino)
        {
            Dino->OnLoaded_Implementation();
        }
    }
}
```

## Best Practices

- Always validate components before accessing
- Handle all tame states in interactions
- Provide clear UI feedback for all actions
- Test save/load thoroughly
- Balance taming/breeding parameters for fun gameplay
- Use events for loose coupling between systems
- Cache component references for performance
- Test multiplayer scenarios if applicable

## See Also

- [Dinosaur AI](dinosaur-ai.md)
- [Dinosaur Base Character](dinosaur-base-character.md)
- [Taming System](taming-system.md)
- [Breeding System](breeding-system.md)

