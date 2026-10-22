# Mining timing and pause-offline persistence (B05)

## Status — rebuilt and native Editor regression verified (2026-10-20)

The selected policy is **pause offline**, matching breeding. The user completed
the full structural C++ rebuild. Mining's DLL was written at 11:42:13 local, before
the reopened Editor (PID 24976) started at 11:42:19; all nine new tests were found.

The first focused run exposed a real save defect: the default-valued version-1
marker was omitted by archetype-delta serialization, so current saves entered
legacy migration. A function-body-only fix makes mining SaveGame writes complete
tagged snapshots and restores the caller's delta policy afterward. The Editor's
approved Live Coding compile succeeded at 09:45:50 UTC (11:45:50 local).
No reflected layout changed in this correction. Initial verification below used
the successful live patch. The later B06 user full build incorporated this fix in
the normal DLL and passed the expanded 45-test suite; see
[B06 mining public APIs](acf-mining-public-apis.md).

After the fix, focused mining passed **11/11, no warnings**. The complete
`Pangea.ACF` suite passed **40/40, 0 failed, 0 skipped**. Seven non-mining tests
still logged cooldown diagnostics and sample/scene/UI warnings; the full suite is
not warning-free. No unexpected legacy-mining migration/recovery warnings were
present. PIE was inactive before and after testing.

No assets or balance values were edited. Before/after exports of
`/Game/_Game/MiningSystem/DA_MiningSite_Iron_Test.DA_MiningSite_Iron_Test`
matched exactly. Its simulated day remains 300 seconds; output, storage, workers,
guards, shipment intervals/risks and upgrade costs remain authored as before.
The post-test export also has the same SHA-256 as the original export.

Reports under `Saved/Yarin`:
- `B05-Mining-Initial-20261020.json` — original 9-pass/2-fail focused run.
- `B05-Mining-Focused-20261020.json` — corrected 11/11 focused results.
- `B05-PangeaRegression-20261020.json` — complete 40/40 results and warnings.
- `B05-Activated-20261020.json` and `B05-LiveCoding-20261020.log` — activation evidence.
- `B05-MiningDefinition-Verified-20261020.json` — unchanged post-test definition.

The earlier pending report/UHT-only evidence is historical and superseded by
these results; it is not an instruction to reapply the changes.

## Ownership and timing contract

- `UMiningSiteComponent` owns production accounting. ACF storage still owns real
  chest inventory; ALS still owns serialization and callback dispatch. There is
  no parallel inventory/save subsystem or wall-clock/offline simulation.
- Fractional **units**, not fractional seconds at a potentially changing rate,
  carry between production calls in `ProductionRemainder`. Unsaturated production
  at 50 units / 300 seconds should yield 50 both for one 300-second sync and 300
  one-second syncs. Floating-point boundary roundoff is tolerated at 1e-9 units.
- Game-world time is paused/time-dilated with gameplay. A transient initialized
  anchor replaces zero-as-an-uninitialized-timestamp. Zero is a valid clock.
  A backward clock step rebases without producing a negative elapsed interval.
- Sync, explicit simulation, save settlement and shipments reject recursive
  timing operations. Accounted time, fractional progress and shipment cooldown
  are committed before synchronous inventory/storage delegates are invoked.
- Upgrades settle elapsed production against the **old** rate/capacity before
  changing level. A fraction carries only if output class and tag are unchanged.
  Newly unlocked shipping starts a full interval. Already unlocked shipping keeps
  its remaining time, clamped if the new interval is shorter.
- Capacity remains a hard cap: overflow is discarded, never banked as future
  output. Reaching/full storage clears fractional backlog. ACF rejection is
  reported as zero actual output rather than assumed success. Storage snapshots
  sum matching stacks; explicit item classes take precedence over tag fallback.
- This slice retains the existing lazy sync/explicit simulation entry points.
  It does not introduce a new production scheduler, change courier visuals or
  replay historical shipments. Production comparisons assume the same storage
  availability; different shipment/withdrawal schedules can still change output.

`ProduceForElapsedSeconds` adds **explicit supplemental simulation time** (the
existing advance-one-day/debug path). It does not advance the online accounting
anchor or consume shipment cooldown. It must not also be used to account the
same online elapsed time that `SyncProductionFromWorldTime` accounts.

## ALS save/load

- `AMiningSiteActor::GetComponentsToSave` already includes its mining component.
  The new `OnComponentSaved` settles online production and updates remaining
  cooldown. It **does not dispatch shipments**, remove their inventory, roll loss
  or deliver resources while ALS is taking a multi-actor snapshot.
- Saves persist whole stored inventory, fractional output, remaining cooldown and
  timing schema version, including values equal to class defaults. This prevents
  a current record being mistaken for legacy and makes saved zero values replace
  nonzero state when reusing an actor. The archive's original delta policy is
  restored after writing this component. Old `LastProductionWorldSeconds` and
  `LastShipmentWorldSeconds` fields are retained only for legacy archive reads;
  gameplay must use the new remainder/cooldown getters instead.
- SaveGame deserialization marks timing awaiting restoration and clears new-field
  defaults before reading tags, including when reusing an existing component.
  `OnComponentLoaded` rebases the runtime clock to **now**, never to saved world
  seconds. Repeated callbacks without another deserialization are no-ops.
- Legacy records cannot recover historical fractions or an exact remaining
  cooldown. They keep their whole inventory/level, start with zero fractional
  output and a full current shipment interval, and log one migration warning.
  Future/unsupported timing versions keep automation paused rather than silently
  overwriting unknown state.
- Actor load still rebuilds its chest before ALS calls component load hooks.
  Seeding an empty ACF chest now iterates a stable saved-item snapshot because
  `AddItem` synchronously broadcasts and rebuilds the mirrored inventory.

## Shipment semantics

`TryAutoShipment` first settles current online time. A due attempt dispatches at
most one shipment per call, not one for every missed interval. An empty due
attempt still consumes its interval (the existing rule). Actual removed quantity
is used for the single loss roll and delivery event; zero removal does not roll
or broadcast delivery. Save/load alone never makes a due shipment fire.

Establishment, level, storage, fractional output and remaining cooldown now have
both SaveGame and Replicated declarations matching lifetime registration. Clients
receive timing snapshots, not server-session timestamps. This fixes metadata,
but is not dedicated-server/remote-client certification. B07 command routing is
still a separate task; mining upgrade payment is not the B04 checked transaction
protocol, and arbitrary custom Blueprint inventory callbacks are not made atomic
by this timing slice.

## Verified native tests

All nine new tests in `Private/Tests/MiningTimingTests.cpp` passed:

- `Pangea.ACF.Mining.CadenceInvariantFractionalProduction`
- `Pangea.ACF.Mining.PauseOfflineALSRoundTrip`
- `Pangea.ACF.Mining.UpgradeSettlesOldRate`
- `Pangea.ACF.Mining.CapacityAndInvalidElapsed`
- `Pangea.ACF.Mining.ShipmentCooldownAndReentry`
- `Pangea.ACF.Mining.LegacyTimingMigration`
- `Pangea.ACF.Mining.LinkedACFStorageAccounting`
- `Pangea.ACF.Mining.TimingDefinitionValidation`
- `Pangea.ACF.Mining.PersistentReplicationContract`

Fixtures use isolated EditorPreview worlds and controlled game clocks, not the
live PIE world. The ALS round trip uses actual collection/reflected hooks and
actor/component serialization. The focused run also passed the existing cost
aggregation and village-unlock tests. The complete 40/40 run supersedes B04's
earlier 31/31 count. Default-value round trips and archive-policy restoration are
explicitly asserted in the capacity test after the serialization correction.

Remaining release gates: real on-disk old/new slots across process restarts,
actual site/chest/presentation load order, dedicated-server/client behavior,
packaging and long-session storage/withdrawal behavior. Keep backups of old save
slots until these migration checks pass.
