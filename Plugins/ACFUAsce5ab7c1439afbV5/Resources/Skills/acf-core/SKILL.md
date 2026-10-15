---
name: acf-core
description: Set up AACFCharacter, UACFCharacterDataAsset, teams, damage, and GameState. Author enemies as Data Assets on a shared character Blueprint.
globs: []
alwaysApply: false
---

# ACF Core — Ascent Combat Framework

The **ACF Core** module is the foundation of the entire combat framework. It provides `AACFCharacter` (the base class for every combatant — player or AI), `UACFCharacterDataAsset` (data-driven configuration), the team/faction system, damage calculation, and the shared `AACFGameState`. All other modules (Inventory, Actions, Quests, Status Effects) build on top of this one.

**Hard rule for new characters:** enemies, NPCs, companions, and stat/mesh variants are `UACFCharacterDataAsset`s
on a shared `AACFCharacter` Blueprint. Do not create a new character Blueprint or custom pawn logic unless
strictly necessary (unique player input/camera, a different locomotion stack, or actor logic fragments cannot
add). See the `character-data-assets` skill. Reuse existing combat-behaviour Data Assets; do not create new ones
unless no existing asset fits.

---

## 1 — Understand the assets

| Asset | Class | Location (sample) | Purpose |
|---|---|---|---|
| `BP_ACFCharacter` (sample) | `AACFCharacter` | `/AscentCombatFramework/Characters/` | Base character with all ACF components pre-wired |
| `DA_CharacterData` (sample) | `UACFCharacterDataAsset` | `/AscentCombatFramework/Data/` | Stats, starting items, ability set, team, mesh data |
| `ACFGameState` | `AACFGameState` | Engine class / your BP subclass | Holds TeamManager, EffectsDispatcher, DifficultyManager |
| `ACFDeveloperSettings` | `UACFDeveloperSettings` | Project Settings → Ascent GAS Settings | Collision channels, difficulty table assignment |

> **Never edit sample assets.** Duplicate into your own content folder first (e.g. `Content/YourGame/Characters/`), then modify your copy.

### Key components on `AACFCharacter`

| Component | Property name | Purpose |
|---|---|---|
| `UACFAbilitySystemComponent` | `ActionsComp` | GAS — triggers actions, manages abilities |
| `UACFCharacterMovementComponent` | `LocomotionComp` | Walk / jog / sprint / strafe locomotion |
| `UARSStatisticsComponent` | `StatisticsComp` | Attributes (Health, Stamina, Mana, etc.) |
| `UACFEquipmentComponent` | `EquipmentComp` | Inventory + equipment slots |
| `UACFDamageHandlerComponent` | `DamageHandlerComp` | Receives damage, immortality toggle |
| `UACFRagdollComponent` | `RagdollComp` | Physics death / ragdoll |
| `UACFEffectsManagerComponent` | `EffetsComp` | VFX / SFX playback |
| `UACFCharacterInitializerComponent` | `InitializerComp` | Loads a `UACFCharacterDataAsset` at BeginPlay |
| `UACFTeamComponent` | `TeamComponent` | Faction / team tag; used by AI perception |

---

## 2 — Setup / Configuration

1. **Create your GameState subclass.** In the Unreal editor, create a Blueprint subclassing `AACFGameState`. Open **Project Settings → Maps & Modes** and set it as the GameState class for your game mode.

2. **Configure teams.** Open your GameState Blueprint and find the `ACFTeamManagerComponent`. Add team `FGameplayTag` entries (e.g. `Teams.Player`, `Teams.Enemy`, `Teams.Neutral`). Register attitude rules (friendly / hostile / neutral) between teams here.

3. **Assign a Difficulty Table (optional).** In **Project Settings → Ascent GAS Settings**, assign your difficulty `UDataTable`. Duplicate the sample at `/AscentCombatFramework/Configuration/DifficultyConfig` first and save under `Content/YourGame/Configuration/`.

4. **Create your CharacterDataAsset.** Duplicate the sample from `/AscentCombatFramework/Data/` (or call
   `SaveCharacter` on `ACFCharacterToolset`) and save it under `Content/YourGame/Data/`. Open your copy and configure:
   - `ChatacterName` — display name.
   - `Team` — GameplayTag matching a team registered in GameState.
   - `CharacterRow` — row from your attribute data table (sets base Health, Stamina, etc.).
   - `LevelingType` — `ECantLevelUp` for fixed NPCs; curve-based for leveling characters.
   - `DefaultAbilitySet` — assign a `UACFAbilitySet` (see Actions System skill).
   - `StartingItems` — array of `FStartingItem` with item class, count, `bAutoEquip`, drop chance.
   - `MeshComponents` — skeletal mesh + material overrides per slot.
   - `CharacterEffectsConfig` — VFX/SFX config DataAsset.
   - `Fragments` — AI, animset, dialogue, components; prefer these over a new Blueprint (see `character-data-assets`).

5. **Reuse a character Blueprint; do not make a new one per character.** Assign the Data Asset to
   `InitializerComp.CharacterInitDataAsset` on an existing `AACFCharacter` (or on a placed instance / spawner
   `AIConfig`). Set `bAutoInit = true` so init runs on BeginPlay. A new Blueprint is only for unique player
   input/camera or a different locomotion family.

6. **Configure fall damage (optional).** On the shared character Blueprint, under **ACF | Fall**:
   - Enable `bEnableFallDamage` and set `FallDamageDistanceThreshold`.
   - Assign a `UCurveFloat` to `FallDamageByFallDistance` mapping distance (cm) → damage.
   - Enable `bTriggerActionOnLand` and set `ActionsToTriggerOnLand` GameplayTag.

---

## 3 — Core Workflow / Usage

### Triggering actions from Blueprints

```
// From any Blueprint that has a reference to AACFCharacter:
Character->TriggerAction(ActionTag, EActionPriority::ELow, false);  // respects priority
Character->ForceAction(ActionTag);                                   // bypasses priority
```

### Switching movesets (weapon equipped / stance)

```
Character->SwitchMoveset(MovesetTag);          // updates locomotion
Character->SwitchMovesetActions(MovesetTag);   // updates ability set
Character->SwitchOverlay(OverlayTag);          // animation overlay
```

### Death / Revive

```
Character->KillCharacter();                        // instant kill (Server RPC)
Character->ReviveCharacter(0.5f);                  // revive with 50% HP
Character->SetIsImmortal(true);                    // prevent all death
```

### Damage activation (weapon swing window)

```
Character->ActivateDamage(EDamageActivationType::EBothWeapons, TraceChannelNames);
Character->DeactivateDamage(EDamageActivationType::EBothWeapons, TraceChannelNames);
```

### Key delegates on `AACFCharacter`

| Delegate | Signature | When it fires |
|---|---|---|
| `OnDeath` | `(AACFCharacter* self)` | Character health reaches zero |
| `OnDamageReceived` | `(FACFDamageEvent)` | Any damage taken |
| `OnDamageInflicted` | `(FACFDamageEvent)` | Any damage dealt |
| `OnCombatTypeChanged` | `(ECombatType)` | Weapon equip state changes |
| `OnCharacterFullyInitialized` | `()` | Initializer component finishes setup |

---

## 4 — Wire to Characters / Blueprints

### Player pawn

1. Open your player character Blueprint (subclass of `AACFCharacter`).
2. Confirm `UACFAbilitySystemComponent`, `UACFEquipmentComponent`, and `UACFCharacterMovementComponent` are present (they are auto-added by the parent class).
3. Assign your `UACFCharacterDataAsset` to the `InitializerComp` slot in the component details.
4. Implement `SetupPlayerInputComponentBP` (BlueprintImplementableEvent) to bind Enhanced Input actions to `TriggerAction` calls.
5. Bind `OnDeath` to show a death screen / respawn logic.

### AI characters / enemies / NPCs

1. Reuse the project's existing AI `AACFCharacter` Blueprint. Do **not** duplicate it per enemy.
2. Author a `UACFCharacterDataAsset` (`Team = Teams.Enemy` or Neutral) and assign it as spawner `AIConfig`
   or the instance `CharacterInitDataAsset`.
3. Put AI on Data Asset fragments. Reuse an existing combat-behaviour Data Asset (`character-data-assets`,
   `ai-framework`). Do not create a new one with `SaveDataAsset` unless none fit.
4. `UACFTeamComponent` is already on the base class. `UACFDamageHandlerComponent` handles death; bind `OnDeath` only if you need extra loot/despawn.

### BoneNameToDamageZoneMap

Populate `BoneNameToDamageZoneMap` on the character's Class Defaults to map skeleton bones (e.g. `head`, `spine_01`) to `EDamageZone` values (`EHead`, `ETorso`, `ELeg`). The damage system reads this to calculate zone multipliers.

---

## 5 — Verify

**Checklist before testing:**

- [ ] `AACFGameState` subclass is assigned in Project Settings → Maps & Modes.
- [ ] Teams are registered in `UACFTeamManagerComponent` with correct attitude rules.
- [ ] `UACFCharacterDataAsset` is duplicated from sample (or created with `SaveCharacter`) under `Content/YourGame/`.
- [ ] New enemies/NPCs did **not** get a new character Blueprint; they reuse a shared spawn class.
- [ ] Character DataAsset `Team` tag matches a registered team in GameState.
- [ ] `CharacterRow` in the DataAsset resolves to a valid row in the Attribute data table.
- [ ] `DefaultAbilitySet` is assigned on the DataAsset.
- [ ] `bAutoInit = true` on the character Blueprint or DataAsset is initializing on BeginPlay.
- [ ] `BoneNameToDamageZoneMap` populated for head/critical zone multipliers.
- [ ] `EDeathType` set correctly (`EDeathAction` plays a death montage; `ERagdoll` enables ragdoll).

**Common failures:**

| Symptom | Fix |
|---|---|
| Characters have no stats / zero health | `CharacterRow` is empty or points to a missing DataTable row |
| Character doesn't die / takes no damage | Team attitude is set to `Friendly` or `DamageHandlerComp` has `bIsImmortal = true` |
| AI not attacking player | Teams are not registered as hostile in `UACFTeamManagerComponent` |
| `OnCharacterFullyInitialized` never fires | `bAutoInit` is false; call `InitializerComp->InitializeCharacter()` manually |
| Movement feels wrong (no strafing) | `RotationMode` in DataAsset is set to `EOrientToMovement` instead of `EStrafing` |
| Crash on BeginPlay | DataAsset is null on `InitializerComp`; assign `CharacterInitDataAsset` on the shared pawn or the placed instance |
