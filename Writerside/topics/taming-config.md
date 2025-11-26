# Taming Configuration

Documentation for `UTameSpeciesConfig` data asset.

## Creating Species Config

1. Right-click in Content Browser → Miscellaneous → Data Asset
2. Choose `TameSpeciesConfig`
3. Name it (e.g., `DA_RaptorTameConfig`)

## Properties Reference

### Roles

```cpp
UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Roles")
bool bCanBeMount = true;

UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Roles")
bool bCanBeCompanion = true;
```
Define which roles this species can fulfill.

```cpp
UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Roles")
TSubclassOf<AAIController> TamedMountAIController;

UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Roles")
TSubclassOf<AAIController> TamedCompanionAIController;
```
AI controllers to use for each role.

### Requirements

```cpp
UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Taming|Requirements")
TSubclassOf<UACFItem> RequiredTamingItem;

UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Taming|Requirements")
int32 RequiredItemCount = 1;
```
Item requirements for taming.

```cpp
UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Taming|Requirements")
TArray<FTameStatRequirement> StatRequirements;
```
Player stat requirements:
```cpp
struct FTameStatRequirement
{
    FGameplayAttribute Attribute;  // e.g., Attributes.Dexterity
    FGameplayTag DisplayTag;       // For UI display
    float MinValue;                // Minimum required value
};
```

### Behavior

```cpp
UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Taming|Behavior")
float TameDuration = 5.f;
```
How long taming minigame lasts.

```cpp
UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Taming|Behavior")
float RetryTameCooldown = 5.f;
```
Cooldown after failed tame attempt.

```cpp
UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Taming|Behavior")
bool bRunAwayOnFail = true;

UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Taming|Behavior")
FGameplayTag RunawayActionTag = FGameplayTag::RequestGameplayTag("Actions.Flee");
```
Behavior on tame failure.

### GAS Integration

```cpp
UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GAS")
FGameplayTag TameAbilityTag;
```
Tag for taming ability activation.

```cpp
UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GAS")
TArray<TSubclassOf<UGameplayAbility>> AbilitiesGrantedWhenTamed;
```
Abilities creature gains when tamed.

```cpp
UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GAS")
TArray<TSubclassOf<UGameplayEffect>> EffectsOnTameSuccess;
```
Effects applied on successful taming.

### Gameplay Tags

**State Tags**:
- `WildStateTag`: Applied when wild (default: `Tame.State.Wild`)
- `HostileStateTag`: Applied when hostile (default: `Tame.State.Hostile`)
- `TamedStateTag`: Applied when tamed (default: `Tame.State.Tamed`)

**Team Tags**:
- `WildTeamTag`: Wild creature team (default: `Teams.Neutral`)
- `HostileTeamTag`: Hostile creature team (default: `Teams.Enemies`)
- `TamedTeamTag`: Tamed creature team (default: `Teams.Heroes`)

**Role Tags**:
- `MountRoleTag`: Applied to mounts (default: `Tame.Role.Mount`)
- `CompanionRoleTag`: Applied to companions (default: `Tame.Role.Companion`)

**Attempt Tags**:
- `InProgressTag`: Applied during taming (default: `Tame.Attempt.InProgress`)
- `SuccessTag`: Applied on success (default: `Tame.Attempt.Success`)
- `FailTag`: Applied on failure (default: `Tame.Attempt.Fail`)

## Example Configuration

### Easy Tame (Dodo)

```
Roles:
  ✓ Can Be Mount: Yes
  ✓ Can Be Companion: Yes

Requirements:
  - Required Item: Berries x3
  - Stat Requirements: (none)

Behavior:
  - Tame Duration: 3 seconds
  - Retry Cooldown: 5 seconds
  - Run Away on Fail: No
```

### Hard Tame (T-Rex)

```
Roles:
  ✓ Can Be Mount: Yes
  ✗ Can Be Companion: No

Requirements:
  - Required Item: Prime Meat x10
  - Stat Requirements:
    * Strength >= 25
    * Dexterity >= 20

Behavior:
  - Tame Duration: 10 seconds
  - Retry Cooldown: 30 seconds
  - Run Away on Fail: Yes
  - Runaway Action: Actions.Flee.Aggressive
```

## See Also

- [Taming Component](taming-component.md)
- [States & Roles](taming-states-roles.md)
- [Integration Guide](taming-integration.md)

