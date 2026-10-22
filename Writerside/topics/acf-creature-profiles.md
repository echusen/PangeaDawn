# Creature profiles: implementation and migration

**B03 follow-up:** [breeding persistence/replication](acf-breeding-persistence.md)
is rebuilt and native Editor regression-verified: **25 passed, 0 failed, 0 skipped**.
The 21-test result below is historical and is superseded by that run.

## Current status — 2026-10-19

**Child/inheritance and hunting lifecycle changes are activated and verified in
a fresh full-build Editor session: 21 passed, 0 failed, 0 skipped.** The child
profile/definition and persistent inherited-target effect are bound. Hunting
activation/deactivation/reactivation checks pass, with no component-replacement
warnings anywhere in the final suite. Six PIE tests still have scene/sample
warnings; this is not warning-free or packaged/multiplayer certification.

**Adult Triceratops pilot activated after the Editor rebuild/restart.**
The reopened Editor exposes the new reflected types. The existing adult Blueprint's
initializer and statistics subobjects use the new subclasses with their original
names. The earlier explicit-input/save follow-up passed 18 tests; the current
21-test run supersedes it and includes actual child inherited-stat restoration.

Created and saved with ACFCharacterToolset, then exported to verify values:
`/Game/Yarin/DA_Character_Triceratops.DA_Character_Triceratops`.
The existing adult definition
`/Game/_Game/Characters/Dinosaurs/Triceratops/CD_Triceratops.CD_Triceratops`
now references it through `Fragments[1]`. Export comparison confirmed that the
existing breeding fragment at `Fragments[0]` retained all settings except the
intentional switch to the corrected inherited-target effect. No explicit initializer
override was assigned to bypass definition resolution. No vendor or sample files
were edited; the Blueprint modification already present on restart was preserved.

## Source contract

- `UPangeaACFProfileFragment` lives in the ACF-aware PangeaDinosaurAI module.
  PangeaCore remains independent of ACF. A hard profile reference participates in
  the definition's normal loading/cooking dependency chain.
- `UPangeaCharacterInitializerComponent` replaces the inherited **Character Initializer**
  subobject while preserving its name. It selects inputs before calling ACF's
  `InitializeComponent`; it does not invoke a second `InitFromDataAsset` pass.
- Selection precedence is **already applied profile > explicit auto-init profile >
  definition default**. CharacterInitLevel is left untouched. A later explicit
  spawner call still follows ACF's existing initialization behavior; this adapter
  does not make arbitrary repeated spawner reinitialization idempotent.
- Early configuration uses the native definition provider, not a Blueprint-only
  interface implementation: Actor::ProcessEvent suppresses script interface
  dispatch while a world is still initializing. For Pangea dinosaurs, configure
  the serialized CreatureDefinition, not a Blueprint graph overriding its getter.
- Blueprint-pure queries on the initializer expose effective profile, provenance
  and completion status. Readiness observes ACF's existing
  `OnInitializationCompleted`; selecting an asset is not completion.
- `UPangeaGASStatisticsComponent` preserves the **Statistic Component** name.
  When a profile owns initialization, it suppresses the statistics component's
  additional BeginPlay auto-init, while retaining the superclass difficulty/ASC/
  health-delegate setup. Unmigrated creatures retain their auto-init setting.
- Dinosaur startup no longer injects the legacy sample row or reapplies fixed
  collision profiles over a configured ACF profile. Legacy behavior remains when
  no profile is configured. Constructor collision defaults still exist.
- No new initialization is called from save restoration. Existing ACF component
  save-list preservation and tame restoration remain in place. ACF currently
  matches saved component records by name, but name preservation alone is not a
  substitute for testing actual old saves after changing component subclasses.

## Validation

Native creature-definition validation checks species identity, loadable concrete
actor class, null/duplicate fragment entries, then validates individual fragments.
Definitions without a profile fragment remain supported.

The profile fragment checks missing/duplicate profiles (including subclasses),
native owner/provider/component compatibility, valid team, finite capsule dimensions,
and attribute-row/curve consistency. A row name without its table is invalid.
An empty row is supported with a warning because shared Blueprint StartingEffects
may initialize stats; the validator cannot establish that those effects yield
correct gameplay values.

## Pilot snapshot and limits

The new profile preserves the inspected adult Blueprint's:

- Display name `ACF Dude`, team `Teams.Dinosaurs`, zero currency and no starting items.
- `DA_Pangea_Triceratops_AS` ability set; `EForwardFacing` rotation mode.
- Capsule radius/half-height approximately **177.74675 cm**.
- Existing skeletal mesh, AnimBP and material overrides; no mesh scale override.

The main mesh has no component tags. The profile deliberately uses
`MeshComponents[0].ComponentTag = None`, which tells installed ACF to use the main
mesh rather than create a second tagged skeletal component.

**Initial pilot activation removed duplicate initialization and changed effective
gameplay values.** The inspected pre-migration
statistics component had `CharacterRow.DataTable=None`, `RowName=MMEnemy` and
`bAutoInitialize=true`. The initial pilot cleared the dangling row name and left
the row empty; its existing StartingEffects remain on the shared Blueprint:
`GE_Pangea_AttributeDependencies`, `GE_Pangea_StatInit`, and `ACF_Regen_GE`.
This preserves the no-table initialization path. Before/after PIE captures of both
placed adults confirmed duplicate permanent effects before activation, and single
effects afterwards. Do not describe this as numerical balance preservation.

| Runtime measurement (both placed adults) | Before | After |
| --- | --- | --- |
| Active AttributeDependencies effects | 2 | 1 |
| Active Regen effects | 2 | 1 |
| Health / MaxHealth | 100 / 100 | 50 / 50 |
| Stamina / MaxStamina | 20 / 20 | 10 / 10 |
| Mana / MaxMana | 40 / 40 | 20 / 20 |
| Equilibrium / MaxEquilibrium | 20 / 20 | 10 / 10 |
| Melee/Ranged/Spell damage (each) | 2 | 1 |
| PhysicalDefense | 10 | 5 |

Both adults still have exactly the level-1 Attack and Flee abilities, the
`Tame.State.Wild` tag and `Teams.Dinosaurs` team. Runtime readback confirmed the
profile in both initializer asset fields, suppressed statistics auto-init, an
empty row, the expected mesh/AnimBP, unit mesh scale and 177.74675 cm capsule.
Material slots still hold the expected eye/body dynamic instances. The subsequent
explicit input-row stage below intentionally retains zero primary attributes,
rather than silently substituting unrelated sample balance.

## Corrected-value input row and resource persistence

The user chose to retain the corrected values, not restore the doubled values.
Created `/Game/Yarin/DT_Pangea_CreatureAttributes.DT_Pangea_CreatureAttributes`
with an `ACFAttributeInits` row named `Triceratops_Adult`, and assigned that row on
`DA_Character_Triceratops`. It explicitly initializes Strength, Endurance,
Intelligence, Constitution and Dexterity to **0**, matching the captured inputs.
This is a primary-input row, **not** a second set of final resource maxima.
The existing live dependency GE/curves still derive health 50, stamina 10, mana 20
and equilibrium 10. Putting those final maxima in the base row as well would
double-count them. Both placed adults' complete attribute snapshots matched the
corrected pilot after row assignment. Species input edits belong in this row;
shared derivation formulas remain curve-driven. Regression tests pin the approved
runtime outputs, including effects and ability counts.

A real spawned-adult save round trip initially failed: damaged health 37 stayed
at the test's mutated health 9 after load. The configured sample serialization
table contained only the five primary attributes. It was duplicated to
`/Game/Yarin/DT_Pangea_SerializableAttributes.DT_Pangea_SerializableAttributes`,
preserving all existing keys and adding **Health, Stamina, Mana and Equilibrium**.
`Config/DefaultPlugins.ini` now selects this project-owned table for
`SerializableAttributes`; `AttributesByCurveRow` is unchanged. This is a global
GAS resource-persistence configuration change, not an adult-only override.
No vendor table was edited. Historical saves without these resource keys keep
their existing startup behavior; no historical health value can be recovered if
it was never serialized.

`AdultBalanceDataContract` checks the named row and five unique primary inputs.
`SpawnedAdultAndSaveRoundTripInPIE` spawns the actual adult class, observes real ACF
completion/default provenance and verifies the approved resource values, two
abilities and exactly two active effects. It uses ACF's actor/component archives
and load callbacks to restore health 37 twice, without new effects/abilities.
It does not write disk slots. No existing `.sav` files were found under Saved;
compatibility with actual old saves, save-slot I/O and fresh-session loading
remain unverified. New test code adds no reflected types; Live Coding is suitable
for these test-only source changes, unlike the initial component-type change.

The initial 15-test regression run covered the four new profile tests and the
existing baseline, including the previously deferred SC_Demo village-to-mining
route. The expanded 18-test run additionally covers authored input data, spawned
adult initialization/current-format save restoration and the hatching visual
smoke path. Non-PIE profile tests have no warnings/errors. PIE tests pass with scene/sample
warnings including duplicate animation slots/hunting component injection, missing
quest targets/attribute curves, invalid ActionsManager/camera references and
navigation/static-attachment warnings. These remain follow-up work, not clean
runtime certification.

The legacy source fallback's FullSample table path does not resolve in this
checkout. Discovery found the table at
`/AscentCombatFramework/Configuration/ACF_SampleAttributesInit_DT` instead.
The old fallback was not silently redirected, since doing so would introduce
unrelated attribute values for unmigrated creatures.

Snapshot of inspected component values and the creation spec:
`Saved/Yarin/TriceratopsProfileSnapshot-20261019.json` (local, ignored by Git).
Definition backup: `Saved/Yarin/CD_Triceratops-BeforeProfile-20261019.json`.
Runtime captures and full test results:
`Saved/Yarin/TriceratopsProfileRuntime-20261019.json` and
`Saved/Yarin/TriceratopsProfileTests-20261019.json` (local, ignored by Git).
The subsequent input-row/save follow-up captures and 18-test results are in
`Saved/Yarin/TriceratopsExplicitStats-20261019.json` and
`Saved/Yarin/PangeaProfileRegression-20261019.json` (local, ignored by Git).
Before the child lifecycle batch, the child definition had no fragments and the
child Blueprint's native `CreatureDefinition` was None. The original `Hatch()`
selected the child class without assigning its definition. That gap is now fixed
through deferred spawn configuration and a separately authored child profile;
the implementation/results below supersede the initial smoke-test limitation.

The earlier `Pangea.ACF.Breeding.SelectsChildAndAppliesVisualsInPIE` passed against the actual
configured egg/child Blueprints. A transient copy of the parent's definition
disables mutation only inside the test. The real `Hatch()` path consumes the egg,
selects the configured offspring class, delivers both generated inherited values
(30 from two equal parents) and transfers the eight configured skin/pattern colors.
The smoke test does not time incubation or assert application of inherited GAS
effects: it emits an explicit warning for the missing child definition/fragment.
Adult breeding fragment export still matches the pre-migration backup.

## Rebuild/activation reference and remaining checks

Steps 1–4 are complete for the adult pilot; step 5 has placed-instance readback
and spawned-adult checks, not full gameplay certification. Step 6 includes a
current-format in-memory ACF save round trip, not historical disk saves. Step 7
now has child profile/inherited-target/visual checks and fresh-child archive
restoration. Timed incubation, actual old saves, packaged builds and multiplayer
remain unverified.

1. Save any work and close the Editor. With the Editor closed, build the Editor
   target from the project's normal IDE/build workflow. PowerShell equivalent:

   ```powershell
   & 'F:\UE_5.8\Engine\Build\BatchFiles\Build.bat' Pangea_DawnEditor Win64 Development '-Project=F:\Game Projects\PangeaDawn\Pangea_Dawn.uproject' -WaitMutex
   ```

2. Reopen the project. Fix any compiler/UHT errors before asset migration. Verify
   that the existing Triceratops Blueprint's initializer is
   `PangeaCharacterInitializerComponent` and its statistics component is
   `PangeaGASStatisticsComponent`, with the original names. If an inherited
   Blueprint archetype retains old types, stop and repair/recompile that asset;
   do not attach a definition that the actor cannot consume.
3. Run the four new tests under `Pangea.ACF.Profile` and the existing Pangea baseline.
   New tests cover selection/explicit overrides, early preparation and completion
   observation, ambiguous defaults and native definition/profile validation.
   The completion test simulates the existing completion delegate; it is **not**
   a complete GAS or save round-trip test.
4. Export/inspect the adult `CD_Triceratops` definition, preserving its breeding
   fragment and all its settings. Append exactly one `PangeaACFProfileFragment`
   pointing to `/Game/Yarin/DA_Character_Triceratops`. Do not set the initializer's
   explicit auto-init slot merely to bypass testing definition resolution.
5. Validate, save and test the adult first: same mesh/materials/AnimBP/capsule,
   one main skeletal mesh, expected team and abilities, initialization provenance,
   inherited stats and live health/stamina. Check both placed and spawned instances.
6. Test explicit instance/spawner overrides, feature/profile arrival order, collision
   behavior, mounting, and save restoration using copies of actual save files.
   Capture before/after attribute values and grant/effect counts; do not infer
   preservation solely from the completion delegate or the profile's exported fields.
7. Capture/author an appropriate child profile if the hatching path is included;
   then verify hatching and inherited material/stat preservation end-to-end.
8. Run clean Editor/Game builds and packaged/multiplayer checks before calling
   the migration complete. The reopened Editor and test pass establish loaded
   Editor behavior, not a clean build or old-save compatibility.

## Rollback boundary

The pilot is active. Remove only the newly appended profile fragment to return the
definition to its previous configuration. Keep its existing breeding fragment.
Do not restore whole directories over unrelated work; native component-type
changes themselves require a rebuild to revert.
To undo only this follow-up, restore the profile's previous empty Character Row,
and restore `SerializableAttributes` in `Config/DefaultPlugins.ini` (and the live
settings object or restart) to the prior sample table. Keep any save files already
written with extra resource keys. Do not overwrite the entire config/Blueprint
or erase test reports over unrelated changes. The new tables can stay unreferenced.

## Activated child and hunting lifecycle batch

### Child spawn ownership

- `PDDefinitionProviderInterface.SetCreatureDefinition` is a Blueprint-callable,
  authority-only spawn contract. The dinosaur accepts a compatible non-null
  definition only before component/actor initialization. It cannot reset a live
  creature. The selected reference is saved and replicated.
- `Hatch()` uses deferred construction, supplies the offspring definition through
  the native provider, then finishes spawning before inherited effects/appearance.
  It does not copy the egg's presentation scale onto the child. A rejected
  definition preserves the egg rather than consume it on that failure.
- The existing child Blueprint is retained. A separate ACF profile was created
  at `/Game/Yarin/DA_Character_Triceratops_Child`, preserving its inspected capsule
  (radius 37.084999 cm, half-height 61.022881 cm), mesh/AnimBP, scale 0.3, team,
  ability set, zero currency and no items. It uses `Triceratops_Child` in the shared
  primary-input table, with the same corrected zero primary inputs. No adult
  capsule was assigned to the child. These assets are now active.
- Appended the profile fragment to `CD_Triceratops_Child`, and set
  the existing child Blueprint's serialized CreatureDefinition to that definition
  for independent/direct spawning. Mother/breeding still selects the child via
  OffspringCreatureDefinition; it does not own the child's appearance configuration.

### Inherited targets and restoration

Inspection found that the old `GE_Pangea_BreedingInheritedStats` is Instant,
targets current Health/Equilibrium and has empty SetByCaller tags. The parent
snapshots, however, collect **maxima**, and the genetics rules combine those
maxima. Treating their results as extra bonuses would inflate offspring stats.

Created and compiled `/Game/Yarin/GE_Pangea_InheritedStatTargets`: Infinite,
Override MaxHealth and MaxEquilibrium, with SetByCaller tags
`RPG.Statistics.Health` and `RPG.Statistics.Equilibrium`. Readback verified these
fields. The old effect asset is untouched. Binding changed only the
adult breeding fragment's effect reference, not its genetics rules or offspring
selection. Adult unbred baseline remains 50/10/20; an inherited target of 30
means max health 30, **not** baseline 50 + 30.

The breeding component now carries its application effect alongside its saved
inherited profile. A juvenile therefore needs no reproduction fragment, live
mother or egg reference to restore its modifier. Existing callers can still use
the definition fragment fallback. Infinite effects are required for safe
idempotent removal/reapplication. Load restores current resource values after
applying inherited maxima, avoiding ACF's normal ratio rescaling against a fresh
actor's baseline. EndPlay unbinds the initialization delegate and removes the
runtime effect. This does not retroactively reconstruct application data missing
from historical saves of formerly unconfigured children.

### B01: single hunting component owner

Removed all manual world/spawn-handler injection from
`FGF_PangeaHuntingSystemRuntimeModule`; module loading no longer enables gameplay.
Epic's existing GameFeatureAction_AddComponents becomes the sole owner.
The saved action now targets emitters at the native
`PDDinosaurBase` family, keeps hunter sense restricted to `BP_Pangea_Player`, retains
server/client coverage and uses **AddUnique | UseAutoGeneratedName (5)** on both
requests. Auto-generated names avoid recycling destroyed named subobjects that
are still awaiting GC on reactivation; uniqueness still prevents live duplicates.
No module fallback expands hunter sense to unrelated ACF NPCs. The lifecycle test
restored the originally active feature state.

Hunter-sense EndPlay destroys its pawn-owned visual components as well as removing
its widget/delegates. Emitter EndPlay destroys only clue actors created by that
emitter. These are transient feature-lifetime objects, not persistent inventory.

### Deployment and verification results

Before/pending data are recorded in:

- `Saved/Yarin/ChildAndHunting-Before-20261019.json`
- `Saved/Yarin/ChildAndHunting-Pending-20261019.json`

The pending JSON is a historical plan, not a script to reapply blindly. After
rebuild, assets were re-exported/checked before incremental mutation. The child
Blueprint was compiled/saved and the hunting feature asset was saved. Final asset
readbacks and all 21 test results are recorded in:

- `Saved/Yarin/ChildAndHunting-Activated-20261019.json`
- `Saved/Yarin/PangeaLifecycleRegression-20261019.json`

The first focused run exposed missing nested SaveGame flags: the component's
profile field was marked SaveGame, but its Values array and StatTag/StatType/Value
members were not. Those existing fields now carry SaveGame flags, and the fresh
child round trip passes. The deferred-setter fixture was corrected to explicitly
initialize its profile component: an unstarted test world does not initialize
components merely because FinishSpawning returned. The production rejection
boundary remains actual component/actor initialization.

An intermediate post-Live-Coding PIE startup crashed with
`Cast of Object /Script/CoreUObject.Default__Object to Actor failed`. The aborted
run has no success claim. A full rebuild/restart followed (DLLs dated 00:50:49–54,
fresh Editor started 00:51:02 local), and the focused test and full suite passed
in that fresh session. Transient reinstancing is a possible cause, not a proven
crash diagnosis. Prefer the full rebuild for reflected metadata changes.

New/strengthened tests executed successfully in the final batch:

- `Profile.DeferredDefinitionIsSpawnOnly`: pre-initialization assignment and late rejection.
- `Breeding.HatchedChildProfileAndPersistenceInPIE`: real child profile/capsule/scale,
  target maxima, no duplicate inherited effects, and restore of damaged health 13
  into a **new** independently spawned child, twice. Also retains color inheritance.
- `Hunting.SingleInjectionDataContract`: one correctly scoped request per role.
- `Hunting.ActivateDeactivateReactivateInPIE`: placed/new receivers, removal on
  deactivation, no injection while inactive and exactly one component on reactivation;
  restores the originally active feature state.

All 21 tests passed. Old disk saves,
incubation timing, dedicated-server/two-client/late-join, feature UI/pool cleanup
under a populated hunt and packaged builds remain separate verification gates.
