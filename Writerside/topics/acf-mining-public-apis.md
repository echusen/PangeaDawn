# Mining Smart Object public APIs (B06)

## Status — rebuilt and native Editor regression verified (2026-10-20)

The user completed the full C++ build, retaining the editor-only
`PropertyBindingUtils` dependency fix for the compiler fixtures. Runtime DLL time
was 12:03:36 local (presentation DLL 12:02:04), before Editor PID 24024 started at
12:03:56. All five new tests were discovered.

Initial verification caught two test-assumption issues, not runtime-task failures:
EditorPreview worlds do not create SmartObjectSubsystem, and disabling admission
does not invalidate an already occupied slot. Fixtures now use isolated **Editor**
worlds and explicitly remove/re-register runtime state for the invalidation test.
They also assert that disabling admission alone does not evict the active user.
More specific fixture diagnostics were added. No runtime-task code changed during
verification. Both test-only Live Coding compiles succeeded (10:08:03 and
10:12:07 UTC); include those test corrections in the next normal full build.

Corrected focused mining passed **16/16 with no warnings**. The complete
`Pangea.ACF` run passed **45/45, 0 failed, 0 skipped**. Seven non-mining tests logged
634 warnings, including 271 `LogVSAccessor` messages in the UI page smoke test
(Visual Studio/module-table access), alongside sample/scene/animation diagnostics.
The full suite is not warning-free. PIE was inactive before and after testing.

No Content assets were edited. The iron definition export still matches the
original B05-before export exactly, SHA-256
`F16C9836DE8E5537738BC308CA56B0C4FF1CF22E0B5B435C82C3FAB96AC730DC`.

On 2026-10-20, replaced native mining's use of the engine-private
`UAITask_UseGameplayInteraction` convenience task. The replacement composes
**public** UE 5.8.3 APIs: `UAITask`, `UAITask_MoveTo`,
`FGameplayInteractionContext` and `USmartObjectSubsystem`. It does not replace
GameplayInteractions, switch Smart Object behavior families, or introduce a
second StateTree runtime.

Removed the GameplayInteractions engine `Private` include directories from both
`GF_PangeaMiningSystemRuntime.Build.cs` and `MiningSystemPresentation.Build.cs`.
Static scan of mining `.h`, `.cpp` and `.cs` files found no remaining references
to the old task or that private directory. Public APIs can still change between
engine versions; this is not a claim of compatibility with untested releases.

Historical staging UHT `-WarningsAsErrors -NoOutput` passed with a **validation copy** of the existing
manifest extended to include the new private UCLASS header and with the old
engine-private include paths removed. The first check against the stale original
manifest could not find the new class; the extended copy resolved that failure.
No generated headers were written during that staging check; it was not C++
compilation. The subsequent user full build applied the new UCLASS/transient task
map and incorporated B05's prior serialization correction into the normal DLL.
The two verification Live Coding patches changed tests only, not reflected layout.

## Design and ownership

- Internal `UMiningSmartObjectInteractionTask` lives under runtime
  `Private/Tasks`. Its factory/cancel/result functions and completion delegates
  follow the Blueprint-callable gameplay-task pattern. Private implementation
  remains private rather than becoming a global utility namespace.
- The coordinator still selects and claims the same slots with the same filters,
  priorities and preferred-slot logic. A rejected factory leaves the fresh claim
  with its caller, which frees it. An accepted task exclusively owns cleanup;
  callers must not independently free/reuse that claim.
- Production calls the task with navigation and AI-logic locking enabled, matching
  the previous convenience-task call. Movement uses public `UAITask_MoveTo` with
  pathfinding and no partial paths. The public interaction context runs the
  existing GameplayInteraction Smart Object behavior/StateTree after arrival.
- The factory rejects missing controller/pawn, non-authority, invalid/free slot,
  missing behavior, and missing/uncompiled StateTree. It is trusted server-side
  presentation glue, not B07's secure remote-player command protocol.
- The original pawn is captured; possession changes terminate the task rather
  than continuing the old worker's interaction on a newly possessed pawn.
- All exits converge on one guarded cleanup path: cancel any pending move,
  provide abort context when appropriate, stop the interaction, unregister its
  invalidation callback and release its accepted claim. During slot invalidation
  the subsystem owns release; the task must not release a second time. Repeated
  cancellation is a no-op. Success requires a completed, succeeded StateTree,
  not merely task destruction.
- Coordinator task references are now `UPROPERTY(Transient)` so a queued/live task
  and its reflected context remain GC-visible. Accepted claims are not freed again
  just because a task already finished or its pointer was GC-cleared.
- Clearing presentation and component EndPlay cancel even orphaned task-map
  entries whose weak actor keys are invalid. Registration occurs before activation
  because movement/StateTree callbacks can complete synchronously.

The existing worker/guard/courier classes, Smart Object definitions, StateTrees,
route targets, hold durations, presentation configuration and B05 economy values
were not edited. The low-fidelity fallback remains in place. No Content assets,
engine files or ACF plugin sources were changed for this slice.

## Verified native tests

All five new tests in `Private/Tests/MiningSmartObjectTaskTests.cpp` passed:

- `Pangea.ACF.Mining.PublicInteractionTaskContract`
- `Pangea.ACF.Mining.InteractionFactoryAndPreActivationCancel`
- `Pangea.ACF.Mining.PublicInteractionCompletesAndReclaims`
- `Pangea.ACF.Mining.InteractionInvalidationReleasesOnce`
- `Pangea.ACF.Mining.CoordinatorCancelsOrphanedInteractions`

Tests compile a transient minimal GameplayInteraction-schema StateTree through
the **public editor compiler**, register a transient Smart Object in an isolated
Editor world, and exercise the public task/context lifecycle. They avoid Game-world
input-preprocessor lifecycle and do not touch the live Editor level/world. No test
assets are imported or saved. `StateTreeEditorModule` and `PropertyBindingUtils`
are editor-target-only private dependencies for those fixtures, not shipping
runtime dependencies.
The tests are guarded for developer automation, Editor and Smart Object debug.

The corrected 16/16 mining and 45/45 full results supersede B05's earlier counts.
The initial focused run was 12/16; after fixing world type it was 15/16; after
exercising actual removal/invalidation it was 16/16. Historical failures and their
corrections remain in the reports rather than being discarded.

Additional gates before declaring the replacement production-ready: successful
navigation/arrival and no-path failures on a real navmesh, existing worker/guard
StateTrees and animations in PIE, AI resource-lock arbitration, cancellation while
moving/queued, owner teardown, repeated level/load/relevance transitions, packaged
build without editor modules and remote-server presentation. These are not
certified by the isolated stationary-context fixture.

Evidence under `Saved/Yarin`:
- `B06-Mining-Initial-20261020.json` — original fixture failures.
- `B06-Mining-FixtureRecovery-20261020.json` — 15/16 intermediate run.
- `B06-Mining-Focused-20261020.json` — final 16/16 focused results.
- `B06-PangeaRegression-20261020.json` — 45/45 results and all warnings.
- `B06-Activated-20261020.json`, `B06-LiveCoding-20261020.log` — build/patch evidence.
- `B06-MiningDefinition-Verified-20261020.json` — unchanged balance definition.

Earlier `B06-Pending-20261020.json`, validation manifest and UHT log are historical
staging evidence, not instructions to reapply the migration.
