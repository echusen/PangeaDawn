# Dinosaur Base Character Reference

Detailed API reference for `APDDinosaurBase`.

## Class Definition

```cpp
UCLASS()
class PANGEADINOSAURAI_API APDDinosaurBase : public AACFCharacter,
    public IACFInteractableInterface,
    public IALSSavableInterface
{
    GENERATED_BODY()
};
```

## Constructor

```cpp
APDDinosaurBase::APDDinosaurBase(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    // Create components
    BreedableComponent = CreateDefaultSubobject<UPangeaBreedableComponent>(TEXT("Breedable"));
    TamingComponent = CreateDefaultSubobject<UPangeaTamingComponent>(TEXT("Taming"));
    MountComponent = CreateDefaultSubobject<UACFMountComponent>(TEXT("Mount"));
    VaultComponent = CreateDefaultSubobject<UACFVaultComponent>(TEXT("Vault"));
    ALSLoadAndSaveComponent = CreateDefaultSubobject<UALSLoadAndSaveComponent>(TEXT("LoadAndSave"));

    // Configure defaults
    PrimaryActorTick.bCanEverTick = true;
}
```

## Properties

### System Components

```cpp
UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", SaveGame)
TObjectPtr<UPangeaBreedableComponent> BreedableComponent;
```
Handles breeding, genetics, and fertility.

```cpp
UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", SaveGame)
TObjectPtr<UPangeaTamingComponent> TamingComponent;
```
Handles taming states and roles.

```cpp
UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
TObjectPtr<UACFMountComponent> MountComponent;
```
Handles player mounting/dismounting.

```cpp
UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
TObjectPtr<UACFVaultComponent> VaultComponent;
```
Enables vaulting and mantling over obstacles (parkour-style movement).

```cpp
UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
TObjectPtr<UALSLoadAndSaveComponent> ALSLoadAndSaveComponent;
```
Handles save/load persistence.

### Movement Properties

```cpp
UPROPERTY(VisibleDefaultsOnly, BlueprintReadWrite, Category="Dinosaur Movement")
bool bIsAccelerating;
```
Whether dinosaur is currently accelerating.

```cpp
UPROPERTY(VisibleDefaultsOnly, BlueprintReadWrite, Category="Dinosaur Movement")
bool bIsBraking;
```
Whether dinosaur is currently braking.

```cpp
UPROPERTY(VisibleDefaultsOnly, BlueprintReadWrite, Category="Dinosaur Movement")
FName HeadSocket;
```
Socket name for head (used for camera, mounting).

```cpp
UPROPERTY(VisibleDefaultsOnly, BlueprintReadWrite, Category="Dinosaur Movement")
float DefaultAcceleration = 10.0f;
```
Acceleration rate.

```cpp
UPROPERTY(VisibleDefaultsOnly, BlueprintReadWrite, Category="Dinosaur Movement")
float DefaultDeceleration = -0.2f;
```
Deceleration/braking rate.

```cpp
UPROPERTY(VisibleDefaultsOnly, BlueprintReadWrite, Category="Dinosaur Movement")
TSoftClassPtr<AACFCharacter> PlayerRider;
```
Reference to current rider (if mounted).

## Core Functions

### BeginPlay
```cpp
virtual void BeginPlay() override;
```
**Purpose**: Initialize components and systems.

**Override to**:
- Set initial tame state
- Configure breeding parameters
- Set up AI controller

### Tick
```cpp
virtual void Tick(float DeltaTime) override;
```
**Purpose**: Update per-frame logic.

**Default Behavior**:
- Calls `ChangeVelocityState()` for movement updates

### Accelerate
```cpp
UFUNCTION(BlueprintCallable, Category="Dinosaur Movement")
void Accelerate(float Value);
```
**Purpose**: Apply acceleration input.

**Parameters**:
- `Value`: Acceleration amount (0-1 typically)

**Usage**:
```cpp
// In input binding
Dinosaur->Accelerate(1.0f);
```

### Brake
```cpp
UFUNCTION(BlueprintCallable, Category="Dinosaur Movement")
void Brake(float Value);
```
**Purpose**: Apply braking input.

**Parameters**:
- `Value`: Braking amount (0-1 typically)

### ChangeVelocityState
```cpp
void ChangeVelocityState();
```
**Purpose**: Internal - updates movement based on acceleration/braking state.

## Interaction Interface

### CanBeInteracted
```cpp
virtual bool CanBeInteracted_Implementation(APawn* Pawn) override;
```
**Purpose**: Check if pawn can interact with this dinosaur.

**Returns**: `true` if interaction allowed.

**Default**: Returns `true` (always interactable).

### OnInteractedByPawn
```cpp
virtual void OnInteractedByPawn_Implementation(APawn* Pawn, const FString& interactionType) override;
```
**Purpose**: Handle interaction from pawn.

**Parameters**:
- `Pawn`: The interacting pawn
- `interactionType`: Optional interaction type string

**Default Behavior**: Checks tame state and initiates appropriate action.

### GetInteractableName
```cpp
virtual FText GetInteractableName_Implementation() override;
```
**Purpose**: Get display name for interaction prompt.

**Returns**: Localized text for UI display.

## Save/Load Interface

### GetComponentsToSave
```cpp
virtual TArray<UActorComponent*> GetComponentsToSave_Implementation() const override;
```
**Purpose**: Specify which components should be saved.

**Returns**: Array of components with `SaveGame` properties.

**Default**: Returns `BreedableComponent` and `TamingComponent`.

### OnLoaded
```cpp
virtual void OnLoaded_Implementation() override;
```
**Purpose**: Called after actor is loaded from save.

**Default Behavior**:
- Restores taming state via `TamingComponent->HandleLoadedActor()`
- Can be overridden to restore additional state

## Usage Examples

### Spawning and Configuring

```cpp
void SpawnConfiguredDinosaur()
{
    APDDinosaurBase* Dino = GetWorld()->SpawnActor<APDDinosaurBase>(
        DinosaurClass,
        SpawnLocation,
        FRotator::ZeroRotator
    );

    if (Dino)
    {
        // Taming setup
        Dino->TamingComponent->TameSpeciesConfig = TameConfig;
        Dino->TamingComponent->InitializeWild();

        // Breeding setup
        Dino->BreedableComponent->SpeciesData = SpeciesData;
        Dino->BreedableComponent->Gender = ECreatureGender::Male;
        Dino->BreedableComponent->bIsFertile = true;

        // Initialize traits
        FGeneticTraitSet Traits;
        Traits.SetValue("Strength", 75.0f);
        Traits.SetValue("Speed", 85.0f);
        Dino->BreedableComponent->GeneticTraits = Traits;
    }
}
```

### Custom Interaction

```cpp
void APDDinosaurBase::OnInteractedByPawn_Implementation(APawn* Pawn, const FString& interactionType)
{
    if (!TamingComponent || TamingComponent->TamedState != ETameState::Tamed)
    {
        // Not tamed - start taming attempt
        TamingComponent->StartTameAttempt(Pawn);
        return;
    }

    // Handle based on role
    if (TamingComponent->TamedRole == ETamedRole::Mount)
    {
        HandleMountInteraction(Pawn);
    }
    else if (TamingComponent->TamedRole == ETamedRole::Companion)
    {
        HandleCompanionInteraction(Pawn, interactionType);
    }
}

void APDDinosaurBase::HandleMountInteraction(APawn* Pawn)
{
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

void APDDinosaurBase::HandleCompanionInteraction(APawn* Pawn, const FString& interactionType)
{
    if (interactionType == "Command")
    {
        OpenCommandMenu(Pawn);
    }
    else
    {
        // Default: pet the dinosaur
        UE_LOG(LogEngine, Display, TEXT("You Pet Dino!"));
    }
}
```

### Movement Control (for Mounted Riding)

```cpp
void APDDinosaurBase::HandleRiderInput(float Forward, float Right)
{
    if (!MountComponent || !MountComponent->IsMounted())
        return;

    // Apply acceleration based on input
    if (Forward > 0.0f)
    {
        Accelerate(Forward);
    }
    else if (Forward < 0.0f)
    {
        Brake(FMath::Abs(Forward));
    }

    // Apply turning
    AddControllerYawInput(Right * TurnRate * GetWorld()->GetDeltaSeconds());
}
```

## See Also

- [Dinosaur AI](dinosaur-ai.md) - System overview
- [Dinosaur Integration](dinosaur-integration.md) - Integration guide
- [Taming System](taming-system.md)
- [Breeding System](breeding-system.md)

