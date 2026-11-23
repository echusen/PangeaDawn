---
name: character-data-assets
description: Author ACF enemies, NPCs, and character variants as UACFCharacterDataAsset. Reuse an existing character Blueprint. Create a new character Blueprint only when strictly necessary.
globs: []
alwaysApply: false
---

# Character Data Assets — ACF Enemies, NPCs, Variants

**Hard rule:** a new enemy, NPC, companion, or visual/stat variant is a `UACFCharacterDataAsset`, not a new character Blueprint and not custom pawn logic.

Reuse one shared `AACFCharacter` class (the project's existing AI/NPC pawn, or `ACFCharacterBP` possessed by
`AACFAIController`) as the spawn class. Put every difference on the Data Asset. Group spawners and unit rosters
already take `AIClassBP` plus `AIConfig` for this split.

Create or subclass a character Blueprint only when the Data Asset and its fragments cannot do the job. Valid
exceptions: a unique player pawn (input and camera — see `player-characters`), a different locomotion stack
(Motion Matching vs GASP), or actor-level logic that `UACFComponentsFragment` cannot add.

**Not** a reason to make a Blueprint: another mesh, name, team, stats, items, abilities, capsule size, AI
aggression, behavior tree, patrol, routine, dialogue, or execution conditions.

**Tools:** `ACFCharacterToolset` (`DescribeCharacterData`, `ListCharacterFragmentClasses`, `DescribeCharacterFragment`,
`ExportCharacter`, `ValidateCharacter`, `SaveCharacter`). For combat AI, search
existing `UACFCombatBehaviorDataAsset` assets with `ListAssets` and assign the match through
`UACFCombatBehaviourFragment`. Creating one with `SaveDataAsset` is last-resort: almost never create a new combat
behaviour. Schedules: reuse an existing routine (`ListAssets` with `ACFAIRoutineDataAsset`), or create one with `SaveDataAsset`, then `UACFAIStateFragment`.

---

## 1 — Understand the assets

| Asset | Class | Role |
|---|---|---|
| Character Data Asset | `UACFCharacterDataAsset` | The character: stats, mesh, team, items, abilities, capsule, fragments |
| Shared AI pawn | `AACFCharacter` Blueprint | Spawn class only. One (or few) per locomotion family, not per enemy |
| Combat behaviour | `UACFCombatBehaviorDataAsset` | Reuse an existing asset; apply via `UACFCombatBehaviourFragment` |
| NPC routine | `UACFAIRoutineDataAsset` | Daily schedule; applied via `UACFAIStateFragment` |
| Spawn entry | `FAISpawnInfo` / `FBaseUnit` | `AIClassBP` (shared class) + `AIConfig` (this Data Asset) + level |

> **Never edit sample assets.** Duplicate a sample `UACFCharacterDataAsset` into `Content/YourGame/Characters/` (or call `SaveCharacter`), then edit your copy.

The initializer is `UACFCharacterInitializerComponent` (`InitializerComp`). Placed actors use `CharacterInitDataAsset` plus `CharacterInitLevel`. Spawners call `InitFromDataAsset` with `AIConfig`.

---

## 2 — Setup / Configuration

1. **Pick the shared spawn class.** Find the project's existing AI/NPC `AACFCharacter` Blueprint (AI Controller
   Class = `AACFAIController`, Auto Possess AI = Placed in World or Spawned). Do not duplicate it for this character.
2. **Create the Data Asset** with `ValidateCharacter` then `SaveCharacter` (or duplicate a sample). Typical `Properties` keys
   (paths from `DescribeCharacterData`):
   - `ChatacterName` — display name (the property is spelled this way).
   - `Team` — GameplayTag registered on the GameState team manager (`Teams.Enemies`, `Teams.Neutral`, ...).
   - `CharacterRow` — attribute table row (Health, Stamina, ...).
   - `LevelingType` — `ECantLevelUp` for most NPCs and enemies.
   - `DefaultAbilitySet` — path to a `UACFAbilitySet`.
   - `MeshComponents` — skeletal mesh and materials per component tag.
   - `CapsuleHalfHeight` / `CapsuleRadius` — centimeters.
   - `RotationMode` — `EStrafing` or `EForwardFacing`.
   - Fragments go in the spec's `Fragments` list, not in `Properties` (see below).
3. **Starting items** in the spec's `StartingItems` list: `ItemClass`, `Count`, `bAutoEquip`, `DropChancePercentage`
   (auto-equip weapons here, not on the Blueprint). A non-empty list replaces the current one.
4. **AI, without a new controller Blueprint or a new combat behaviour:**
   - `ListAssets` with AssetClass `ACFCombatBehaviorDataAsset` (and `ACFBaseCombatBehaviorDataAsset` if needed).
     Pick an existing asset that matches melee/ranged/flee style. Then add `UACFCombatBehaviourFragment` pointing at it.
   - Create one with `SaveDataAsset` only if no existing asset fits. Do not duplicate a sample "just in case".
   - `UACFAIControllerFragment` for aggression, lose-target distance, home, teleport, or BT override.
   - `UACFAIStateFragment` for default `AIState.*`, optional patrol spline, optional routine.
5. **Wire spawn:**
   - Spawner / units roster: same `AIClassBP` for every similar unit, unique `AIConfig` per variant, set `AILevel`.
   - Placed in level: same Blueprint, set that instance's `CharacterInitDataAsset` (and level) on `InitializerComp`.

### Fragments to prefer over Blueprint work

| Fragment | Puts on the Data Asset |
|---|---|
| `UACFCombatBehaviourFragment` | Combat behaviour Data Asset |
| `UACFAIControllerFragment` | BT, default state, home, teleport, aggression, hit reaction |
| `UACFAIStateFragment` | Default `AIState.*`, patrol path, routine |
| `UACFAnimsetFragment` | Animset (movesets, overlays) |
| `UACFComponentsFragment` | Extra `UActorComponent` classes at init |
| `UACFDialogueFragment` | Dialogue participant + dialogue assets |
| `UACFExecutionConditionsFragment` | Execution / contextual-anim gating |
| `UACFAudioBankFragment` | Ability sound bank |
| `UACFCharacterCapsuleChannelFragment` | Capsule object channel / profile |

Fragment example for `SaveCharacter` (CombatBehaviour path must be an **existing** asset from `ListAssets`). A non-empty
`Fragments` list replaces the current fragments; use `ExportCharacter` first to keep the ones you want:

```
"Fragments": [
  { "Class": "ACFCombatBehaviourFragment", "Properties": { "CombatBehaviour": "/Game/YourGame/AI/DA_AI_Orc.DA_AI_Orc" } },
  { "Class": "ACFAIControllerFragment", "Properties": { "bOverrideCombatSettings": "true", "bIsAggressive": "true" } }
]
```

---

## 3 — Core Workflow / Usage

```
InitializerComp->InitFromDataAsset(MyCharacterData, Level);   // spawners already do this from AIConfig
InitializerComp->ApplyAppearanceFromDataAsset(MyCharacterData);
UACFCharacterFragment* Frag = MyCharacterData->GetFragmentByClass(UACFCombatBehaviourFragment::StaticClass());
```

Group spawners load `AIConfig` before spawn and apply it on the pawn. You do not need a per-enemy Blueprint for that apply step.

---

## 4 — Wire to Characters / Blueprints

1. Shared AI pawn: `bAutoInit` can stay true; spawners still override with `AIConfig`. For placed actors, set `CharacterInitDataAsset` per instance.
2. Do not copy Event Graph, input, or "BeginPlay grants abilities" onto a new character class — abilities, items, and team come from the Data Asset.
3. Player pawns stay on the `player-characters` variants. Their stats, team, mesh, and items still belong on a `UACFCharacterDataAsset` assigned to `InitializerComp`.
4. Dialogue, executions, and units skills all attach through fragments or `AIConfig`, not new pawn classes.

---

## 5 — Verify

**Checklist:**

- [ ] New content is a `UACFCharacterDataAsset` under `Content/YourGame/`, not a new character Blueprint.
- [ ] Spawn class is an existing AI/NPC `AACFCharacter` (or player variant only if this is the player).
- [ ] `Team` matches a GameState team tag; enemies use a hostile attitude toward the player team.
- [ ] `CharacterRow` / `DefaultAbilitySet` resolve; starting weapons are on the Data Asset.
- [ ] Combat behaviour fragment points at an **existing** `UACFCombatBehaviorDataAsset`; no new one was created unless none fitted.
- [ ] Spawners/units use one `AIClassBP` and a unique `AIConfig` per variant.

**Common failures:**

| Symptom | Fix |
|---|---|
| Agent created a new character Blueprint | Delete it if unused; move mesh/stats/AI onto a Data Asset and point `AIConfig` / `CharacterInitDataAsset` at it |
| Spawned unit has the wrong mesh or stats | `AIConfig` is missing or still the sample Data Asset |
| AI never attacks | Missing `UACFCombatBehaviourFragment` / `UACFAIControllerFragment`, or `Team` is not hostile |
| Placed NPC looks like the shared pawn defaults | Instance `CharacterInitDataAsset` is empty |
| `ChatacterName` "unknown property" | Use the engine spelling `ChatacterName`, not CharacterName |

**Related skills:** `acf-core` (teams, GameState, damage), `ai-framework` (BT, perception, groups, waves),
`player-characters` (player pawn variants only), `units-system`, `configure-dialogue`, `actions-system`.
