# ACF modernization: Phase 0 baseline

## Scope and status

**Audit date:** 2026-10-18. **Baseline commit:** `029c12531a3c75b4fccfc18458c46e154d73d8b1`.

This source-backed baseline refines the preliminary Editor-only modernization plan.
No gameplay source, configuration or assets were changed by this audit. Tests ran
against the currently loaded binaries.

**Completed:** source/dependency/history review, initialization and persistence
inventory, test discovery, eight non-PIE tests and two PIE tests.
**Outstanding:** clean build/cook/package, real save-file round trips, multiplayer,
feature deactivation/reactivation, and performance captures. This is not release
certification or a completed performance baseline.

## 1. Verified environment and history

| Item | Baseline |
|---|---|
| Engine | **UE 5.8.3**, CL **58210709**, compatible CL 55116800 |
| Engine location | `F:/UE_5.8`; confirmed by current Editor log and `Engine/Build/Build.version` |
| Project association | `Pangea_Dawn.uproject`: `5.8` |
| Targets | `BuildSettingsVersion.V7`, `EngineIncludeOrderVersion.Unreal5_8` |
| ACF | Descriptor **45 / 4.5**, declared engine version `5.8.0` |
| Active vendor source | `Plugins/ACFUAsce5ab7c1439afbV5/Source` |
| Initial working tree | Clean |
| Test map | `/GF_PangeaMiningSystem/Mining/Maps/L_MiningTest` |

The startup log confirms project ACF takes priority over its installed engine copy.
Pin these versions while establishing correctness. Installed versions are not a
claim that every dependency is the newest available release.

Repository layout is different from historical submodule assumptions:

- `Content/FullSample` is a gitlink at `c9deeecae5c431f74518d38de2ce52b93fe7e02f`;
  query its history inside that directory.
- The active ACF vendor directory is tracked in the root repository in this checkout.
  `Plugins/ACFUltimate` contains legacy config, not the active plugin source, and
  resolves to the root Git repository. The inspected index query returned no
  `Content/AnimationLibrary` gitlink.
- `029c1253` (2026-10-16): engine/ACF build recovery and integration changes.
- `98030b5f` (2026-10-16): plugin/dependency update; last reported change to the
  active ACF initializer implementation.
- `008afbee` (2026-04-25): mining saving; `33482b27` (2026-04-24): mining V2.
- `git blame` places the dinosaur sample-stat fallback in `b762e385` (2026-04-16).

## 2. Keep/refactor/retire inventory

| Area | Keep | Refactor | Retire only after migration |
|---|---|---|---|
| PangeaCore | Primary definitions, fragments, provider interfaces | Validation and stable identity contract | Nothing yet |
| Dinosaur | ACF character, quadruped movement, super save list, component-receiver lifecycle | Explicit ACF profile initialization, collision ownership | Reflective sample-stat fallback |
| Taming | Definition resolution, ACF inventory/team/actions, guarded results, safe restoration | Authority, ownership, attempt protocol, eligibility results | Class-map and legacy UI fallback configuration |
| Breeding | Farm checks, genetic strategy, max-stat inheritance, inherited Gameplay Effect | Durable state, egg replication, timers and restore hooks | Legacy species configuration after loader/reference audit |
| Base | Requirement/action objects, fragments, player-context quest checks | Replay policy, authoritative commit and failure handling | Stale comments/setup instructions |
| Mining | Authoritative simulation, ACF storage, presentation/UI separation, cost aggregation | Fractional output/time, RPC ownership, engine-private dependency | Redundant fields after proving runtime non-use |
| Hunting | Bounded history, visual pool, ACF aim/crouch and server identification | Single injection owner, discovery policy, spatial lookup | Module fallback injection after supported paths are verified |
| UI | ANS hosts, registry, AUT theme, tag routing, cleanup | Context contracts and production QA | Legacy direct-widget fields after verified migration |

### Dependency map

Arrows mean consumer -> dependency; engine dependencies are abbreviated.

```text
PangeaCore -> Core / CoreUObject / Engine / GameplayTags
PangeaDinosaurAI -> PangeaCore + ACF character/movement/mount/save + ModularGameplay
PangeaTamingSystem -> PangeaCore + ACF actions/inventory/teams/UI
PangeaBreedingSystem -> PangeaCore + ACF GAS/statistics/inventory/UI
PangeaBaseUpgradeSystem -> ACF inventory/quests/save/maps/statistics/UI + ModularGameplay
GF_PangeaMiningSystemRuntime -> PangeaCore + PangeaBaseUpgradeSystem + ACF storage/save
                            + SmartObjects / GameplayInteractions / StateTree
MiningSystemPresentation -> mining runtime + SmartObjects / GameplayInteractions / StateTree
MiningSystemUI -> mining runtime + ACF UI/inventory
MiningSystemEditor -> editor support
GF_PangeaHuntingSystemRuntime -> PangeaCore + PangeaDinosaurAI + ACF movement/stats/UI
Pangea_Dawn -> engine template gameplay + explicit startup load of mining runtime
```

`Source/Pangea_Dawn/Pangea_Dawn.cpp` deliberately loads mining's native type module
before the base feature loads `BUD_Village`, whose inline action uses
`UA_UnlockMiningSite`. Preserve that recovery mechanism until replacing the
asset/type-loading dependency and testing fresh startup/cooked builds. Feature
activation is not module loading.

## 3. Corrections to the preliminary plan

The following safeguards already exist and should not be rebuilt:

- Taming resolves explicit config, then definition fragment, then class fallback.
- Taming rechecks inventory, guards already-tamed results, consumes through ACF,
  and restores state without repeating item consumption or success effects.
- Dinosaur saving preserves ACF's component list before adding feature state.
- Breeding's statistic inheritance reads maximum values, not depleted current health.
- Mining combines duplicate costs and rejects overflow/invalid item entries.
- Feature screens already spawn by ANS tags. Legacy widget-class fields alone do
  not imply an active direct-widget route.
- `UA_GrantInventory` already uses ACF inventory; its generic header comment is stale.
- Mining definition/presentation classes already implement native asset validation.

## 4. Prioritized findings

Priority denotes implementation order, not a claim of a reproduced crash/exploit.
Runtime observations and source-confirmed risks are distinguished below.

### B01 — Competing hunting injection paths: high, runtime observed

Both PIE tests warned that ModularGameplay was replacing an existing named
`PangeaHunterSenseComponent`, suggesting duplicate Game Feature actions.
The hunting module also installs world/spawn handlers and manually creates named
components in `TryAddRuntimeComponents`, while GameFeatureData injects those roles.

**Evidence:** hunting runtime module cpp lines 50-139; inspected AddComponents action;
current test warnings. **Change:** make Game Feature activation the lifecycle owner.
Prove placed/spawned coverage, one component per role, initialization order,
deactivation cleanup and reactivation before removing the module fallback.
Module shutdown is not equivalent to feature deactivation.

### B02 — Missing explicit creature-to-ACF profile contract: high, source confirmed

`PangeaCreatureDefinition` contains species, spawn class, gender defaults and fragments,
but no ACF profile bridge. The inspected Triceratops initializer defaults have no
Character Data Asset assigned. `EnsureDefaultCombatStatRow` instead loads FullSample's
attribute table and assigns `MMEnemy` through reflection. Fixed collision profiles
are also reapplied at startup, requiring reconciliation with ACF profile fragments.

**Evidence:** `Source/PangeaDinosaurAI/Private/Characters/PDDinosaurBase.cpp` lines
44-60, 231-281; core definition header; prior Editor CDO inspection.
**Change:** a narrow profile adapter in the existing ACF-aware dinosaur module;
keep PangeaCore vendor-independent and respect explicit instance/spawner overrides.
The installed initializer clears/grants abilities and initializes stats in
`InternalHandleServerInit`: calling it again after save restoration is not a harmless refresh.

### B03 — Incomplete breeding persistence/replication: high, source confirmed

Gender, fertility/cooldown and `GeneticTraits` lack native `SaveGame` flags; only
`InheritedStatProfile` has one. Gender is randomized when unspecified. The component
enables replication without registering those state fields. Parent snapshots create
a new GUID per call, not a durable creature identity.

The egg's definition is transient; parent snapshots/traits lack save flags;
incubation progress is held in unreflected floats/timers. The cpp registers parent
snapshots/child traits through `DOREPLIFETIME`, but their header properties lack the
matching replication flags. ACF's save archive sets `ArIsSaveGame = true`.

**Evidence:** breeding component h/cpp; `PangeaEggActor.h` lines 28-38, 73-74 and cpp
lines 19-142; active ACF `AscentSaveSystem/Public/ALSSaveTypes.h`.
**Change:** versioned durable instance state, stable identity, generated-once
offspring outcomes, correct replication and explicit save restoration. Add real
round-trip/late-join tests; current safety tests do not prove these behaviors.

### B04 — Base action replay and commit semantics: high, source confirmed

`LoadCompletedMilestones` re-executes all actions without a reconstruction/one-time
classification. `OnLevelIncreased` commits the level before evaluating milestones;
actions do not return transaction results. The upgrade component lacks an authority
guard on its upgrade entry point.

**Evidence:** base `Private/Components/UpgradeSystemComponent.cpp` lines 47-94,
103-119 and `ExecuteMilestonesForLevel`; `Private/Actions/UA_GrantInventory.cpp`.
**Change:** explicit replay policy, command authority, cost policy and failure/commit
semantics. Normal loading passes village context, so inventory grants may simply
fail to resolve a pawn. Duplicate inventory on load was not reproduced; replaying
all action types is the architectural risk.

### B05 — Mining output depends on sync cadence: high, source confirmed

**2026-10-20 verification update:** pause-offline accounting is rebuilt; a save
delta-serialization defect caught by the new tests was corrected with Live Coding.
Focused mining passes 11/11 without warnings; the complete native suite passes
40/40. See [B05 mining timing](acf-mining-timing.md) for patch/build evidence and
remaining release gates. The evidence below records the original baseline.

Production floors fractional output, then sync advances its timestamp even when
no whole unit was produced. At 50 units per 300-second day, repeated one-second
syncs each yield zero, unlike a single 300-second sync. Saved production/shipment
timestamps use world-session seconds; the inspected load handler does not rebase them.

**Evidence:** mining runtime `Private/Components/MiningSiteComponent.Storage.cpp`
lines 131-166, 219-240; `GetWorldSeconds`; site actor `OnLoaded`.
**Change:** preserve fractional progress/accounted time and define gameplay/offline
time policy. Test cadence invariance, capacity, upgrades, new-session clock deltas
and repeated restoration before altering economy values. This is static evidence,
not a measured production benchmark.

### B06 — Engine-private mining dependency: medium, source confirmed

**2026-10-20 verification update:** public movement/context composition replaces
the private task; both private include paths are removed. The user full build
includes the editor-only PropertyBindingUtils linker fix. Following two test-only
fixture corrections, mining passes 16/16 without warnings and the full suite passes
45/45. See [B06 mining public APIs](acf-mining-public-apis.md) for evidence and
remaining navigation/release gates. The following is baseline evidence.

Mining runtime/presentation Build.cs files add GameplayInteractions' `Private`
include directory; runtime uses `AI/AITask_UseGameplayInteraction.h` through it.
**Change:** prefer supported public APIs or isolate a version-pinned adapter.
Removing the include path without replacing its consumers would break compilation.

### B07 — Multiplayer command protocol: high before multiplayer release

Taming state/role are not native replicated fields, and resolution lacks a complete
server attempt protocol. Mining exposes RPCs on world actors, so remote ownership,
interactor validation and acknowledgements need tests. Authority checks alone do
not prove a client can legally invoke a server RPC. No exploit or successful
remote flow was reproduced. **Change:** define player-owned routing and validate
target, distance, authority, ownership and attempt identity.

### B08 — Hunting scale/discovery policy: medium, source confirmed

Hunter sense scans world actors for emitters/clues with a default 0.1-second scan
interval. Identification flags live on replicated emitters/clues rather than a
per-player knowledge record. History is already bounded. **Change:** decide shared
versus individual knowledge and benchmark representative populations before
choosing spatial indexing/optimization. No performance bottleneck was measured.

### B09 — Validation, asset discovery and docs: medium

Native `IsDataValid` overrides were found only in mining definition/presentation
classes within the inspected project source roots. Definition validation checks
presentation assignment/loadability, not full progression/economy invariants.
Inspected Asset Manager settings explicitly register mining GameFeatureData, not
Pangea creature/base types. Hard references may still cook assets; verify packaged
discovery rather than assuming missing registration proves missing content.

The November 2025 Writerside overview omits mining/hunting and uses old class names.
Its dinosaur constructor directly creates feature components, unlike current
injection, and it describes ALS as locomotion rather than Ascent Load & Save.
**Change:** extend existing validation and update setup docs per migrated slice;
never delete assets solely because registry referencers are empty.

## 5. Automation baseline

Discovered **11** Pangea tests; executed two batches through the native Automation
Test toolset: **10 passed, 0 failed in executed tests; one not run**.

| Suffix under `Pangea.ACF.` | Result |
|---|---|
| `Base.QuestsUseOwningController` | Passed |
| `Breeding.FertilityAndInvalidParents` | Passed |
| `Mining.AggregatesInventoryCosts` | Passed |
| `Mining.VillageUnlockShowsEstablishInteraction` | Passed |
| `Save.DinosaurPreservesACFComponents` | Passed |
| `Taming.ConsumesGUIDItemsOnce` | Passed |
| `Taming.InvalidInstigatorAndRoles` | Passed |
| `UI.RegistryContract` | Passed |
| `UI.PagesInPIE` | Passed with warnings |
| `UI.MiningActorRoutesInPIE` | Passed with warnings |
| `UI.VillageToMiningInteractionInPIE` | Not run: expects populated SC_Demo fixtures |

The loaded mining map was preserved. The two PIE tests started/stopped transient
sessions; PIE was inactive before/after. No save slots were intentionally written.
Current session details are in `Saved/Logs/Pangea_Dawn.log`.

Both PIE tests warned about duplicate hunter-sense injection, invalid ActionsManager
character, an unresolved sample quest target, invalid cinematic camera references
and missing RecastNavMesh for the crowd manager. Actor-route fixtures also logged
an unestablished site's chest refresh. This is not a warning-free baseline.

These tests do not prove real breeding/hatching, old-save compatibility, remote
multiplayer, populated gamepad navigation, cooking or production performance.
No source compile was needed for documentation-only changes; no clean build ran.

## 6. First implementation slice: Triceratops profile contract

Implement B02 using Triceratops; preserve the legacy path for unmigrated species.
B01 is a separate focused lifecycle fix, not a hunting rewrite.

1. Add an ACF-profile fragment in `PangeaDinosaurAI`, derived from the existing
   Pangea fragment base and referencing `UACFCharacterDataAsset`. No ACF dependency
   in PangeaCore and no parallel initialization framework.
2. Define precedence: explicit instance/spawner profile beats definition default.
   Expose provenance/conflicts rather than silently overwriting configuration.
3. Resolve the default before ACF consumes it. Test the lifecycle hook against
   installed ACF 4.5: appearance work already begins in `InitializeComponent`.
4. Expose useful Blueprint diagnostics/queries on the owning actor/component;
   keep private helpers private and avoid polling-based readiness.
5. Capture current mesh/materials/capsule/team/stats/animation/abilities, then
   author the pilot profile under `/Game/Yarin`. Reuse the existing dinosaur
   Blueprint and preserve skeleton-specific movement/inherited appearance.
6. Validate missing/duplicate fragments, invalid references and contradictory
   configuration through native asset validation.
7. Preserve save-list behavior, tame restoration and regression tests; never
   indiscriminately reinitialize baseline stats/abilities after loading.
8. Retain the sample fallback with diagnostics for unmigrated content; remove it
   after placed/spawned/hatching paths are migrated.

Acceptance criteria:

- Definition-only defaults initialize the expected profile once; explicit
  instance/spawner overrides remain authoritative.
- Both feature/profile initialization orders work for placed, spawned and
  hatched pilot creatures, without copied species gameplay graphs.
- Save restoration does not reset health, overwrite inherited stats, duplicate
  inventory/grants or lose tame state. Test old saves separately from fresh saves.
- Keep all executed baseline tests green; add profile/validation/order tests.
- New reflected classes/properties require closing the Editor and a full rebuild;
  Live Coding is not the structural-change deployment path.

## 7. Remaining gates

- Run the SC_Demo-specific test without overwriting unsaved map changes.
- Run clean Editor/Game builds and a packaged Development smoke test.
- Add production-cadence and breeding save/restore regression tests; migrate
  archived copies, never the only player save.
- Exercise dedicated-server/two-client play, late join, mounting, command ownership
  and feature deactivation/reactivation.
- Record game-thread cost, actor/component counts, memory/network use for
  representative creatures, active hunting and near/far/unloaded mines. Define
  platform/population budgets before selecting optimization technologies.
- Reconcile sample quest/camera/startup warnings without modifying sample/vendor
  assets merely to silence logs.

Direction: **ACF owns infrastructure; Pangea owns domain rules; definitions are
validated configuration; components own state; UI/presentation observe it.**
Prefer focused adapters and explicit lifecycles over speculative subsystems,
service locators or wholesale rewrites.
