# Taming States and Roles

Detailed explanation of taming state management and role assignment.

## Tame States

### ETameState Enum

```cpp
UENUM(BlueprintType)
enum class ETameState : uint8
{
    Wild UMETA(DisplayName="Wild"),
    Hostile UMETA(DisplayName="Hostile"),
    Tamed UMETA(DisplayName="Tamed")
};
```

### State Transitions

```
Wild → Taming Attempt → Success → Tamed
                      → Failure → Hostile → (Cooldown) → Wild
```

### Wild State

**Characteristics**:
- Neutral toward player
- Can be tamed
- Applies `WildStateTag` and `WildTeamTag`
- Uses default wild AI

**Entry**: `InitializeWild()` or spawn default

### Hostile State

**Characteristics**:
- Aggressive toward player
- Cannot be tamed (until cooldown expires)
- Applies `HostileStateTag` and `HostileTeamTag`
- May flee if `bRunAwayOnFail` is true

**Entry**: Failed tame attempt, or `InitializeHostile()`

### Tamed State

**Characteristics**:
- Belongs to player
- Follows commands based on role
- Applies `TamedStateTag` and `TamedTeamTag`
- Has role-specific AI controller
- Receives granted abilities

**Entry**: Successful tame with role selection

## Tamed Roles

### ETamedRole Enum

```cpp
UENUM(BlueprintType)
enum class ETamedRole : uint8
{
    None UMETA(DisplayName="None"),
    Mount UMETA(DisplayName="Mount"),
    Companion UMETA(DisplayName="Companion")
};
```

### Mount Role

**Purpose**: Creature can be ridden by player.

**Requirements**:
- `bCanBeMount = true` in species config
- Must have `TamedMountAIController` assigned

**Features**:
- ACF Mount Component integration
- Player can mount/dismount
- Applies `MountRoleTag`

**AI Behavior**: Uses `TamedMountAIController`

### Companion Role

**Purpose**: Creature fights alongside player.

**Requirements**:
- `bCanBeCompanion = true` in species config
- Must have `TamedCompanionAIController` assigned

**Features**:
- Follows player
- Attacks player's enemies
- Responds to commands (stay, follow, attack)
- Applies `CompanionRoleTag`

**AI Behavior**: Uses `TamedCompanionAIController`

## Role Selection Logic

### During Taming

If minigame provides role:
```cpp
OnTameResolved(true, ETamedRole::Mount);
```

If minigame doesn't provide role:
```cpp
ETamedRole DetermineFinalRole(ETamedRole DesiredRole) const
{
    // If specific role requested and valid, use it
    if (DesiredRole == ETamedRole::Mount && TameSpeciesConfig->bCanBeMount)
        return ETamedRole::Mount;
    if (DesiredRole == ETamedRole::Companion && TameSpeciesConfig->bCanBeCompanion)
        return ETamedRole::Companion;

    // Auto-select first available
    if (TameSpeciesConfig->bCanBeMount)
        return ETamedRole::Mount;
    if (TameSpeciesConfig->bCanBeCompanion)
        return ETamedRole::Companion;

    return ETamedRole::None;
}
```

### Changing Roles

```cpp
// Switch from Companion to Mount
TamingComponent->SetTamedRole(ETamedRole::Mount);
```

**Process**:
1. Clear old role tags
2. Apply new role tag
3. Switch AI controller
4. Fire `OnTameRoleSelected` event

## Team Management

### Team Tags

Teams control faction relationships:
- `Teams.Neutral`: Wild creatures
- `Teams.Enemies`: Hostile creatures
- `Teams.Heroes`: Player team (tamed creatures)

### Team Switching

```cpp
void ChangeTeam(const FGameplayTag& TeamTag)
{
    AActor* Owner = GetOwner();
    if (!Owner)
        return;

    // Remove old team tags
    RemoveTagFromActor(Owner, TameSpeciesConfig->WildTeamTag);
    RemoveTagFromActor(Owner, TameSpeciesConfig->HostileTeamTag);
    RemoveTagFromActor(Owner, TameSpeciesConfig->TamedTeamTag);

    // Add new team tag
    AddTagToActor(Owner, TeamTag);
}
```

**Called automatically** during state transitions.

## AI Controller Switching

### Purpose

Different behaviors for different states/roles:
- Wild: Passive, wander, flee from threats
- Hostile: Aggressive, attack player
- Tamed Mount: Follow rider commands
- Tamed Companion: Follow player, attack enemies

### Implementation

```cpp
void SwitchAIController(TSubclassOf<AAIController> NewControllerClass)
{
    APawn* Pawn = Cast<APawn>(GetOwner());
    if (!Pawn || !NewControllerClass)
        return;

    // Destroy old controller
    if (Pawn->GetController())
    {
        Pawn->GetController()->UnPossess();
        Pawn->GetController()->Destroy();
    }

    // Spawn new controller
    AAIController* NewController = GetWorld()->SpawnActor<AAIController>(NewControllerClass);
    if (NewController)
    {
        NewController->Possess(Pawn);
    }
}
```

**Called automatically** when role changes.

## Gameplay Tag Integration

### Checking State in Behavior Trees

```cpp
// In Behavior Tree decorator
bool CheckIsWild()
{
    UAbilitySystemComponent* ASC = GetASC();
    return ASC && ASC->HasMatchingGameplayTag(WildStateTag);
}
```

### Checking Role

```cpp
bool IsMount(AActor* Actor)
{
    UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Actor);
    if (!ASC)
        return false;

    return ASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag("Tame.Role.Mount"));
}
```

## Save/Load Support

### SaveGame Properties

```cpp
UPROPERTY(SaveGame)
ETameState TamedState;

UPROPERTY(SaveGame)
ETamedRole TamedRole;
```

### Loading Saved Creature

```cpp
void UPangeaTamingComponent::HandleLoadedActor()
{
    // Re-apply state after load
    HandleTameStateChanged(TamedState);

    // Re-apply role if tamed
    if (TamedState == ETameState::Tamed && TamedRole != ETamedRole::None)
    {
        SetTamedRole(TamedRole);
    }
}
```

**Call this** after loading saved dinosaurs.

## See Also

- [Taming Component](taming-component.md)
- [Taming Config](taming-config.md)
- [Integration Guide](taming-integration.md)

