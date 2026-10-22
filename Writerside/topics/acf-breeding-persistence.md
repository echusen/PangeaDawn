# Breeding persistence and replication (B03)

## Status — rebuilt and native Editor regression verified

The user completed a full C++ build and reopened the Editor. DLLs were dated
2026-10-19 23:51:40 (PangeaCore) / 23:52:01 (breeding), before the fresh Editor
started at 23:52:14 local. The first focused run passed 5/6: the isolated egg
fixture encountered Enhanced Input ensures from game-world subsystem lifetime.
The test fixture now uses an EditorPreview world for actors/authority/timers,
without creating game-only input subsystems. No engine error suppression or vendor
edit was used. This non-reflected test-only fix compiled successfully via the
approved Live Coding tool (`LogLiveCoding: Live coding succeeded`, 21:55:58 UTC).

The complete native `Pangea.ACF` run then passed **25/25, 0 failed, 0 skipped**,
including all six breeding tests and the strengthened real-PIE hatch/restore test.
PIE was inactive before and after. Seven tests still logged warnings (including
diagnostic cooldown messages); this is not a warning-free certification. Dedicated
multiplayer, real disk restart/legacy saves and packaged builds remain gates.

Reports:
- `Saved/Yarin/B03-Breeding-FirstRun-20261019.json` — initial 5-pass/1-fail run.
- `Saved/Yarin/B03-PangeaRegression-20261019.json` — final 25-test results, errors/warnings.
- `Saved/Yarin/B03-UHT-Fix-Verified-20261019.log` — earlier successful header check.

The user selected **pause offline**. No balancing values, asset references,
ACF vendor files or sample files were changed by B03. Corrected Triceratops
baselines and inherited targets remain. The preceding 21-test result documented
in [creature profiles](acf-creature-profiles.md) is historical, superseded by this run.

## Runtime ownership and policy

- `APDDinosaurBase` owns a replicated, SaveGame `CreatureId`, exposed through
  `IPDCreatureIdentityInterface`. Authority generates it at initialization if
  missing. Queries never create IDs. Save restoration supplies the saved ID;
  a new creature gets a new ID. Actor-owned identity survives breeding feature removal.
- Breedable components on non-Pangea actors use a saved/replicated fallback GUID.
  It is generated on authority during BeginPlay/save/load, not while building a snapshot.
- Gender selection and fertility mutations are authority-only. Gender, explicit
  fertility, cooldown state, traits, inherited profile/appearance and lineage are
  saved and replicated. Clients consume that state, rather than randomizing gender.
- Blueprint compatibility retains existing writable trait/fertility properties.
  Gameplay should use authority-side setters; assigning those fields directly in
  a client Blueprint is not a server RPC and is not authoritative.

## Pause-offline timers

Cooldown and incubation save **the actual timer's remaining seconds** at the ALS
save hook. Loading rebuilds transient timers from that duration. No world-session
start timestamp or wall-clock timestamp is serialized, and offline time is never
subtracted. World game time (including normal pause/time-dilation behavior) drives
both timers. Display updates sample the timer; they do not accumulate a presumed
fixed tick interval. Replicated display values are periodic samples, not a
client-predicted countdown or client-authoritative timer.

EndPlay clears timers and delegate bindings. Explicit infertility is separate
from cooldown: completing a cooldown does not override a deliberate infertile flag.

## Eggs and inherited state

An egg validates the parental species/definition, then caches once:

- both parent snapshots and durable lineage IDs;
- selected offspring definition and inherited-stat application effect;
- combined traits and child gender;
- generated inherited stat targets and material color/scalar values;
- total duration, remaining duration and state (`Uninitialized`, `Incubating`,
  `ReadyToHatch`, `Hatched`).

Repeated initialization is ignored. Hatch retries and current-format restoration
reuse the cached results, never call the genetics strategy or visual/stat mutation
again. The offspring definition is selected through the mother/breeding rules at
initialization; the child's own ACF profile still owns its base appearance/stats.
Juveniles do not need a reproduction fragment to retain inheritance.

The server deferred-spawns the child and supplies its definition before ACF
initializes. A child must have its breedable component to retain inherited state;
otherwise the attempted child is destroyed and the egg is preserved. Missing class,
spawn or definition-assignment failures also preserve the egg. When automatic
incubation ends, a failed hatch remains ready for an explicit retry; it does not
start an unbounded automatic retry loop. Existing explicit `Hatch` commands may
still force an early hatch. Re-entry is guarded and the state is committed before
broadcasting the hatch event, so a listener cannot produce a second child.

The child stores its traits, gender, lineage, generated appearance, stat profile
and application effect independently of parents/egg lifetime. Appearance is
reapplied after load, on RepNotify and after ACF's appearance-change event, covering
profile/replication arrival order. GAS remains the authority for live attributes;
no parallel ARS statistics component is introduced.

## Save and network format

All relevant nested trait/snapshot/appearance members carry SaveGame flags, not
just their outer actor/component property. Eggs use their inherited ALS savable
interface; dinosaur component collection still merges ACF's existing save list.
No separate persistence subsystem or hand-written disk format is introduced.

`FParentSnapshot` keeps its existing maps for callers but supplies a native bounded
`NetSerialize` format (maximum 256 entries per collection), including gameplay-tag
serialization. The optional runtime `ParentActor` reference is not saved or sent;
lineage must never depend on a live actor or actor path. Generated appearances use
arrays, avoiding default replicated-map support assumptions.

### Header-check correction

The first build attempt exposed two invalid declarations: explicit `BlueprintPure`
on the identity interface, and map members lacking `NotReplicated` inside replicated
parent snapshots. The interface getter is now `BlueprintCallable` without explicit
`BlueprintPure`. `ParentA`/`ParentB` remain replicated; all snapshot maps carry
`NotReplicated` solely to bypass **default member** replication. The native
`NetSerialize` still explicitly sends their contents, and SaveGame flags remain.
The optional runtime parent pointer also skips default member replication.

A standalone UE 5.8 UHT check with `-WarningsAsErrors -NoOutput` succeeded after
this correction (0 generated files written). It did not compile/link C++ or patch the running Editor; the subsequent full build
and regression results are recorded above. The wire
regression test now goes through `FStructProperty::NetSerializeItem`, the engine's
native property serialization path, and asserts map contents still survive.

State format version 1 is written for new records. Save deserialization resets the
version marker before reading tagged fields, so a missing version is detected even
when loading into an existing object. Components warn for legacy records and retain
available state/defaults; they cannot reconstruct previously unsaved history. Legacy
or unsupported egg records are **not** automatically resumed or rerolled. Unsupported
future component versions are not reinitialized or silently stamped as version 1.
Back up real old saves and define a migration/recovery policy before releasing this.

## Verification results

Four new `Pangea.ACF.Breeding` tests **passed in the final 25-test run**:

- `PersistentReplicationContract`: reflected SaveGame/Net flags and native snapshot serializer.
- `ParentSnapshotWireAndSaveRoundTrip`: bounded network/archive round trips for
  IDs, nested traits and maps, plus oversized snapshot rejection.
- `CooldownPausesOffline`: fractional remaining duration, identity stability,
  independent restore, gender/genes, repeated load hooks and timer expiry.
- `EggPausesOfflineAndNeverRerolls`: cached outcome, actual remaining duration,
  ALS restore after simulated elapsed time and failed-hatch preservation.

The existing real-PIE `HatchedChildProfileAndPersistenceInPIE` test now also checks
single consumption, stable actor identity, lineage, cached traits/gender, saved
fertility/cooldown and inherited colors after ALS restore and an ACF appearance
refresh. These strengthened assertions passed in the final run. The fertility safety fixture
now has an authority owner instead of bypassing the runtime ownership contract.

### Remaining release gates

1. Preserve the 25-test Editor regression pass. Repeat a clean build including the test-only fixture fix, then verify Game/cooked builds. Review remaining warnings.
2. Dedicated server + two clients + late join: gender, cooldown/progress UI,
   snapshots, inherited appearance, GAS targets, hatch visibility and no duplicates.
   Metadata/wire tests alone do not prove network runtime behavior.
3. Real disk slot save/load, map travel and process restart midway through incubation
   and cooldown; legacy `.sav` migration, recovery and backup policy.
4. Packaged/cooked build and feature-active/inactive save/load behavior.

Removing an optional breedable component still removes its unsaved runtime breeding
state; this batch does not transfer all feature state to a permanent actor-owned
store. Restore requires the feature/component to be present in ALS's component list.
Actor identity does survive feature toggles. Save-world transaction atomicity and
exactly-once births across arbitrary save rollback are not claimed: egg and child
must be recorded coherently by the world save operation.
