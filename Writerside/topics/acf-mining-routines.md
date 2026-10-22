# Mining duties: ACF-led hybrid

## Rebuilt Korro transport prototype accepted locally (2026-10-22)

The user's closed-Editor rebuild was verified: draft fields/broker are loaded,
both nav agents are configured, and SC_Demo's saved prototype link survives the
restart. Only tiers 4/5 of `/Game/Yarin/DA_Pangea_MiningSite_HybridPrototype` now
use `BP_Pangea_KorroDraft` and **450 cm/s** instead of 320. Native read-back
confirmed every other tier field was preserved. Explicit Pangea-only saves
succeeded. The original demo definition still has hybrid disabled.

Initial transport acceptance: **11/12**. Physical delivery/return stalled because
the new multi-agent height matching sent 176cm humanoids onto Korro's navmesh,
while DetourCrowd still used the legacy Default mesh. The project now preserves
the previous radius-based selection with
`bSkipAgentHeightCheckWhenPickingNavData=True`. This matches the old single-agent
selection behavior; it is NOT new humanoid overhead-clearance certification.
Korro's spawn guard still explicitly checks both capsule radius and height.
The property was applied to the engine settings CDO for this session and is
persisted in the project's DefaultEngine.ini, not a vendor or engine file.

Two Pangea function-only fixes compiled successfully through Live Coding:
- Read CDO collision size from its capsule template rather than ACharacter's
  GetSimpleCollision getters, which return zero on default objects. This fixes
  spawn height and makes the larger-agent guard meaningful.
- Reapply configured duty speed only during the routine's own active MoveTo,
  after late ACF locomotion initialization. Combat/commands/holds retain their
  ownership; no global animation-rate workaround.

The copied Korro mesh now refreshes bones when its pose ticks, so near-player
physical animation remains evaluable even outside the camera frustum. Its native
Pangea Korro AnimBP and GlobalAnimRateScale=1 are retained. Added pose sampling
initially exposed a const-correctness compile error in the test; it was fixed
and Live Coding then succeeded. No outstanding reported compile error.

Final results:
- **12/12 transport**, zero errors, 408 warnings, 33.31s.
- **83/83 Pangea.ACF**, zero errors, 1,642 warnings, 139.64s. Includes the transport
  cases and existing animation/legacy multiplayer regressions; not 95 distinct
  tests. Warnings remain in map/dependency/ACF fixture content.
- Physical PIE uses the placed endpoints with isolated transient inventory;
  checks independent Korro movement, animated moving bone poses, handler/animal
  travel speed settings, cart attachment, ALS rematerialization, persisted escort
  loss, unload/return, exact destination grant and actual animal-death cargo loss.
- Vendor guard remains Git-clean, 8,136 authored files read-only. Both previously
  concerning vendor Blueprints are clean in memory. No Save All.

**Prototype limits remain:** the human handler leads on foot; this is not seated
driving. The cart is kinematic, without hitch/wheel physics, full trailer swept
clearance or visual hitch/terrain acceptance. Long/streamed routes, named draft
animal recruitment/ownership and player taming/mount policy need further work.
Dedicated hybrid network/late join, packaged/fresh-process acceptance of this
final configuration and cross-actor save atomicity remain unverified. Existing
network tests cover legacy logistics. Offline stays paused.

Report: `Saved/Yarin/Mining-Korro-Draft-Acceptance-20261022.json`.
The source-only checkpoint below is historical and superseded by this section.

## Korro draft / separate SC_Demo area checkpoint (2026-10-22)

**Partial implementation: normal closed-Editor rebuild required.** The new
reflected draft configuration and broker/job layout have not been compiled or
accepted. Do not interpret the earlier 80/80 or 10/10 batches as towing acceptance.

Saved scene additions (tag `Pangea.Mining.Prototype`):
- `Pangea Prototype - Mining Discovery`: (-7000, 6500, -1285.160156).
- `Pangea Prototype - Settlement Stockpile`: (-4500, 6500, -1235.157715).
- Discovery explicitly links that stockpile, the hybrid profile and hybrid site
  class. The existing discovery, stockpile and Korro transforms are unchanged.
- SC_Demo uses external actors. AssetTools.save_assets(level) did not save these
  new packages, and SceneTools.save_actor failed because they were not yet in the
  asset registry. Recovery used the native **Save Current Level** toolbar button.
  Both new external actor files were confirmed on disk; no Save All was used.

Saved `/Game/Yarin/BP_Pangea_KorroDraft` and `DA_Pangea_KorroDraft` are copies of
the Pangea Korro Blueprint/creature definition, mutually linked. Only the copy's
team changes from Enemies to Neutral, matching the human courier. It retains
Korro's own AnimBP and skeleton. Originals and all ACF vendor files remain intact.

Function-only test selector changes compiled via Live Coding. The subsequent
transport batch passed **9/10**: the new editor-time endpoint nav check failed.
This project uses runtime navigation invokers, so editor-only path queries cannot
accept the new area. The source now moves the assertion to the physical PIE case
and uses the actual placed route with an isolated transient destination inventory.
That revision and all draft changes below are **not yet built or rerun**.

Prepared Pangea-only source/config:
- Manifest v3 snapshots an optional draft animal; v1/v2 manifests stay on foot.
- Korro uses a separate ACF routine job and its own locomotion, with a conditional
  travel-speed override restored on combat/command yield. It never gets the human
  animation agent. The human courier leads on foot and retains loading/unloading.
- The cart attaches behind the animal; no dinosaur is attached/teleported along
  with a human. This is kinematic prototype towing, **not a physics hitch, seated
  driver, wheel animation or trailer-aware swept path planner**.
- Unloading/return is gated on the live animal's arrival. A real ACF
  animal death uses the existing deferred/exactly-once remaining-cargo loss path;
  failed spawn and visibility retirement are not death. Save/load retires both.
- Runtime invokers cover the short test route. Nav config preserves the old
  Default 35/144 agent and adds PangeaKorroDraft 80/220 for Korro's ~72/210 capsule.
  Undersized mesh fallback is refused. Korro alone disables single-navmesh
  DetourCrowd simulation and uses normal path following on its larger mesh.
  Multi-agent impacts on humanoid path selection require broad regression.
- New tests cover draft snapshot/load, invalid animal-without-cart withdrawal,
  physical independent movement/restore and actual animal death. Legacy tests
  explicitly ignore the separately tagged prototype rather than changing their
  intended legacy logistics coverage.

After rebuilding/reopening, configure ONLY tiers 4/5 of
`DA_Pangea_MiningSite_HybridPrototype`: `TransportDraftAnimalClass` =
`/Game/Yarin/BP_Pangea_KorroDraft.BP_Pangea_KorroDraft_C`,
`TransportTravelSpeed` = **450 cm/s** (provisional, up from 320), keeping every
other tier field exactly. These values are **not yet saved into the profile**;
the running Editor does not know the new reflected field. Then compile/save only
the Pangea assets; verify the larger navdata and actual Korro locomotion/tow offset,
run transport tests, animation and broad legacy/network regression. Keep long
routes, streaming ownership, seated driving, full cart clearance, packaged/late
join acceptance and cross-actor save atomicity explicitly pending.

Report: `Saved/Yarin/Mining-Korro-Draft-Checkpoint-20261022.json`.

## Separate hybrid prototype kit accepted (2026-10-21, after user's rebuild)

The user chose **separate Pangea prototype assets**, keeping the current demo and
legacy tests unchanged. Four assets are saved under `/Game/Yarin`:

- `DA_Pangea_MiningSite_HybridPrototype`
- `BP_Pangea_MiningSite_HybridPrototype`
- `BP_Pangea_MiningDiscovery_HybridPrototype`
- `BP_Pangea_MiningStockpile_HybridPrototype`

### Verification and restore fix

The rebuilt convoy/job/storage classes and the Pangea storage subclass on the
stockpile were verified. Initial transport tests: 7 passed, one active-restore
case failed. ALS restores component records BEFORE actor records; the actor's
Serialize hook was clearing the shipment just read by the component. The Pangea
actor hook now resets only when the component is not already awaiting its final
loaded callback. No vendor patch. A regression checks component-before-actor,
repeated restoration and actor-only legacy resets.

Function-only changes compiled successfully with Live Coding. Transport then
passed 9/9; broader `Pangea.ACF` regression passed **80/80**, zero errors,
1,438 map/dependency warnings. After creating the kit, the focused transport
batch passed **10/10**, zero errors, 193 warnings. It includes a saved kit
contract and physical convoy/escort death/ALS restore/unload/return using the
new prototype site class and tier-5 convoy settings. Only transient fixture
production, batch quantity, cooldown and risk are adjusted for deterministic
testing. These are overlapping batches, not 90 distinct cases. Existing network
tests still cover the legacy profile, not dedicated hybrid late-join acceptance.

The kit contract verifies full tier copies against the original, permitting only
cart assignment at tiers 4/5 and two escort slots at tier 5 (the existing tier-5
guard count). Corrected costs, capacities, rates, day length (300 s), unlock
configuration and current travel speeds are preserved. The original definition
still has hybrid disabled and its inspected fields are unchanged. No demo map
was edited and no original asset explicitly saved. Vendor guard: Git-clean,
8,136 authored files read-only; the two previously concerning vendor Blueprints
are not dirty in memory.

### Using the kit

1. In a separate test level with navigable ground, place
   `BP_Pangea_MiningDiscovery_HybridPrototype` and
   `BP_Pangea_MiningStockpile_HybridPrototype`.
2. On the placed discovery actor, set **SettlementResourceActor** to that
   stockpile. The discovery already references the new profile and site class;
   the stockpile has the Pangea exact-acceptance ACF storage adapter.
3. Establish and upgrade the site normally. Actual cargo and the copied shipment
   cooldown (60 s) are required; hybrid does not display an empty cosmetic courier.
   At tier 0 shipping is locked. Recruitment prices/unlock settings are copied,
   not newly completed production balance.

**Remaining limits:** tiers 4/5 have a cosmetic attached cart, not a dinosaur
hitch, rider pose, wheel physics or mount. Travel speed remains the copied
320 cm/s at every tier; faster chariot tuning is not implemented. Offline is
paused. The logical ledger still requires a loaded site; streaming ownership,
route-aware virtual waypoints, cross-actor save atomicity (including reentrant
inventory-callback save requests), dedicated hybrid network/late-join, packaged
and fresh-process kit reload acceptance remain outstanding.

Report: `Saved/Yarin/Mining-Convoy-Restart-Acceptance-20261021.json`.
Earlier source-only checkpoints below are historical, superseded by this section.

## Physical convoy source checkpoint (2026-10-21)

**Normal closed-Editor rebuild required.** New reflected classes/interfaces and
default components have NOT been compiled or tested in the running Editor.
Hybrid remains disabled in authored definitions. Do not enable it on the old
binary or treat the baseline tests below as acceptance of this new source.

- Baseline before edits: the four transport ledger tests passed, zero errors/
  warnings. Walking was also visually confirmed by the user after its preceding
  fix. This turn changes no walking/station animation assets and no vendor files.
- New Pangea convoy broker materializes one ACF courier, configured escort slots
  and optional cosmetic cart near players. It uses the existing Pangea routine
  controller and fresh per-controller task instances. It does not adopt other
  sites' actors or use station-population tags. The legacy cosmetic courier is
  suppressed only when hybrid is explicitly enabled.
- New transport duty component/private ACF task owns complete-path MoveTo tasks,
  yields to ACF combat/commands/external schedules, uses full loading/unloading
  clip durations and calls the ledger only on physical arrival. No teleport or
  instant-delivery fallback on failed navigation. Exit-radius hysteresis checks
  every convoy member; fights/commands prevent dematerialization.
- Manifest v2 snapshots courier/escort/cart classes and speed. Actual escort
  deaths persist by slot for the current manifest; culling/load cannot respawn
  those slots. Courier death loses remaining escrow; inventory-callback deaths
  are deferred until the acceptance count is committed. Restore retires owned
  actors/controllers before reading the saved ledger. No wall-clock catch-up.
- Added Pangea ACF storage subclass exposing Internal_AddItem's actual accepted
  count. Hybrid dispatch requires this destination adapter, not count-delta
  guesses that could duplicate immediately consumed items. The Pangea settlement
  stockpile constructor uses it; inspect/recompile its project Blueprint after
  rebuilding to verify its serialized native component class migrated correctly.
- Virtual travel stops at visibility crossings even during a long tick. Ground
  positions exclude capsule height, avoiding repeated materialization elevation.

### Next acceptance / wiring

1. Close Editor, normal C++ build, reopen; never close it through assistant tools.
2. Verify the four new reflected classes/default components and stockpile storage
   adapter. Compile project Blueprints, saving only explicitly named Pangea assets.
3. Run `Pangea.ACF.Mining.Transport.*`: four baseline cases plus new
   ManifestRosterSnapshot, VisibilityCrossingStopsLongFrame,
   MissingPresentationRetainsEscrow, PhysicalConvoyAndRestoreInPIE.
   The new PIE case uses transient config/destination only; exercises actual ACF
   convoy spawning, escort death, active ALS restore, unload and return.
4. Re-run walking/station, mining lifecycle/restore, then broader ACF regression.
   Only then migrate the authored transport tiers, preserving corrected costs.

**Still not a production chariot/large-world solution:** cart attachment is a
cosmetic prototype (no dinosaur, hitch, rider animation or wheel physics), escorts
follow the courier rather than a finished formation. Offline remains paused.
The logical ledger still lives on a loaded site actor; World Partition ownership,
route-aware virtual waypoints, full cross-actor save atomicity (including saves
requested from inventory callbacks), multiplayer/late-join and packaged acceptance
remain outstanding. No hybrid assets were enabled or balance values changed.

## Walking assignment regression fix (2026-10-21)

After reload, all three project role mesh defaults referenced the original
ACF_SimpleCombat_ABP again. The Pangea walking blendspace itself still had the
correct sample rows/rates and passed its coverage test. Restoring only a transient
mesh CDO assignment was not sufficient protection against this regression.

- Worker, guard and courier project Blueprints now explicitly select
  `/Game/Yarin/ABP_Pangea_MiningLocomotion` in their own construction scripts,
  after the unchanged parent construction call. Saved only these three Pangea
  packages; no vendor Blueprint, animation or source edits.
- Extended the actual PIE animation test to assert the spawned AnimBP class,
  linked Pangea moveset and live walking blendspace, zero idle contribution at
  cruise, and unit mesh playback/actor time scales. Changing non-reference poses
  alone did not detect use of the old sample locomotion setup.
- Live Coding succeeded for the editor test function changes. Three focused
  tests passed twice, including after explicitly recompiling and saving all
  three role Blueprints: WalkBlendspaceCoverage, StationAnimationCopies and
  RoleLocomotionAndStationAnimationInPIE. Final run: 3/3, zero errors, 95 map/
  dependency warnings. Actual mining/retrieval/deposit states/clips were observed.
- Role navigation speeds, station clips and global playback rates were not
  changed. Vendor guard remains Git-clean, 8,136 authored files read-only.
- This is not a fresh-process reload or subjective visual art acceptance, nor
  full-suite/transport acceptance. Report:
  `Saved/Yarin/Mining-Walking-Regression-Fix-20261021.json`.

## Station animation copies and hybrid transport foundation (2026-10-21, after reload)

**Gate: normal closed-Editor C++ rebuild, then reopen.** The new animation
interface/state/properties and transport class below are source-only, NOT Live
Coding compiled or runtime accepted. Hybrid is deliberately NOT enabled in any
authored site definition yet. The physical actor broker is still pending.

### Verified on the user's reloaded build

- Workforce + walking data checks: **7/7 pass**. Attendance initially failed in
  its uninitialized EditorPreview fixture, where PostInitializeComponents had
  not registered the controller. Explicit fixture-only World.AddController and
  possession assertion corrected that; runtime attendance code was unchanged.
- Created actual AnimSequence COPIES, not children:
  `/Game/Yarin/AS_Pangea_MiningStrike` (two-handed strike prototype, 2.5 s at
  RateScale 0.8), `/Game/Yarin/AS_Pangea_MinerRetrieve` (kneeling pickup,
  1.933 s), `/Game/Yarin/AS_Pangea_MinerDeposit` (table reach, 0.833 s).
  All copies have root motion disabled and forced root lock. StationAnimationCopies
  passed skeleton compatibility and zero-notify checks before the new duration
  assertions were added. Strike uses the existing compatible UEFN skeleton;
  retrieval/deposit use ACF_UE5Manny. Original skeletons/clips are unchanged.
- These are adapted prototype motions, not newly generated AI motion or final
  art acceptance. No mining/pickaxe clip or pickaxe prop was found. In-world
  review must determine whether Control Rig corrections and a tool prop are
  needed. The user selected adaptation first, fresh keyframes only if unsuitable.
- Saved `/Game/Yarin/BP_Pangea_MiningChariot`, a copy of the Oppidam cart
  composition. Its root was empty in the source Blueprint; the Pangea copy now
  has SM_Oppi_DinoCart_AA, retaining separate door/wheels. All mesh components
  are movable; actor/movement replication is enabled. This is a cart prototype,
  NOT yet a dinosaur-driven vehicle, tested hitch or working escort formation.

### New animation source, pending rebuild

- Append-only Retrieving enum value preserves existing serialized state values.
- Work/guard loops remain loops; pickup/deposit/loading/unloading select distinct
  one-shot copies. An interface duration query lets the ACF duty finish a whole
  clip without a reverse module dependency. Primary work uses max(configured
  duration, clip cycle). Retrieval retains a running station claim; combat,
  cancellation and teardown still release owned operations. No animation grants
  resources. Focus-only replication cannot replay a completed one-shot.
- Added active-clip diagnostics and expanded the existing PIE animation case to
  require the actual retrieval and deposit states/assets. These expanded checks
  have NOT been run. Original motion/station acceptance is not proof of this
  new three-phase implementation.

### Hybrid policy selected by the user

Simulate travel offscreen; use physical ACF couriers/chariots/escorts near players.
Offline progress stays paused. New UPangeaMiningTransportComponent is a separate
logistics ledger on the Pangea site actor, included in ALS component saves.
It reserves cargo once, retains a manifest GUID/route/position, and excludes the
legacy instant-grant path when hybrid is enabled. Existing authored definitions
stay legacy because bUseHybridTransport defaults false until migration.

Prepared backend APIs support virtual outbound/return travel, explicit physical
courier binding/unbinding, validated shipment IDs and endpoint arrival after
unloading, actual ACF courier death, partial destination acceptance with retained
cargo, and saved progress without wall-clock catch-up. The actor load hook resets
the ledger before restoring components, including old records missing the new
component. Unsupported/corrupt records pause rather than dropping unknown cargo.

The proposed risk implementation uses one saved server-only random sample per
shipment. Hidden risk scales with virtual exposure; wholly visible danger uses
actual ACF combat. This refinement must be reviewed when enabling/migrating the
authored definitions, not treated as an unchanged balancing guarantee.

**Not finished/enabled:** physical materialization/nav-to-manifest broker,
dinosaur selection/hitch/mount configuration, L4 faster-transport data, L5 escort
AI/formation data, corpse/relevance handoff, manifest UI and global handling of
World Partition unloaded sites. Current virtual stepping requires a loaded site
actor. Near an observer it fails closed without a bound physical courier rather
than delivering invisibly. Four source-only transport tests cover escrow/arrival,
destination rejection, saved offline pause and that materialization safety gate.

After rebuild: run the four Transport cases, expanded StationAnimationCopies and
RoleLocomotionAndStationAnimationInPIE, timing/workforce/ALS regressions; fix all
build/test errors. Then finish the physical broker and species/escort wiring,
test actual attacks/culling and enable migration explicitly. Do not call the
whole transport/chariot/escort brief complete at this checkpoint.

Vendor authored files remain Git-clean and all 8,136 protected files read-only.
At the final memory check both ACFDeathActionBP and ACF_SimpleTemplate_ABP were
NOT dirty. No vendor package was saved. See
`Saved/Yarin/Mining-Animations-Transport-Checkpoint-20261021.json`.

## Walking and day-end workforce slice (2026-10-21)

**Current gate: close the Editor, perform a normal C++ rebuild, then reopen.**
Walking assets are saved and tested. Workforce source adds reflected fields,
functions and delegates and has NOT been compiled or accepted in this session.
Do not run the new workforce tests or Live Coding this reflected change.

### Saved and verified walking

- Role meshes use `/Game/Yarin/ABP_Pangea_MiningLocomotion`, its unarmed layer
  `/Game/Yarin/ABP_Pangea_MiningUnarmedMoveset` and
  `/Game/Yarin/BS_Pangea_MiningLocomotion`. These are project-owned data copies;
  no vendor base graph/source clip was edited.
- Original locomotion had idle/jog but no walk row. At 220/250/320 cm/s it
  blended idle with weights 0.587/0.531/0.400. Added directional walk rows at
  normalized 0.2/0.4 with rates calibrated to source root-track velocities.
  ACF normalizes against the 800 cm/s character maximum, not role MaxWalkSpeed.
- Use triangulation, 30 speed divisions and disabled speed-grid snapping.
  Rebuilt/resampled through the asset editor after property edits: SampleData
  alone initially left a stale runtime cache. Final cruising samples at all
  three speeds have **zero idle weight and 1.0 walk weight**.
- Travel speeds stay 220/250/320, GlobalAnimRateScale stays 1, station montages
  are unchanged, and main root-motion mode stays RootMotionFromMontagesOnly.
- Final loaded-code acceptance: **2/2 pass**, zero errors, **95 warnings**,
  including actual all-role PIE travel poses and station montages. Report:
  `Saved/Yarin/Mining-Walking-Final-Acceptance-20261021.json`.
  This is not visual art approval or full-suite certification. The additional
  source root-speed assertion now in the test also awaits normal rebuild.

### Workforce prepared in source, not yet compiled/tested

The user chose **independent rolls for each living miner at mining-day end,
only while no player is nearby**. The period is no longer an unanswered question.
Default attendance radius is editable **5,000 cm / 50 m**; any possessed player
pawn counts. Attendance is sampled when the due boundary is accounted, normally
every game-time second on authority. Explicit batched simulation uses current
attendance, not reconstructed historical player positions.

- One existing clock owns production and day boundaries. Finishing-day output
  precedes loss; following segments scale by living/configured miner count.
  Full storage does not pause risk. No-workforce legacy passive output retains
  its previous semantics. The new accounting tick does NOT dispatch shipments.
- Save/replicate vacancies, partial online day and completed-day count. Save
  the server-only risk seed; rolls use seed + day. ALS load rebases the world
  clock: **offline time stays paused**. Legacy records start a full crew/fresh
  online day, and unsupported future versions fail closed. Saving settles
  already-due online losses, but does not send shipments.
- Existing vacancies survive upgrades, culling and load; only newly added
  capacity fills on upgrade. Real ACF OnDeath records loss; presentation
  OnDestroyed/culling does not. Day-end visual victims use ACF KillCharacter
  with double-accounting protection and uniform victim selection.
- Per-level WorkerRecruitmentCost and UI use existing ACF inventory payment and
  the **pawn-owned ACF interaction RPC**, not an RPC on the unowned site. Server
  checks possessed player, 600 cm interaction range, vacancy and payment. Settle
  old staffing before hiring. **Empty cost disables recruitment**, not free
  workers. Prices need a design choice and asset wiring AFTER rebuild. Existing
  corrected upgrade costs were not changed.
- This is persistent population/vacancy accounting, NOT named/GUID-backed worker
  roster persistence. Six new Workforce tests cover boundary ordering/full
  storage, attendance, independent rolls/cadence, save/load/offline pause,
  paid replacement/upgrades and save/replication flags. They are NOT run yet.
- Unattended explicit simulation over 10,000 days per call is rejected rather
  than hanging the game thread. Passive/attended intervals use bounded O(1)
  accounting. This does not limit ordinary continuously accounted online days.

### After rebuild

1. Inspect effective authored site-component tick flags. Run Workforce.*, walking,
   existing timing, ALS and routine tests. Fix any compiler/test failures first.
2. Verify real-worker combat death versus culling, and remote-player recruitment
   with no double charge/loss. Configure explicit replacement prices.
3. Rerun the expanded suite twice. Historical 63/63 is NOT acceptance of this
   source. Vendor files must remain Git-clean/read-only.
4. Remaining mining completion: guard persistence/recruitment, authoritative
   physical cargo/arrival/failure policy, L4 chariot/L5 escort, prerequisite data
   migration, packaged/new-session persistence and late join. Not implemented
   by this slice; do not claim the whole mining brief is complete.

## Current mining completion slice — cleanup, restore and fixture isolation (2026-10-20)

The user requested finishing mining, leaving the slow-looking walking animation
until last. **No walking play rate, travel speed, station animation or economy
value was changed in this slice.**

- Reproduced a destructive ownership defect: reloading one site removed another
  nearby site's same-class chest/worker and unowned scenery/pawns. The new
  `RestoreCleanupRespectsSiteOwnership` failed six checks before the fix.
  Cleanup now requires explicit tracking, exact site tags or actual ownership,
  never proximity/class alone. A missing chest class cannot delete unrelated
  site-owned actors. The fixed regression passes.
- Added `ActiveDutyALSRoundTripInPIE`: real ALS in-memory actor/component records
  while a worker occupies a station, non-empty inventory changed after saving,
  repeated restore, exact role counts, retired jobs/chest and resumed occupied
  work. No user save slots or map files are written. This does not certify a
  packaged/new-session world save or late joining.
- That test exposed delayed animation initialization on just-created NPCs being
  destroyed by a second restore in the same frame. Mesh unregistration alone
  was insufficient. The stock Simple template uses `DelayUntilNextTick`;
  Pangea now cancels the retiring anim instances' latent actions and unregisters
  their meshes while the owner is valid, then destroys the owned actor. Active
  restore now passes without expected-error filters or editing vendor graphs.
- The earlier animation timeout recorded a dead observer; the project controller
  opens a pausing death menu. The animation-only fixture now protects/pins its
  observer before warmup, and explicitly requires a live observer/unpaused world.
  NPC health, task execution and moving-pose/station-montage assertions remain.
  Actual project-player multiplayer initialization remains unmodified by fixtures.
- Created/compiled/saved `/Game/Yarin/BP_Pangea_MiningRoutineController` from the
  stock routine controller and selected it in the project presentation config.
  Export comparison confirmed every other presentation setting unchanged.

Function-only changes and native tests compiled successfully through approved
Live Coding (latest 21:48:26 UTC). No new reflected layout was added. Include
these source changes in the next normal C++ build before relying on a restart.
Focused restore/ownership/isolation checks pass. The expanded full suite passed
**63/63 twice consecutively**, 0 failed (99.64 s and 102.53 s, **1,219 warnings
each**). Both runs include 30 mining cases and the previously intermittent
animation case in normal suite order. They are not warning-free or release
certification. Reports: `Saved/Yarin/Mining-Completion-Regression{1,2}-20261020.json`;
handoff: `Saved/Yarin/Mining-Completion-Checkpoint-20261020.json`.
Vendor protection checks remain clean/read-only for all 8,136 authored files.

Mining is **not feature-complete**: persistent workforce/death/recruitment,
unattended-risk semantics, physical cargo/vehicle/escort and the release gates
below remain. The user has been asked whether worker-loss percentages apply per
in-game day, per shipment, or only through actual combat. No default period or
recruitment price has been silently chosen; pause-offline remains unchanged.

## Previous vendor-migration verification (2026-10-20)

**Vendor ACF source and assets are READ ONLY**, enforced by root `AGENTS.md`.
The five preceding vendor source edits were backed up and restored to HEAD.
The user also requested restoring `ACFTask_GoToCampFire.uasset`; that rollback
is complete, with its previous binary backed up under `Saved/Yarin`.
The entire `Plugins/ACFUAsce5ab7c1439afbV5` working tree is clean.
The final sample rollback completed at **23:18:39 local** through the user's
external PowerShell run. Full and Ultimate player controllers and the skill-info
widget were backed up and restored to HEAD in `Content/FullSample`; that
submodule is now Git-clean as well. The script checked Pangea asset SHA-256 hashes
before/after: **project assets are unchanged**.

**Filesystem protection is now applied and independently rechecked:** all
**8,136 authored vendor files** are read-only (3,194 in the ACF plugin, one in
`Plugins/ACFUltimate`, 4,941 in `Content/FullSample`). Build/cache directories and
Git metadata are deliberately excluded so stock vendor modules can still build.
`Content/AnimationLibrary` is absent in this checkout; the guard covers it if added.
Run `Scripts/Protect-PangeaVendor.ps1 -Action Check` before and after work; never
clear attributes for a customization. New ACF-derived assets must be Pangea-named
copies/children in `/Game/Yarin`, never edits to originals. The protection report
is `Saved/Yarin/ACF-VendorProtection-Status.json`.

Fresh Editor PID 20880 opened after rollback/protection. The new full-suite run
passed **60/61** (130.72 s, **1,097 warnings**); see current acceptance below.

The user completed the closed-Editor normal rebuild. Fresh Editor PID 6664
loaded the new Pangea classes without the earlier vendor Live Coding patches.
Project-only asset wiring is now compiled and saved:

- `Actions.Hit` in `/Game/_Game/Characters/Player/DA_Pangea_AS_Player` uses
  `UPangeaAdvancedHitAction`, retaining montage, sync point and ability level.
  Export comparison confirmed **every other ability entry unchanged**.
- `BP_Pangea_Player` owns `UPangeaCharacterSetupComponent`, which supplies a
  transient constant death-reward curve through ACF's public setter, retaining
  fixed XP, leveling mode and authored curve overrides on the existing GAS stats.
- `/Game/Yarin/BP_Pangea_FullPlayerControllerBase` and
  `/Game/Yarin/BP_Pangea_UltimatePlayerControllerBase` replace the sample
  composition through the existing Pangea bridge. The Full copy owns one
  `UPangeaItemsManagerComponent`; copied parent calls are retargeted.
  The supported instance-level preview adapter handles local-only deferred
  spawning, async loading, possession/destruction retries and cleanup.
  No vendor template, function table or global spawn hook is changed.
- All effective preview settings, including stage, render target, transform
  (99999, 99999, 99999) and customized lighting, are retained.
- Project-owned `WBP_Pangea_SkillInfo`, `WBP_Pangea_SkillTrees` and
  `WBP_Pangea_SkillTreesNavigation` live in `/Game/Yarin`. The skill-info
  controller cast and member calls target the new controller hierarchy.
  Nested widget replacements compile; no referenced members were unmatched.
  Only the SkillTree class mapping changes in `DA_PangeaWidgetRegistry`.
  Vendor/sample widgets remain untouched.
- `UPangeaAdvancedHitAction` uses supported `UACFActionAbility` because the
  vendor advanced action's constructor is private. Its safe-direction helper
  is Blueprint-exposed. Self/coincident/missing hits preserve finite transforms.

**Current acceptance after complete stock-sample restoration:** **60/61
`Pangea.ACF` tests pass** (130.72 s, **1,097 warnings**, one failure).
`Mining.RoleLocomotionAndStationAnimationInPIE` timed out with travel W/G/C=1/1/1,
station=0/0/0. Movement poses were detected for all roles, but this run did not
observe their station animations. Do not certify the full suite as passing.
The other 60 cases, including actual project-player multiplayer preview safety,
replication metadata, UI and persistence, passed. No vendor changes were made to
work around this failure. The animation fixture keeps its observer nearby;
independent distance-culling coverage remains active.

An isolated recheck of the same animation test then passed in **14.50 s** without
any code or asset changes. Batch and isolated observations differ; the cause is
not established. This does **not** turn the full-suite result into 61/61.
Recheck report: `Saved/Yarin/Pangea-VendorProtectedAnimation-Recheck-20261020.json`.

The earlier 4/4 focused and 61/61 full suite (94.71 s, 1,018 warnings) passed
before the final sample rollback and is retained as historical evidence only.

Full results and status are saved in
`Saved/Yarin/Pangea-VendorProtectedRegression-20261020.json`; the earlier run is
`Saved/Yarin/Pangea-ProjectOwnedMigration-Verified-20261020.json`.
Earlier 60/60 vendor-patched reports below are historical only.
Corrected economy and pause-offline policy are unchanged.
Dedicated server, late join/relevance, latency/loss, active-duty save/load,
actual death/recruitment, additional combat/path cases, multi-site isolation
and packaging remain release gates. A general skill dialogue still references
the sample skill-tree-opening action; migrating that unrelated sample dialogue
flow is separate from the project registry route verified here.

## Historical vendor-patched verification — superseded (2026-10-20)

The restarted Editor loaded the normal build written at 18:10–18:11 local.
Its first project-player listen-server test reproduced the NaN rotation error;
this was not merely the older replication-index screenshot. The engine's own
stack traced it through `AACFCharacter::Landed` / self-caused fall damage into
`UACFAdvancedHitAction::GetWarpTransform_Implementation`. The shared relative
direction helper used `GetUnsafeNormal()` on a zero vector when source == target.

### Source changes and focused acceptance

- Relative direction now uses safe normalization. Hit warps preserve facing for
  self/coincident hits and guard missing owners/targets. Ordinary hit displacement
  and facing are retained. `Pangea.ACF.Player.SelfHitWarpFinite` failed before the
  fix on the zero-vector direction, then passed with actual native warp checks
  for self, overlapping, missing and ordinary attackers.
- The death-XP curve is explicitly an optional override. Without it, a levelable
  character uses its configured fixed reward; the player's **30 XP**, leveling
  mode and next-level curve are unchanged. `OptionalDeathRewardCurve` checks the
  fixed fallback, authored override and non-leveling behavior without error filters.
- Item preview stages/copies are local-only, transient and nonreplicating. Copies
  cannot possess, fall, collide or take damage; movement is disabled before and
  after construction. Remote server controllers cannot create preview copies.
  Requests before possession no longer strand an empty stage permanently.
  A preview replication assertion failed before this change and now passes on
  the host and remote player's actual project classes.
- `ProjectPlayerRemotePresentationInPIE` retains the project's GameMode,
  GameState, player and controller initialization. It checks local preview safety
  and the real replicated mining roles/duties. Diagnostic capture preserves
  original errors and catches repeated hit-warp stack warnings: Unreal logs the
  NaN error itself only once per process, so warm passes without that guard were
  insufficient evidence. No expected-error suppression was added.

The focused three-case run passed **3/3** (203 existing scene warnings). The
final full `Pangea.ACF` run passed **60/60**, 0 failed/skipped, **996 warnings
across 12 tests**, in 91.57 seconds. All **28 mining cases** passed as a subset;
this is not an independent focused-mining suite or a warning-free run.

### Animation fixture correction and boundaries

Two earlier full runs were **59/60**: the animation-only case timed out despite
an isolated recheck passing. A minimal remote-then-animation pair reproduced it.
Timeout diagnostics found the observer back over 70 metres from the site, alive,
with no remaining site agents: normal relevance culling had removed the subjects.
The reason for that player displacement was not traced. The animation-only
fixture now cancels the transient observer's actions, disables its movement and
holds it near the site. NPC travel, evaluated-pose and station-montage assertions
are unchanged; NPCs are not teleported during sampling. The same ordered pair
then passed **2/2**, followed by the full **60/60** run. Separate lifecycle/culling
and project-player network tests are not pinned and still passed.

Live Coding finished successfully at **16:41:16 UTC**, with compilation inactive
afterward. No reflected layout was added. Include these source changes in the
next normal C++ build; a fresh-process retest of that build remains a separate
gate. This certifies the tested listen-server slice, not dedicated servers,
late join/relevance re-entry, latency/loss, packaging or full player lifecycle.
Scene warnings (GAS curve-table references, animation slots, preview RPCs and
sample content) still need a separately scoped cleanup.

PIE was inactive after verification. The player Blueprint and five inspected
mining assets were clean; their on-disk hashes were unchanged by this work.
No content asset was edited or saved. Corrected economy and pause-offline policy
remain unchanged. Reports are under `Saved/Yarin/PlayerStartup-*20261020.*`,
including cold baseline, engine stack, failing geometry baseline, observer
diagnostics, ordered-pair checks and `RegressionVerified-20261020.json`.

## Earlier status — isolated remote fixture before startup fixes (2026-10-20)

The user completed the reflected-metadata rebuild. Hunting runtime and mining
Editor DLLs were written at 17:34:47 and 17:35:00 local, before Editor PID 32316
started at 17:35:06. Both previously missing replication flags are present.

An isolated metadata fixture initially queried lifetime properties without the
network driver's prerequisite `SetUpRuntimeReplicationData()`. That can leave
FProperty replication indices uninitialized and collide with inherited push-based
entries. Both new metadata tests now perform that setup first. This is an
Editor-test correction, not another reflected/runtime change. Live Coding
succeeded at 15:38:34 UTC. The two metadata cases plus presentation snapshot
contract then passed **3/3 with no warnings**.

### Verified remote scope and fixture boundaries

`Pangea.ACF.Mining.RemotePresentationInPIE` passed directly in **20.21 seconds**
(139 warnings), then passed again in the full regression. It runs an actual
listen-server world and a connected remote-client world in one Editor process,
using the real SC_Demo mining site/role Blueprints, Smart Objects, ACF routines
and generated navigation. It verifies:

- One replicated native presentation component per selected remote role pawn.
- Moving locomotion and station montages for worker, guard and courier.
- Client-side role/state/focus setters cannot author the replicated state.
- AI controllers and routine jobs remain server-owned, not client execution.
- Destroying the server site removes the selected replicated role actors remotely.

The first attempt with project player startup reached the mining assertions but
failed on an earlier NaN rotation and `Invalid ExpToGiveOnDeathByCurrentLevel`
curve error. These are **unresolved project-player multiplayer startup issues**;
they were neither suppressed nor fixed by changing the fixture. A first isolated
fixture also lacked ACF team/collision and navigation-invoker prerequisites and
failed; those fixture omissions were corrected rather than hiding their logs.

The passing fixture uses native `AACFGameMode`, native default player pawns,
`AACFGameState` and the existing `/Game/_Game/DA_PangeaTeamConfig` on both peers.
It configures the GameState before character initialization, registers explicit
navigation invokers and waits for generated tiles before arranging travel.
Only transient PIE instances are configured. Scoped world/spawn hooks are removed
on fixture teardown; copied per-session play settings do not edit user defaults.
The final fixture Live Coding compile succeeded at 15:44:11 UTC. This certifies
the mining presentation slice, **not the original project's full multiplayer
player startup or shipping multiplayer**.

### Regression and remaining gates

The final untouched full `Pangea.ACF` regression passed **57/57**, 0 failed and
0 skipped, with **829 warnings across 11 tests**. All **27 mining cases** passed
as a subset (508 warnings); no independent focused mining suite was run here.
The locomotion-pose, navigation and lifecycle cases passed again. This is not a
warning-free run. PIE was inactive afterward, and all five inspected mining
definition/config/role assets were saved/clean with unchanged file hashes.

An initial full attempt recorded 55/57: concurrent Editor asset inspections were
invalid during PIE and emitted errors into the remote test; the pose case also
timed out. Its cause was not proven. The pose case passed a separate recheck,
then all tests passed in the untouched full repeat. Do not count the contaminated
attempt as a clean pass or make Editor-only asset queries while PIE tests run.

Only test function/fixture code changed in this verification step. Include it in
the next normal build; there is no new reflected layout requiring an immediate
additional rebuild. Reports: `Saved/Yarin/Mining-Network-{Metadata-Verified,
AfterMetadataFix,IsolatedPlayers,ACFFixture,RegressionInitial,Regression,Verified}-20261020.json`
and `Mining-Network-Verification-20261020.log`.

**Next priority:** resolve the original project-player multiplayer NaN/XP-curve
startup errors. Dedicated server, late join/relevance re-entry, latency/loss,
packaging, active-duty persistence, actual death/roster/recruitment, nearby-site
isolation and custom actor-level presentation overrides remain separate gates.
Corrected economy and pause-offline policy are unchanged.

## Earlier network interruption and metadata correction (2026-10-20)

The user completed the normal rebuild. Mining presentation and Editor DLLs were
written at 17:26:29 and 17:26:35 local, respectively. The new replication and
remote-PIE tests were discovered. The network attempt in Editor PID 3080 then hit
a **scene-wide fatal replication declaration mismatch before acceptance**:
`UPangeaTrackEmitterComponent::GetLifetimeReplicatedProps` registers
`HuntSpeciesConfig`, but its UPROPERTY lacked `Replicated`. The two debugger
screenshots show the original fatal and Windows fatal-error handling for that
same error; they do not establish two independent mining defects. Neither the
interrupted run nor the aborted earlier request is counted as a passing suite.
The isolated `PresentationReplicationContract` case did finish successfully at
15:29:28 UTC before the remote-PIE case started; that is not a remote-network pass.

A targeted source scan found the same mismatch for mining discovery's
`AMiningDiscoveryNodeActor::EstablishedSite`. Both declarations now include
`Replicated` while retaining their SaveGame/editability settings and existing
lifetime registrations. The repeat source scan matched 45 project declarations
under Source and GameFeatures and found no further mismatches among those matches;
this is a targeted static check, not full network certification.

Added native metadata/lifetime-registration cases (then pending):
`Pangea.ACF.Hunting.ConfigurationReplicationContract` and
`Pangea.ACF.Mining.DiscoveryReferenceReplicationContract`. Both check CPF_Net
before entering the otherwise-fatal registration path. The remote-PIE test also
preflights these two scene contracts before launching the session.

At that stage, a closed-Editor normal rebuild was required to apply the two
reflected flags. It has now been completed; the current section above records
the corrected tests and subsequent scoped acceptance rather than leaving this
historical staging instruction as the current status.
The fatal excerpt is `Saved/Yarin/Mining-Network-Interrupted-20261020.log`.

### Verified locomotion correction

All three existing project role Blueprints (`BP_MiningWorkerActor_Test`,
`BP_MiningGuardActor_Test`, `BP_MiningCourierActor_Test`, under
`/Game/_Game/MiningSystem`) had their mesh Anim Class set to the bare
`/AscentCombatFramework/CharacterController/Simple/ACF_SimpleTemplate_ABP`.
Inspection confirmed its `MovesetLayers` is empty. Station montages could play,
but there was no configured base locomotion layer during travel.

Changed only those three mesh Anim Class assignments to the existing configured
`/Game/FullSample/Blueprints/SimpleCombat/ACF_SimpleCombat_ABP`, then compiled,
saved and read back the Blueprints. The child and SKM_Manny use the same
`ACF_UE5Manny` skeleton. No shared template/sample or animation graph was edited.

The new native Editor case
`Pangea.ACF.Mining.RoleLocomotionAndStationAnimationInPIE` passed in 11.07 seconds
(130 warnings). It samples actual evaluated local-bone poses during path-followed
travel for worker, guard and courier: an active ACF moveset, movement-driven anim
speed, at least six non-reference bone rotations and three changing rotations
across moving samples are required. All roles also reach their station state with
an active montage. This is pose/navigation verification, not merely checking that
an Anim Class field is non-null or a montage starts.

Live Coding of the Editor test succeeded at 15:17:06 UTC in Editor PID 26172.
The subsequent full suite passed **53/53**, 0 failed, 0 skipped, with **643 warnings**.
This result applies to the animation fix and previous runtime, **not** the staged
replication source below. No independent focused mining suite was run in this step.
PIE was inactive afterward. Reports: `Saved/Yarin/Mining-Animation-20261020.json`
and `Saved/Yarin/Mining-Animation-Regression-20261020.json`.

### Presentation replication implementation

The native `UMiningPresentationAgentComponent` now has source for a replicated
role/state/focus snapshot with one RepNotify. Dynamic components replicate from
the server; clients reconstruct cosmetic state and start/stop their own owned
station montage. Movement still comes from CharacterMovement replication; clients
do not steer duty movement, run routine jobs, reserve stations or award resources.
Public presentation setters reject non-authority callers. No client RPC was added.
This is current-state replication, not frame-exact montage time synchronization.

Source also refreshes the cached AnimInstance on ACF appearance changes, retains
the owned dynamic montage through GC, replaces it when the requested sequence
changes, and releases owned montage/root-motion/speed overrides on EndPlay.
Site coordinator creation, station setup and fallback presentation teardown are
authority-gated. It does not solve nearby multi-site teardown isolation on server.

Presentation acceptance cases:

- `Pangea.ACF.Mining.PresentationReplicationContract`: isolated snapshot/RepNotify
  and rejected-client-setter contract passed before the interruption. Simulated
  roles here are **not** proof of real network transport; rerun after the fixes.
- `Pangea.ACF.Mining.RemotePresentationInPIE`: real listen-server plus remote-client
  worlds, replicated role components, moving locomotion, station montages,
  rejected client authorship, server-only jobs and replicated actor destruction.
  It uses per-session copied play settings rather than changing user preferences,
  and a transient site with the real definition reference and fixture-only tier 3.

The presentation USTRUCT/UPROPERTY/UFUNCTION layout was normally rebuilt by the
user, not applied through Live Coding. The earlier remote attempt was interrupted
by the separate hunting declaration fatal; the current section above records
subsequent corrected-fixture acceptance. Dedicated server, late join/relevance
re-entry, latency/loss, packaging and custom actor-level presentation-interface
overrides are further gates, not certified by the first listen-server case.

The corrected economy, pause-offline accounting, iron definition and presentation
config were unchanged; both definition/config files retain their previous SHA-256
hashes. Staging summary: `Saved/Yarin/Mining-Network-Animation-Staging-20261020.json`.

## Earlier lifecycle/contention acceptance (2026-10-20)

Added the Editor-only native automation case
`Pangea.ACF.Mining.LifecycleAndContentionInPIE`. It reuses the transient SC_Demo
tier-3 fixture described below; no saved progression, economy or assets changed.
No runtime correction was needed for this acceptance slice.

Verified against real Smart Object reservations and ACF-controlled movement:

- An externally reserved primary slot remains claimed. The worker blocks without
  stealing it, using the deposit slot as fallback, or moving more than 30 cm;
  freeing the reservation allows backoff/retry to resume the duty.
- A combat-state transition during observed active path following immediately
  suspends the duty, cancels its owned task and frees the station reservation.
- Removing the active Smart Object from the runtime subsystem invalidates the
  task and blocks the duty; re-registering that component recovers on retry.
- Unpossession suspends the job and frees claims without the site taking back
  possession. Explicit repossession by the same controller resumes interaction.
- Moving the player outside the relevance radius culls workers and clears the
  selected old job/claims without leaving an unpossessed site-owned AI controller.
  Returning spawns a fresh worker identity that resumes interaction.

The individual test passed in **17.21 seconds** with 136 warnings. The subsequent
full `Pangea.ACF` regression passed **52/52**, with 0 failed and 0 skipped;
554 warnings were recorded across nine tests. All **23 mining tests** passed as
a subset of that full run (233 warnings), not as an independent focused run.
These results are not warning-free. PIE was inactive after verification.

The Editor-only test compiled successfully through Live Coding at 12:09:12 UTC.
Include it in the next normal build; no new reflected layout was introduced.
The presentation config is saved/clean, and it and the iron definition remain
byte-identical to their pre-test hashes. Reports:
`Saved/Yarin/Mining-Lifecycle-{Initial,Regression,Verified}-20261020.json` and
`Saved/Yarin/Mining-Lifecycle-Verification-20261020.log`.

**Remaining acceptance:** multi-client authority/presentation, active-duty site
save/load, actual death and persistent roster/recruitment, physical obstacles and
partial paths, natural threat/perception combat, controller replacement, nearby
multi-site isolation, and packaging. Runtime station removal is not deletion of
the persistent station asset; distance culling is not worker death/recruitment.
A forced combat-state transition is not a full combat encounter.

## Earlier live navigation/interruption acceptance (2026-10-20)

Added the native Editor automation test
`Pangea.ACF.Mining.NavigationAndInterruptionsInPIE` in `MiningSystemEditor`.
It runs against the configured discovery/site Blueprint, stations and generated
navigation in `/Game/Maps/SC_Demo`, not an empty movement mock. A transient copy
of tier 3 is used as a level-0 fixture so it can exercise two workers, one guard
and the courier without purchasing upgrades or modifying the saved definition.
Player/worker teleports are used only to arrange the fixture; subsequent mining
movement is measured and must use the actual path-following tasks.

The first run was interrupted by an Editor restart. Its last recorded runtime
error was a controller/pawn ownership loop. No successful navigation result was
inferred from that interrupted run. After recovery, the test found and drove two
runtime corrections:

- **Acyclic possession ownership:** spawning the controller with `Owner=Pawn`
  conflicted with possession assigning `Pawn.Owner=Controller`. The controller
  is now owned by the site, producing `Pawn -> Controller -> Site`, not a cycle.
  PIE asserts the possession chain. Site association in the test uses the
  existing site tags rather than assuming a possessed pawn is directly site-owned.
- **Bounded presentation sessions:** the configured GI presentation StateTrees
  have delay tasks but no exit transitions. Their contexts can stay Running
  indefinitely, so waiting for terminal success stranded the miner in Working.
  The routine now starts a separate work deadline after observed arrival and
  ends its owned context/claim when that configured session expires, advancing
  to the next leg. Earlier terminal failure still blocks/retries. A timed end is
  an intentional context cancellation, **not** a forged StateTree-success result
  and not a mineral/inventory reward. The economic simulation remains separate.

An additional failed command assertion was corrected in the **fixture**, not in
ACF: a spline starting under the pawn completed immediately through the stock
follower's already-at-goal handling. The fixture now chooses a genuinely distant,
projected, reachable first point so a command is still active when inspected.
The stock follower's general already-at-goal/reentrant-completion path was not
modified or certified by this test and should be tracked as separate ACF hardening.
The temporary first-interaction-tick diagnostic confirmed the real gameplay task
was ticking after arrival; that diagnostic is now Verbose rather than Display.

Successful PIE assertions now cover:

- Two workers spawn through the ACF controller path; the selected miner moves
  meaningfully and is within 150 cm of its actual slot before reporting Working.
- The selected miner performs Working **and Depositing**, the guard reaches
  Guarding, and the courier performs both Loading and Unloading.
- An ACF **combat-state transition during interaction** immediately suspends the
  duty, cancels its interaction task and frees both station slots; Routine resumes.
- A real ACF command-manager/spline command remains active while the mining job
  is suspended and the station is free; the job resumes after command completion.
- An injected off-nav destination blocks without direct movement/teleport fallback
  (pawn displacement must stay within 50 cm).
- Destroying the site leaves the job unassigned or destroyed. The later lifecycle
  case above adds contention, removal, possession and distance-culling coverage.

The individual case passed, then passed again in **22/22 focused mining tests**
and **51/51 full Pangea.ACF regression tests** (0 failed, 0 skipped). Focused tests
logged 95 warnings and the full suite logged 416; these are not warning-free
runs. Warnings include existing sample animation, GAS curve and scene diagnostics.
PIE was inactive after verification.

Function-body/test changes were applied through successful Live Coding compiles
at 11:47:01, 11:53:38 and 11:57:14 UTC in Editor PID 29440. Include these source
changes and the Editor-only test dependencies in the next normal build; there is
no new reflected layout requiring another immediate rebuild.

Reports: `Saved/Yarin/Mining-Navigation-{Run1,Run2,Run3,Focused,Regression}-20261020.json`,
plus the verification summary/log and interrupted-session log in that folder.
The presentation config and iron definition files are byte-identical to their
pre-test hashes. No persistent level/content edits or economic changes were made.

At this stage, contention, travel interruption and broader lifecycle were still
pending. The current-status section above records the subsequent coverage and
the acceptance gates that remain open.

## Earlier build and activation verification

This is the first behavior-integration slice, **not completion of the mining-site
design brief**. The user completed the normal full C++ rebuild. Mining runtime and
presentation DLLs were written at 13:24:55 local, before Editor PID 13672 started
at 13:25:03. All five new tests were discovered in the rebuilt Editor.

Initial mining verification passed 20/21: the isolated routine-controller fixture
lacked the GameState team configuration required by ACF possession. The fixture
now supplies a real GameState, UACFTeamManagerComponent and transient team config,
instead of suppressing those errors. Its first Live Coding compile linked with
three unresolved AscentTeams symbols; adding the explicit editor-only AscentTeams
dependency resolved that failure. The second Live Coding compile succeeded at
11:29:55 UTC. These verification changes affect the fixture/build dependency, not
runtime behavior or reflected layout; include them in the next normal full build.

Corrected mining verification passed **21/21 with no warnings**. After activation,
the full `Pangea.ACF` suite passed **50/50, 0 failed, 0 skipped**. Seven non-mining
tests emitted 358 warnings (sample/animation diagnostics and expected test logs);
the complete suite is not warning-free. PIE was inactive before and after.

Saved the existing `/Game/_Game/MiningSystem/DA_MiningSitePresentation_Test` with
only `bUseACFRoutineJobs=True` and the existing ACF routine controller class added.
The asset was clean before editing and is saved/clean afterward. Readback confirms
both values. The iron definition export is identical before/after; all role
settings, corrected economy, upgrade costs, shipment timers and pause-offline
accounting are preserved. No sample assets were edited by this verification step.

Reports and before/after exports: `Saved/Yarin/Mining-Routines-*-20261020.json`;
compile diagnostics: `Saved/Yarin/Mining-Routines-LiveCoding-20261020.log`.
The prior pending report/UHT log are historical staging records.

Those earlier 21/50-test runs did not verify NPC locomotion. The new dedicated PIE
case above adds that coverage. The existing UI test named
`MiningActorRoutesInPIE` checks menu routing, not NPC locomotion; its successful
result must not be presented as a navigation acceptance test.

## Ownership

1. `AACFAIController` and its existing Behavior Tree own AI state, threats, combat
   and commands. The site must not force Routine state to defeat those decisions.
2. `UACFAIRoutineComponent` owns the requested duty. Project-owned
   `UMiningRoutineTask` forwards schedule start/end to a controller-owned
   `UMiningRoutineJobComponent`. The first slice installs an all-day midnight
   duty; authorable multi-shift schedules are a later extension, not implemented.
3. The job component owns its navigation/interaction child tasks, not the whole
   controller. It yields immediately on an ACF state-change event; commands,
   death, changed possession and schedule ownership are checked every 0.1 seconds.
   It resumes only while alive, in `AIState.Routine`, without an active command,
   and still holding its assigned schedule and pawn. It never calls controller
   StopMovement/brain StopLogic as a generic cleanup shortcut.
4. Smart Objects reserve worker/guard stations. The existing mining
   GameplayInteraction StateTree backend remains. Stock ACF Smart Object tasks use
   GameplayBehavior and are not a drop-in replacement for these definitions.
5. `UMiningSiteComponent` alone owns abstract production, storage and shipment
   accounting. Animation/job completion does **not** grant minerals or deliver a
   second shipment. Couriers in this slice visualize the abstract system; they do
   not carry authoritative inventory or gate its delivery time.

The inspected ACF routine implementation starts the `UACFTask` object stored in
the definition rather than creating an instance per NPC. The bridge therefore
creates a private transient definition **and task per controller**. Do not assign
one live mutable task instance to many NPCs. No ACF plugin source was modified.

## Movement and cleanup

- Worker/guard primary duty uses station slot **0**, secondary duty slot **1**.
  A busy/missing slot blocks/retries; it does not silently select a different duty.
  Verify slot transforms against scene markers during migration.
- Courier uses pathfinding between configured site/settlement targets. Exclusive
  loading-dock Smart Object reservations are not implemented in this slice.
- All hybrid moves disallow partial paths; no direct SetActorLocation or
  pathfinding-disabled courier fallback is used. Missing navigation blocks/retries.
- Travel deadline defaults to 120 seconds; retry delay to 3 seconds, editable on
  the presentation config independently of each role's interaction duration.
- Minimum worker/guard interaction time begins after arrival and retains the
  occupied claim even if the presentation StateTree succeeds earlier. A failed
  StateTree does not earn a successful hold. Abort and invalidation release the
  claim through the existing task-owned cleanup path.
- The routine also bounds each presentation session by the authored duration
  after arrival, so a non-terminating cosmetic StateTree cannot prevent the next
  duty leg. Cancellation at the session boundary is not a gameplay reward signal.
- Completion results are cached synchronously before task garbage collection can
  erase a finished task pointer between timer updates.
- The site creates the selected controller via deferred pawn spawning, before
  possession. The inspected controller inherits the sample `DA_RoutineTask`:
  spawning suppresses automatic possession, creates a fresh controller, and
  installs an empty private transient schedule **before** possession starts its
  Behavior Tree. The assigned private mining duty follows. The plugin sample
  schedule/task instances are never intentionally started for these mining NPCs.
  Construction/character-definition overrides are checked afterward;
  incorrect controllers fail closed rather than being silently replaced live.
- Hybrid actors preserve their existing collision setup. They are not subjected
  to the old presentation-only mesh-collision disabling loop.
- Site teardown/culling revokes assignments before destroying presentation actors.
  Detach restores the original routine definition only if no external owner has
  replaced it. It does not force-start an old duty while tearing down the pawn.
- Ending presentation restores travel speed only while the component still owns
  that value; it preserves a newer combat/command speed. Root-motion restoration
  similarly avoids overwriting a different mode applied by another owner.

## Activation checklist and remaining acceptance

1. Completed `Pangea.ACF.Mining` and full `Pangea.ACF` native regression runs above.
   New cases cover bridge metadata, per-controller definition/task isolation and
   stop ownership, rejecting legacy controllers, minimum hold/claim retention,
   and locomotion ownership. These are not substitutes for real navigation tests.
2. Saved/read back the rebuilt presentation config fields on the existing
   `/Game/_Game/MiningSystem/DA_MiningSitePresentation_Test`:
   `bUseACFRoutineJobs=true` and `RoutineControllerClass` now references
   `/Game/Yarin/BP_Pangea_MiningRoutineController.BP_Pangea_MiningRoutineController_C`.
   Its inspected defaults are `AIState.Routine` and the existing `ACFBT`.
   Do not reparent/edit the three legacy Mining*AIController_Test assets or plugin
   samples. New project assets, if needed later, belong in `/Game/Yarin`.
3. Stop/restart PIE to migrate already spawned actors; live controller replacement
   is intentionally unsupported. Inspect actual post-spawn controller, character
   definition, team, combat behavior, routine component and job diagnostics.
4. Live tier-3-fixture movement, work/deposit, guard/courier states, combat-state
   and command interruption/resumption, off-nav failure and basic site destruction
   passed in the new PIE test. Extend this to all remaining cases listed above;
   the fixture does not certify every tier's final content or physical transport.
5. The subsequent lifecycle test verifies travel interruption, unpossession and
   explicit repossession, contested/removed stations, and distance culling/re-spawn.
   The new tests additionally cover nearby-site cleanup isolation and in-memory
   active-duty ALS restore. Actual death/recruitment, controller replacement,
   simultaneous multi-site navigation and disk/new-session active-duty restore
   remain open; do not call all lifecycle work complete.
6. Test cooked asset references, packaged navigation and server/remote-client
   visuals separately. Native role/state replication is now staged as described
   above and verified for the scoped listen-server/native-player fixture. NavMeshBounds/Recast actors alone are not proof of
   usable paths; the new test verifies actual path-followed arrival in SC_Demo.

## Brief preserved as target design, not silently applied balancing

Setup is gated by **Wood & Stone Area level 5** and discovery near a vein.
Level 0 supplies site visuals/chest, manual hauling and **1.25x manual mining
speed**. The current test definition's milestone/cost setup differs; explicit
migration/validation is still required. The gate must query the actual facility
level, not treat an unrelated settlement tier or tag alone as equivalent.

| Tier | Total storage | Duties/unlocks | Auto output/day | Shipment loss | Unattended worker loss |
|---|---:|---|---:|---:|---:|
| 0 | No capacity specified | Manual mining/hauling | 0 | Not applicable | None |
| 1 | 50 | Courier shipping (hourly intent, amount/timing to refine) | 0 | 25% | None; no miners |
| 2 | 150 | Two miners; recruit replacements | 50 | 25% | 25% |
| 3 | 250 | Guard; recruit replacement | 50 | 25% | 10% |
| 4 | 350 | Faster courier, dino chariot | 100 | 25% | 10% |
| 5 | 450 | Courier escort patrol | 100 | 5% | 10% |

Target upgrade costs from the brief:

- L1: 100 wood, 50 hide, 100 fiber, 50 stone.
- L2: 100 meat, 300 berries, 100 herbs.
- L3/L4/L5 each: 200 wood, 100 stone, 100 hide, 200 fiber, 100 meat,
  300 berries, 100 herbs.

The existing iron test definition instead uses its corrected Wood/Metal costs;
these were **not replaced**. It also currently requests two guards at L5, which is
not itself a distinct moving escort patrol.

### Remaining feature work, in order

1. Extend runtime acceptance to disk/new-session active-duty persistence, network authority/client
   presentation, physical obstacles/partial paths, natural combat, actual death,
   controller replacement and nearby multi-site isolation as listed above.
2. Introduce a saved, authoritative worker roster (identity, alive/dead, role,
   recruitment state) independent of transient visual actors. Current distance
   culling/despawning is **not** worker death or recruitment. Reconcile production
   staffing with this roster without creating a second production clock.
3. Define **unattended**, risk evaluation period and replacement recruitment cost.
   The percentages specify no sampling period; do not roll them each frame,
   movement loop or arbitrary refresh. Persist outcomes and follow pause-offline.
   If observed attacks and abstract risks coexist, avoid charging the same loss
   twice. Define guard death/replacement behavior explicitly.
4. Add authoritative shipment records/cargo with one transaction owner before
   making courier arrival gate inventory delivery. Keep a consistent off-screen
   abstraction and loss resolution; do not duplicate existing timer deliveries.
5. Model L4 transport and L5 escort as explicit courier/vehicle/escort assignments
   using the relevant ACF systems, not only higher speed or extra stationary guards.
6. Reconcile the prerequisite, tier content and final item costs as a deliberate
   data migration, preserving corrected values until approved balancing changes.

Probability period, unattended-area radius, worker replacement pricing, shipment
quantities and observed/off-screen loss arbitration remain design decisions. No
new offline simulation or chance-based worker death was added here.
