# Dinosaur AI System

The **PangeaDinosaurAI** module provides the base character class (`APDDinosaurBase`) that integrates all dinosaur-related systems.

## Overview

`APDDinosaurBase` serves as the foundation for all dinosaur characters in Pangea Dawn. It:
- Inherits from `AACFCharacter` (Ascent Combat Framework)
- Integrates taming, breeding, and mounting systems
- Implements interaction and save/load interfaces
- Provides dinosaur-specific movement mechanics

## Class Hierarchy

```
AACFCharacter (ACF Plugin)
└── APDDinosaurBase
    ├── Taming System Integration
    ├── Breeding System Integration
    ├── Mount System Integration
    ├── Vaulting System Integration (parkour movement)
    └── Save/Load System Integration
```

## Core Components

### Integrated Components

```cpp
UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
TObjectPtr<UPangeaBreedableComponent> BreedableComponent;

UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
TObjectPtr<UPangeaTamingComponent> TamingComponent;

UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
TObjectPtr<UACFMountComponent> MountComponent;

UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
TObjectPtr<UACFVaultComponent> VaultComponent;

UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
TObjectPtr<UALSLoadAndSaveComponent> ALSLoadAndSaveComponent;
```

## Interaction System

### IACFInteractableInterface

```cpp
virtual bool CanBeInteracted_Implementation(APawn* Pawn) override;
virtual void OnInteractedByPawn_Implementation(APawn* Pawn, const FString& interactionType) override;
virtual FText GetInteractableName_Implementation() override;
```

**Usage**: Allows players to interact with dinosaurs for:
- Taming wild dinosaurs
- Mounting tamed dinosaurs  
- Issuing commands to companions

### Example Implementation

```cpp
bool APDDinosaurBase::CanBeInteracted_Implementation(APawn* Pawn)
{
    // Can be interacted with if not mounted and if can be a mount or companion
    return !MountComponent->IsMounted() && TamingComponent && TamingComponent->TameSpeciesConfig &&
        (TamingComponent->TameSpeciesConfig->bCanBeMount || TamingComponent->TameSpeciesConfig->bCanBeCompanion);
}

void APDDinosaurBase::OnInteractedByPawn_Implementation(APawn* Pawn, const FString& interactionType)
{
    if (!TamingComponent || TamingComponent->TamedState != ETameState::Tamed)
    {
        // Not tamed yet - start a taming attempt
        TamingComponent->StartTameAttempt(Pawn);
        return;
    }

    if (TamingComponent->TamedRole == ETamedRole::Mount)
    {
        // Request mount/dismount via gameplay tags
        AACFCharacter* ACFCharacter = Cast<AACFCharacter>(Pawn);
        if (!ACFCharacter) return;

        FGameplayTag MountTag = FGameplayTag::RequestGameplayTag("Actions.Mount");
        FGameplayTag DismountTag = FGameplayTag::RequestGameplayTag("Actions.Dismount");

        if (MountComponent->IsMounted())
        {
            ACFCharacter->TriggerAction(DismountTag, EActionPriority::ELow, false);
        }
        else
        {
            ACFCharacter->TriggerAction(MountTag, EActionPriority::EHigh, false);
        }
    }
    else if (TamingComponent->TamedRole == ETamedRole::Companion)
    {
        // Companion interaction (pet, command menu, etc.)
        UE_LOG(LogEngine, Display, TEXT("You Pet Dino!"));
    }
}

FText APDDinosaurBase::GetInteractableName_Implementation()
{
    if (TamingComponent && TamingComponent->TamedState == ETameState::Wild)
    {
        return FText::FromString("Tame");
    }

    if (TamingComponent && TamingComponent->TamedState == ETameState::Tamed)
    {
        if (TamingComponent->TamedRole == ETamedRole::Mount)
        {
            return FText::FromString("Mount");
        }
        if (TamingComponent->TamedRole == ETamedRole::Companion)
        {
            return FText::FromString("Pet");
        }
    }
    
    return FText::FromString("Interact");
}
```

## Movement System

### Dinosaur-Specific Movement

```cpp
UPROPERTY(VisibleDefaultsOnly, BlueprintReadWrite, Category="Dinosaur Movement")
bool bIsAccelerating;

UPROPERTY(VisibleDefaultsOnly, BlueprintReadWrite, Category="Dinosaur Movement")
bool bIsBraking;

UPROPERTY(VisibleDefaultsOnly, BlueprintReadWrite, Category="Dinosaur Movement")
float DefaultAcceleration = 10.0f;

UPROPERTY(VisibleDefaultsOnly, BlueprintReadWrite, Category="Dinosaur Movement")
float DefaultDeceleration = -0.2f;
```

### Movement Functions

```cpp
UFUNCTION(BlueprintCallable, Category="Dinosaur Movement")
void Accelerate(float Value);

UFUNCTION(BlueprintCallable, Category="Dinosaur Movement")
void Brake(float Value);

void ChangeVelocityState();
```

**Purpose**: Provides dinosaur-specific movement feel (acceleration, momentum).

## Save/Load Integration

### IALSSavableInterface

```cpp
virtual TArray<UActorComponent*> GetComponentsToSave_Implementation() const override;
virtual void OnLoaded_Implementation() override;
```

### Implementation

```cpp
TArray<UActorComponent*> APDDinosaurBase::GetComponentsToSave_Implementation() const
{
    TArray<UActorComponent*> Components;

    // Save breeding and taming data
    if (BreedableComponent)
        Components.Add(BreedableComponent);
    if (TamingComponent)
        Components.Add(TamingComponent);

    // Add other saveable components
    // ACF components handled by parent class

    return Components;
}

void APDDinosaurBase::OnLoaded_Implementation()
{
    Super::OnLoaded_Implementation();

    // Restore taming state
    if (TamingComponent)
    {
        TamingComponent->HandleLoadedActor();
    }

    // Restore other systems as needed
}
```

## Setup Example

### Creating a Dinosaur Blueprint

1. Create Blueprint based on `PDDinosaurBase`
2. Configure mesh and animations
3. Set up components:
   - `BreedableComponent`: Set species data, gender, initial traits
   - `TamingComponent`: Set tame species config
   - `MountComponent`: Configure mounting points (if mount-capable)
   - `VaultComponent`: Configure vaulting detection (for obstacle traversal)
4. Configure movement settings
5. Set up AI controller reference

### Spawning Dinosaurs

```cpp
APDDinosaurBase* SpawnWildRaptor(FVector Location)
{
    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    APDDinosaurBase* Raptor = GetWorld()->SpawnActor<APDDinosaurBase>(
        RaptorBlueprintClass,
        Location,
        FRotator::ZeroRotator,
        SpawnParams
    );

    if (Raptor)
    {
        // Initialize as wild
        Raptor->TamingComponent->TameSpeciesConfig = RaptorTameConfig;
        Raptor->TamingComponent->InitializeWild();

        // Initialize breeding
        Raptor->BreedableComponent->SpeciesData = RaptorSpeciesData;
        Raptor->BreedableComponent->Gender = FMath::RandBool() ? ECreatureGender::Male : ECreatureGender::Female;
        Raptor->BreedableComponent->bIsFertile = true;

        // Set random initial traits
        Raptor->BreedableComponent->GeneticTraits.SetValue("Strength", FMath::RandRange(60.0f, 90.0f));
        Raptor->BreedableComponent->GeneticTraits.SetValue("Speed", FMath::RandRange(70.0f, 100.0f));
    }

    return Raptor;
}
```

## Integration Points

### With Taming System

```cpp
// Check if dinosaur is tameable
if (Dinosaur->TamingComponent->GetTameState() == ETameState::Wild)
{
    // Start taming
}

// Check if can be mounted
if (Dinosaur->TamingComponent->GetTamedRole() == ETamedRole::Mount)
{
    // Activate mount
}
```

### With Breeding System

```cpp
// Check if can breed
if (Dinosaur->BreedableComponent->IsFertile())
{
    // Move to breeding farm
}

// Display genetics
for (const FGeneticTrait& Trait : Dinosaur->BreedableComponent->GeneticTraits.Traits)
{
    DisplayTrait(Trait.Name, Trait.Value);
}
```

### With Mount System

```cpp
// Player mounts dinosaur
if (Dinosaur->MountComponent)
{
    Dinosaur->MountComponent->StartMount(PlayerPawn);
}

// Player dismounts
if (Dinosaur->MountComponent->IsMounted())
{
    Dinosaur->MountComponent->StopMount();
}
```

### With Vaulting System

```cpp
// Enable/disable vaulting for mounted dinosaur
if (Dinosaur->VaultComponent)
{
    Dinosaur->VaultComponent->SetVaultingEnabled(true);
}
```

## See Also

- [Dinosaur Base Character](dinosaur-base-character.md) - Detailed API reference
- [Dinosaur Integration](dinosaur-integration.md) - Full integration guide
- [Taming System](taming-system.md) - Taming mechanics
- [Breeding System](breeding-system.md) - Breeding mechanics

