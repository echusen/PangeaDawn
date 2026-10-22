# Base-upgrade safety (B04): implementation and verification

## Status — full build and native Editor regression verified (2026-10-20)

The user completed the full C++ rebuild and reopened the Editor. Verified DLL
modification times were 00:17:35 (base) / 00:17:41 (mining), before the fresh Editor
started at 11:17:33 local. All six added tests were discovered in the rebuilt session.

The focused base run passed **7/7, no warnings**. The complete `Pangea.ACF` suite
passed **31/31, 0 failed, 0 skipped**, including the strengthened native mining
fixture and real village-to-mining PIE flow. Seven tests still logged warnings
(cooldown diagnostics and scene/sample/UI warnings); this is not warning-free,
packaged-build or remote-multiplayer certification. PIE was inactive before/after.
No source changes or Live Coding were needed during this verification turn.

The selected policy remains **separate explicit costs**: existing item requirements
check ownership and are never implicitly consumed. Readback in the rebuilt Editor
confirmed that BUD_Village's level-1 and level-2 `ItemCosts` arrays are both empty.
Blacksmith's action and the Mine milestone's single mining-unlock action remain as
migrated. No prices or item quantities were authored for existing levels.

Results:
- `Saved/Yarin/B04-BaseFocused-20261020.json` — all seven focused base results.
- `Saved/Yarin/B04-PangeaRegression-20261020.json` — all 31 test results and warnings.
- `Saved/Yarin/B04-Activated-20261020.json` — build/session evidence and asset export.

Historical staging records remain in `B04-Pending-20261020.json`, village before/after
exports, the asset-only smoke report and the earlier UHT log. The pending file is
not an instruction to reapply the migration. B03's preceding 25-test result and the
old-binary asset-only smoke are superseded by this rebuilt 31-test run.

## Source contract


- Use `TryUpgradeChecked(PlayerPawn)` on the owning upgrade component. It returns
  a Blueprint-readable result and reason. The existing boolean interface and
  next-level `OnLevelIncreased` entry point route through it; arbitrary level jumps
  no longer silently bypass execution. Queries do not consume costs.
- Mutations require authority. Re-entry is blocked before preflight callbacks.
  Level and milestone completion are committed only after checked actions and
  explicit payment finish. Completion uses exact milestone tags.
- `ValidateConfiguration`, `CanExecuteUpgrade`, `TryExecuteUpgrade` and
  `CanReconstructOnLoad` are Blueprint-facing action contracts. Base implementations
  fail closed. Native facility, inventory-grant and mining-unlock actions implement
  them; legacy void Blueprint Execute overrides require explicit migration.
- Absolute facility enable/disable requires exactly one discovered, loaded group.
  Absolute mining unlock/lock requires matching loaded discovery nodes. Toggle
  configurations are rejected rather than guessed into an absolute target.
- Inventory grants preflight capacity and confirm ACF's actual authority-side
  count delta. Reconstruction never dispatches grants. `GetUnlockedFacilityTag`
  lets UI advertise both physical and mining unlocks without a base-to-mining
  module dependency or dummy physical-facility action.
- Explicit `Levels[n].ItemCosts` are aggregated by class and removed from the
  interacting pawn's **unequipped** ACF inventory using GUIDs, not mutable indices.
  Payment is attempted after actions, never during requirement evaluation or load.
  Existing `UReq_HasItems` remains check-only; invalid counts/classes are rejected.
- SaveGame action receipts use definition path + exact milestone tag + authored
  ActionId. If ActionId is None, the existing inline object's relative path is used,
  not its array position. Preserve IDs/paths in released definitions; renaming a
  definition or recreating an inline object requires save migration review.
- `PendingOperation` is saved before side effects. A failed/uncertain action or
  payment sets `RecoveryRequired`; an interrupted-operation record loaded later
  is also fenced. Confirmed effects/receipts are retained, but the level is not
  committed. No automatic retry/refund is attempted. Restore a coherent backup or
  perform an audited migration; never merely clear the fence and retry rewards.
  Save deserialization clears journal defaults before reading tagged fields so
  older records do not retain a live failed command's missing fields.
- Reconstruction respects the save-rule opt-out. Otherwise it rebuilds transient
  facility references, starts upgrade-managed groups locked, and applies completed
  absolute states in ascending level/authored milestone order. Repeated actor and
  component load hooks converge without charges or rewards. Unloaded required
  targets produce a diagnostic failure, not a false successful restoration.
- Native definition validation checks progression/identities, null configuration,
  invalid costs/rewards/ownership entries, unsafe toggles and legacy Blueprint
  overrides. It is also used by command preflight. No ACF vendor edits or parallel
  inventory/save subsystem were introduced.

### Explicit failure boundary

ACF inventory RPCs return void and do not expose an atomic multi-item reservation
transaction. Individual action preflight does **not** reserve combined reward
capacity. A later grant may therefore fail after an earlier grant succeeded, or
inventory callbacks may change payment state. Such cases are explicitly fenced
for recovery; they are not ordinary rollbackable failures and must not be reported
as successful upgrades. Costs are not reached after failed actions. An unexpected
failure during payment can leave partial payment, and the fence prevents another
charge attempt. Recovery uses coherent world/player records, not an automatic
class-based refund that would lose item runtime state/GUIDs.

No exactly-once claim is made across arbitrary save rollback, non-coherent player
and village saves, definition renames or opaque custom Blueprint side effects.
Native tests, real disk slot recovery and failure-injection/remote-client tests
must pass before enabling paid upgrades in production. Current asset costs remain
empty. Server guards alone do not provide player-owned RPC routing or replicated
base UI state; those remain B07 work.

### Village asset migration (applied)

Inspected `/Game/_Game/BaseUpgradeSystem/BUD_Village.BUD_Village`. Its level-2 Mine
milestone had both a physical `UA_EnableFacility(Test.Village.Mine)` and the real
`UA_UnlockMiningSite(Test.Village.Mine)`. The previous regression report confirmed
`FacilityManager: Facility 'Test.Village.Mine' not found`; the catalog contains
only Blacksmith. Removed that no-op physical-facility action, retaining the mining
unlock behavior. All other exported fields/objects match the pre-change export.

The initial attempt to assign an inline-object array by text was rejected without
changes by the native tool. Documented recovery cleared/re-authored only that
milestone's Actions array, producing `UA_UnlockMiningSite_1` instead of `_0`.
This occurred before B04 receipts exist; preserve the new identity after release.
No new asset was created. The asset was already dirty before the targeted change;
its pre-change exported values were recorded and retained except for the intended
array correction. The saved result was exported and the asset-only PIE smoke
passed, with no missing-Mine-facility warning.

### Added tests (all passed after the full rebuild)

Under `Pangea.ACF.Base`:

- `ExplicitCostsPreserveOwnershipGates`: ownership checks do not charge; duplicate
  explicit cost rows aggregate and load does not charge again.
- `FailedPreflightNeverCommitsOrCharges`: insufficient costs/missing target, no
  side effects or false level commit, no skipping levels, client mutation rejection.
- `LoadReconstructsStateWithoutRewards`: reward once, no replay on repeated load,
  deterministic absolute-state ordering and saved action receipts.
- `InterruptedCommandRequiresRecovery`: serialized pending operation blocks retry;
  restoring a coherent pre-command archive clears failed live journal state.
- `PartialActionFailureIsFenced`: a later capacity failure after a confirmed reward
  preserves its receipt, charges no costs, commits no level and prevents duplicate retry.
- `RejectsAmbiguousConfiguration`: unsafe toggle, duplicate identities/levels/tags.

Strengthened the existing native mining-discovery interaction fixture with a real
Blacksmith group/manager, committed-level assertions, checked definition validation
and logical mining-unlock introspection. Its old silent missing-target no-op no
longer qualifies as a valid fixture. Both the strengthened native fixture and real map smoke passed.

## Previously observed behavior (before B04)


Inspected `GF_PangeaBaseSystem/Source/PangeaBaseUpgradeSystem`:

- `LoadCompletedMilestones` executes every action of each completed milestone,
  without distinguishing state reconstruction from one-time rewards.
- `UUA_EnableFacility` supports EnableOnly, DisableOnly and Toggle. Toggle is not
  idempotent, so replaying it on each load can reverse the restored state.
- `UUA_GrantInventory` calls ACF inventory on the interacting pawn. It is not a
  reconstruction action and must not be replayed as a reward during load.
- `OnLevelIncreased` sets CurrentLevel before executing milestones. Actions return
  void, so missing managers, missing inventory and other action failures cannot be
  reported as a failed upgrade. TryUpgrade currently returns true after this call.
- Upgrade and milestone mutation entry points lack runtime authority guards.
- `UReq_HasItems` checks ACF inventory counts but **does not consume items**. Making
  those existing requirements charge players would change the game's economy.
- Facilities are discovered into a transient list; load reconstruction remains
  necessary. Disabling every replay would break legitimate facility restoration.

These are source observations, not proof that a particular shipped save duplicated
items. Normal village load supplies village context, not an interacting pawn.

## Original implementation plan

1. **Explicit action policy.** Distinguish one-time effects from repeatable state
   reconstruction. Inventory grants are one-time. Absolute facility enable/disable
   may reconstruct. Toggle cannot reconstruct without a persisted absolute target;
   validate/migrate that authored case rather than replaying it blindly. Custom
   Blueprint actions require an explicit reviewed policy; do not assume safety.
2. **Deterministic restoration.** Reconstruct completed milestones in ascending
   level/authored milestone order, not gameplay-tag iteration order. Respect the
   existing save-rule opt-out. Repeated restore must converge to the same state and
   never evaluate fresh purchase requirements or re-charge/reward a player.
3. **Server-owned command boundary.** Guard every mutation entry point, including
   direct level changes and debug milestone marking. Queries remain side-effect-free.
   Prevent re-entrant upgrades. Player-owned RPC routing/remote ownership belongs
   to B07; an authority guard alone does not implement a multiplayer command path.
4. **Observable outcomes.** Add a Blueprint-facing result/failure contract on the
   owning component/actions. Preflight definition, context, requirements, costs and
   supported actions before mutation. Do not report success or commit a level
   merely because a void Blueprint event returned. Preserve old entry points as
   compatibility wrappers where possible; migrate existing action assets explicitly.
5. **Partial-effect safety.** Use stable action identities and saved completion
   receipts where one-time actions can succeed before a later action fails. Retrying
   must not grant the same reward twice. Do not claim arbitrary Blueprint side
   effects are rollbackable or that ACF inventory/save operations form an atomic
   cross-system transaction. Define failure/recovery behavior before enabling costs.
6. **Cost policy (user selected).** keep HasItems as eligibility,
   add separately authored consumable upgrade costs, and use ACF inventory APIs for
   validated payment. Do not replace ACF inventory or silently turn all current
   ownership requirements into purchases. Alternative: deliberately migrate existing
   item requirements into consumable payment, after auditing each definition.
7. **Setup validation.** Validate missing/duplicate progression fragments, level
   numbering, missing/duplicate milestone/action identities, invalid facility tags,
   invalid item counts/classes, unsafe replay and unsupported custom actions. Audit
   actual assets and Blueprint overrides before assigning new policies. Retain
   existing definitions and shared village/facility actors.

## Acceptance coverage and remaining release gates

- Load twice: facility state identical, no repeated toggle, no inventory rewards.
- Failed requirement/context/action: no false success and no level commit; no cost
  charged during preflight. Exercise documented recovery after partial execution.
- Retry/re-entry: no duplicate grants or payments; completion receipts survive ALS.
- Client/direct mutation calls: rejected; authoritative queries remain usable by UI.
- Existing check-only item gates and approved cost policy behave as documented.
- Preserve the full Pangea regression suite, village/mining UI flow and ACF quest
  requirement resolution on the interacting player's controller.
- New reflected contracts require UHT plus a closed-Editor full rebuild; no structural
  Live Coding. Disk saves, dedicated-client routing and packaged builds are separate
  gates, not implied by native unit tests.

### Remaining gates after the native pass

Dedicated-server/two-client routing and base UI replication remain B07 work. Verify
real disk slots, coherent player/village recovery, legacy records, custom Blueprint
action re-entry/failure injection, definition-identity migrations and cooked/Game
builds separately. The passed native partial-failure test proves fencing, not
universal rollback or atomic inventory reservations.
