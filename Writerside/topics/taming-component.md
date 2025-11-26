# Taming Component

Detailed documentation for `UPangeaTamingComponent`.

## Properties

### Configuration

```cpp
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Config", SaveGame)
UTameSpeciesConfig* TameSpeciesConfig;
```
Species-specific taming configuration (requirements, behavior, abilities).

### Runtime State

```cpp
UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="State", SaveGame)
ETameState TamedState = ETameState::Wild;
```
Current tame state: Wild, Hostile, or Tamed.

```cpp
UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="State", SaveGame)
ETamedRole TamedRole = ETamedRole::None;
```
Current role: None, Mount, or Companion.

## Core Functions

### InitializeWild
```cpp
UFUNCTION(BlueprintCallable, Category="Taming")
void InitializeWild();
```
**Purpose**: Set creature to Wild state.

**Behavior**:
- Sets `TamedState = Wild`
- Applies wild team tag
- Applies wild state tag
- Clears role tags

**Usage**: Call on creature spawn if it should be wild.

### InitializeHostile
```cpp
UFUNCTION(BlueprintCallable, Category="Taming")
void InitializeHostile();
```
**Purpose**: Set creature to Hostile state.

**Behavior**:
- Sets `TamedState = Hostile`
- Applies hostile team tag  
- Applies hostile state tag
- Clears role tags

**Usage**: Call if creature should be aggressive toward player.

### StartTameAttempt
```cpp
UFUNCTION(BlueprintCallable, Category="Taming")
void StartTameAttempt(AActor* Instigator);
```
**Purpose**: Begin taming process.

**Parameters**:
- `Instigator`: The actor attempting to tame (usually player)

**Validation**:
1. Check if already tamed
2. Validate `TameSpeciesConfig` exists
3. Check prerequisites (stats, items) via `CheckTamePrerequisites()`
4. Check if on cooldown

**Behavior**:
- Caches instigator
- Starts minigame UI
- Applies "InProgress" tag

**Usage**:
```cpp
TamingComponent->StartTameAttempt(PlayerPawn);
```

### OnTameResolved
```cpp
UFUNCTION(BlueprintCallable, Category="Taming")
void OnTameResolved(bool bSuccess, ETamedRole DesiredRole);
```
**Purpose**: Handle taming result from minigame.

**Parameters**:
- `bSuccess`: Whether taming succeeded
- `DesiredRole`: Player's choice of role (if successful)

**Success Behavior**:
- Transitions to Tamed state
- Sets role to `DesiredRole` (or determines automatically)
- Consumes taming items
- Applies success effects
- Grants tamed abilities
- Switches to appropriate AI controller
- Changes team tag
- Fires `OnTameStateChanged` event

**Failure Behavior**:
- Transitions to Hostile (if configured)
- Starts retry cooldown
- Optionally triggers flee behavior
- Fires `OnTameStateChanged` event

### SetTamedRole
```cpp
UFUNCTION(BlueprintCallable, Category="Taming")
void SetTamedRole(ETamedRole NewRole);
```
**Purpose**: Change role of already-tamed creature.

**Behavior**:
- Updates `TamedRole`
- Switches AI controller to match role
- Updates role gameplay tags
- Fires `OnTameRoleSelected` event

**Usage**:
```cpp
// Switch from Companion to Mount
TamingComponent->SetTamedRole(ETamedRole::Mount);
```

## Events

### OnTameStateChanged
```cpp
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTameStateChanged, ETameState, NewState);

UPROPERTY(BlueprintAssignable)
FTameStateChanged OnTameStateChanged;
```

**Fired**: When tame state changes (Wild/Hostile/Tamed).

**Usage**:
```cpp
TamingComponent->OnTameStateChanged.AddDynamic(this, &AMyController::HandleTameStateChanged);

void AMyController::HandleTameStateChanged(ETameState NewState)
{
    switch (NewState)
    {
        case ETameState::Tamed:
            ShowNotification("Dinosaur tamed!");
            break;
        case ETameState::Hostile:
            ShowNotification("Dinosaur is hostile!");
            break;
    }
}
```

### OnTameRoleSelected
```cpp
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTameRoleSelected, ETamedRole, Role);

UPROPERTY(BlueprintAssignable)
FTameRoleSelected OnTameRoleSelected;
```

**Fired**: When role changes.

## Minigame Integration

### Starting Minigame

```cpp
void UPangeaTamingComponent::StartMinigame(AActor* Instigator, AActor* Target, float Duration)
{
    if (!MinigameWidgetClass)
        return;

    APlayerController* PC = Cast<APlayerController>(Instigator->GetInstigatorController());
    if (!PC)
        return;

    UTamingWidget* Widget = CreateWidget<UTamingWidget>(PC, MinigameWidgetClass);
    Widget->Initialize(this, Duration);
    Widget->AddToViewport();

    ActiveMinigameWidget = Widget;
}
```

### Minigame Result

```cpp
void UPangeaTamingComponent::OnMinigameResult(bool bSuccess)
{
    if (ActiveMinigameWidget)
    {
        ActiveMinigameWidget->RemoveFromParent();
        ActiveMinigameWidget = nullptr;
    }

    // If minigame doesn't provide role, use default logic
    ETamedRole Role = DetermineFinalRole(ETamedRole::None);
    OnTameResolved(bSuccess, Role);
}
```

## Internal Helpers

### CheckTamePrerequisites
```cpp
bool CheckTamePrerequisites(AActor* Instigator) const
{
    // Check stats
    if (!HasRequiredStats(Instigator))
        return false;

    // Check items
    if (!HasRequiredItem(Instigator))
        return false;

    return true;
}
```

### HasRequiredStats
```cpp
bool HasRequiredStats(AActor* Instigator) const
{
    if (!TameSpeciesConfig)
        return true;

    UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Instigator);
    if (!ASC)
        return false;

    for (const FTameStatRequirement& StatReq : TameSpeciesConfig->StatRequirements)
    {
        float Value = ASC->GetNumericAttribute(StatReq.Attribute);
        if (Value < StatReq.MinValue)
            return false;
    }

    return true;
}
```

### HasRequiredItem
```cpp
bool HasRequiredItem(AActor* Instigator) const
{
    if (!TameSpeciesConfig || !TameSpeciesConfig->RequiredTamingItem)
        return true;

    const UACFInventoryComponent* Inventory = Instigator->FindComponentByClass<UACFInventoryComponent>();
    return Inventory && Inventory->HasAnyItemOfType(TameSpeciesConfig->RequiredTamingItem) &&
           Inventory->GetTotalCountOfItemsByClass(TameSpeciesConfig->RequiredTamingItem) >= TameSpeciesConfig->RequiredItemCount;
}
```

## See Also

- [Taming Config](taming-config.md) - Species configuration
- [States & Roles](taming-states-roles.md) - State management
- [Integration Guide](taming-integration.md) - Full integration

