---
name: cinematic-camera
description: Drive cinematic and dialogue cameras, camera events, target locks, level sequences, and camera points with the ACF Cinematic Camera Manager (CCM) module.
globs: []
alwaysApply: false
---

# Cinematic Camera — ACF Ultimate

The **Cinematic Camera Manager (CCM)** replaces the default `APlayerCameraManager` with `ACCMPlayerCameraManager`, adding data-driven **camera events** (offset/FOV/shake presets from a DataTable), **target locking**, **camera point blending** (`UACFCameraPointComponent`), automated **movement presets** (orbit, crane, pan), and **gameplay level sequences** with runtime actor bindings. It is what powers dialogue framing, lock-on, and battle/cinematic cameras. Most gameplay calls go through the Blueprint function library `UCCMCameraFunctionLibrary`.

---

## 1 — Understand the assets

| Asset | Class | Location (sample) | Purpose |
|---|---|---|---|
| Camera manager | `ACCMPlayerCameraManager` | Set as `PlayerCameraManagerClass` on your PlayerController | Runtime camera brain (events, locks, sequences) |
| Camera movements table | `UDataTable` (row `FCCMCameraMovementSettings`) | `/AscentCombatFramework/.../Camera/` | Named offset/FOV/shake presets for camera events |
| Camera point | `UACFCameraPointComponent` | Added on actors (NPCs, arenas, dialogue rigs) | Tagged `UCineCameraComponent` you can blend to |
| Sequence binding | `FCCMSequenceBinding` | Built at call time | Binds tag names in a Level Sequence to runtime actors |

> **Never edit sample assets.** Duplicate the camera movements DataTable and any camera-rig Blueprints into `Content/YourGame/Camera/` and customize your copies.

### Key classes

| Class | Role |
|---|---|
| `ACCMPlayerCameraManager` | Custom `APlayerCameraManager`; runs camera events, locks, follow, sequences |
| `UCCMCameraFunctionLibrary` | Static Blueprint API — the main entry point for gameplay |
| `UACFCameraPointComponent` | `UCineCameraComponent` resolved by `CameraTag`, with movement presets |
| `FCCMCameraMovementSettings` | DataTable row: `CameraOffset`, `FOV`, `Shake`, interp speeds |
| `FACFCameraMovementConfig` | Per-point movement preset (`EACFCameraMovementPreset`: orbit/crane/pan/custom) |
| `FCCMSequenceBinding` | `{ TagName, BoundActor }` used by `PlayGameplaySequenceWithBinding` |
| `ELockType` | `EYawOnly` / `EAllAxis` lock behaviour |

---

## 2 — Setup / Configuration

1. Set your PlayerController's **`PlayerCameraManagerClass`** to `ACCMPlayerCameraManager` (or a Blueprint subclass of it). Duplicate the sample camera-manager Blueprint into `Content/YourGame/Camera/` if you customize it.
2. Duplicate the sample **Camera Movements DataTable** (row struct `FCCMCameraMovementSettings`) into `Content/YourGame/Camera/`, then assign it to **`CameraMovements`** on your camera manager.
3. Add rows to the DataTable — each **row name** is the `CameraEventName` you trigger at runtime. Per row set:
   - `CameraOffset` — positional offset applied to the camera.
   - `FOV` — additive field-of-view change.
   - `Shake` (+ `ShakeIntensity`, `bShakeLooping`) — `UCameraShakeBase` class.
   - `InterpSpeed` / `FovInterpSpeed` — blend smoothing.
4. For framed shots (dialogue, finishers, arena cams), add `UACFCameraPointComponent` to the relevant actor and set its **`CameraTag`** (e.g. `Camera.Dialogue.NPC`, `Camera.Battle.Attack1`) and optional **`MovementConfig`** preset.
5. Optional combat lock-on comfort settings on the camera manager:
   - **`MaxLockRotationRate`** — **0** (off) by default: the lock re-centers as fast as `lockStrength` asks. Set it to ~240 deg/s only if the camera whips too hard.
   - **`MinLockTargetDistance`** = **30** cm — skips the look-at when the target is on top of the camera (dodge-roll through the target), where `FindLookAtRotation` degenerates.

---

## 3 — Core Workflow / Runtime API

All functions below are static on `UCCMCameraFunctionLibrary` (category **CCM**) and need a World Context.

### Camera events (offset / FOV / shake presets)

```
// Apply a named preset until stopped:
UCCMCameraFunctionLibrary::TriggerCameraEvent(WorldContext, "CombatZoom");

// Apply for a fixed duration, then auto-revert:
UCCMCameraFunctionLibrary::TriggerTimedCameraEvent(WorldContext, "HitFocus", 0.5f);

// Stop a running event:
UCCMCameraFunctionLibrary::StopCameraEvent(WorldContext, "CombatZoom");

// Snap/blend back to the default camera:
UCCMCameraFunctionLibrary::ResetCameraPosition(WorldContext, /*bInstantReset*/ false);
```

### Target lock (lock-on)

The camera manager is the only thing that writes to the camera: gameplay code says who to look at and
how hard, never what to write. The lock and the framing below are **mutually exclusive** — starting one
gives up the other inside the manager — so they can never fight over the control rotation.

```
// Lock onto an actor (yaw only or all axes):
UCCMCameraFunctionLibrary::LockCameraOnActor(WorldContext, TargetActor, ELockType::EYawOnly, /*strength*/ 5.f);

// Lock onto a component (e.g. a bone/socket scene component):
UCCMCameraFunctionLibrary::LockCameraOnComponent(WorldContext, SceneComp, ELockType::EAllAxis, 5.f);

// Release the lock:
UCCMCameraFunctionLibrary::StopLockingCameraOnActor(WorldContext);
```

### Framing two actors (composition, not a lock)

This is **not** a lock: nothing is pulled to the center of the screen and neither actor has a favourite
side. The camera works — as hard as the strength you pass it — to keep both the player and the framed
actor inside `ScreenFraction` of the screen, asking each frame for **only** the angle that is missing,
the **spring arm** length that makes the pair fit, and the **sideways shift** that gives them a side of
the frame each. Once they are both in there is nothing left to do and the camera stays where the player
left it.

The sideways shift is what actually moves the *player* off center: the arm points at the player, so
turning alone can only walk the other actor across the frame. It fades out as the target comes back to
the middle, so it never snaps over when the target crosses in front.

The composition is a **parameter**, owned by whoever asks for the framing — the targeting component
holds it in `ActorFramingSettings` and hands it over on start:

```
FCCMActorFramingSettings Settings;
Settings.ScreenFraction = 0.66f;      // the middle two thirds is what they have to stay inside
Settings.MaxArmLength = 550.f;        // widest arm allowed; never narrower than the camera's own
Settings.MaxLateralOffset = 150.f;    // sideways room; 0 keeps the player centered
Settings.ArmInterpSpeed = 2.f;        // arm and sideways shift speed; low, or the camera pumps
Settings.bFramePitch = false;         // watch the vertical edges too; off leaves the pitch to the player

// The last argument is the same strength scale the camera lock uses: that is how hard it works.
UCCMCameraFunctionLibrary::StartFramingActors(WorldContext, TargetActor, Settings, 15.f);
UCCMCameraFunctionLibrary::StopFramingActors(WorldContext);
```

The framing's sideways shift is **added** to the camera events' own offset rather than replacing it, so
a `MoveCamera` event and the framing can both be running without overwriting each other.

`Get Framing Composition` on the manager returns the rotation, arm length and sideways offset the shot
asks for this frame, and `Get View Relative Angles` / `Get Framing Correction` on the function library
are the two pieces of maths behind it, so a Blueprint override of `Frame Actor Pair` can reuse them.

The targeting component drives this for you: set `TargetingType` to `EFrameBothActors` on
`UATSTargetingComponent` and it starts/stops with the target, replacing the magnetic lock rather than
adding to it. Override the `Frame Actor Pair` Blueprint event on the camera manager to compose the
shot your own way.

Characters are transparent to the spring arm's collision probe: `AACFCharacter` clears its Camera
channel response on capsule and mesh through `bIgnoreCameraCollisions`, and the manager forces the
arm's `ProbeChannel` to Camera when it takes the arm over (`bEnforceCameraProbeChannel`), so an enemy
crossing in front of the camera no longer pushes it in. Re-call `Apply Camera Collision Setup` on the
character if you assign a collision profile to its capsule or mesh at runtime, since a profile
replaces every response.

### Blending to a tagged camera point

Use camera points for dialogue and scripted shots. On the manager, `BlendToPoint` / `PlayCameraBlend` resolve a `UACFCameraPointComponent` by tag on a target actor:

```
ACCMPlayerCameraManager* Cam = UCCMCameraFunctionLibrary::GetLocalCinematicCameraManager(WorldContext);

// Blend to the camera point tagged "Camera.Dialogue.NPC" on NpcActor:
Cam->PlayCameraBlend(NpcActor, FGameplayTag::RequestGameplayTag("Camera.Dialogue.NPC"),
                     /*BlendTime*/ 1.f, VTBlend_Cubic, /*BlendExp*/ 2.f, /*bLockOutgoing*/ false);
```

### Gameplay level sequences with bindings

```
TArray<FCCMSequenceBinding> Bindings;
Bindings.Add(FCCMSequenceBinding("Player", PlayerActor));
Bindings.Add(FCCMSequenceBinding("Enemy",  EnemyActor));

UCCMCameraFunctionLibrary::PlayGameplaySequenceWithBinding(
    WorldContext, MyLevelSequence, PlayerActor->GetActorTransform(), Bindings);

// Stop it (only one sequence plays at a time):
UCCMCameraFunctionLibrary::StopSequence(WorldContext);
```

### Movement presets on a camera point

`UACFCameraPointComponent` can auto-move while active:

```
PointComp->SetMovementPreset(EACFCameraMovementPreset::RotateAroundOwner);
PointComp->SetMovementSpeed(20.f);   // deg/sec for orbit
PointComp->StopMovement();           // pause / ResumeMovement() / ResetMovement()
```

### Key delegates (on `ACCMPlayerCameraManager`)

| Delegate | Fires when |
|---|---|
| `OnCameraSequenceStarted (FName)` | A camera sequence begins |
| `OnCameraSequenceEnded (FName)` | A camera sequence ends |

---

## 4 — Wire to Characters / Blueprints

- **Dialogue cameras (most common):** place a `UACFCameraPointComponent` on the NPC (or dialogue rig), tag it (e.g. `Camera.Dialogue.NPC`). When dialogue starts, call `PlayCameraBlend`/`BlendToPoint` to that tag; on dialogue end, `ResetCameraPosition`. (See the `configure-dialogue` skill for the dialogue side.)
- **Combat / lock-on:** when the targeting system selects a target, call `LockCameraOnActor`; clear it with `StopLockingCameraOnActor` when the target is lost.
- **Hit feedback:** fire `TriggerTimedCameraEvent` on big hits/finishers for a quick zoom + shake.
- **Animation-driven shots:** the module ships `CCMStartSequenceNotify` / `CCMSequenceNotifyState` anim notifies — add them to montages to trigger camera events/sequences from animation.
- **Cutscenes:** drive `PlayGameplaySequenceWithBinding` with a `ULevelSequence`, binding scene tags to live actors so the same sequence works for any participants.

---

## 5 — Verify

**Checklist before testing:**

- [ ] `PlayerCameraManagerClass` on the PlayerController is `ACCMPlayerCameraManager` (or a subclass).
- [ ] `CameraMovements` DataTable assigned, with row names matching every `CameraEventName` you trigger.
- [ ] Camera points have a valid, unique `CameraTag` and exist on the actor you pass to the blend call.
- [ ] Level Sequence binding tag names match the `FCCMSequenceBinding.TagName` values you pass.
- [ ] Lock calls are paired with a `StopLockingCameraOnActor`.

**Common failures:**

| Symptom | Fix |
|---|---|
| `TriggerCameraEvent` does nothing | Row name not found in `CameraMovements`, or DataTable not assigned |
| Camera never reverts | Missing `StopCameraEvent` / `ResetCameraPosition`; or used `TriggerCameraEvent` (untimed) and never stopped it |
| Blend-to-point fails | `CameraTag` mismatch, or the point component isn't on the actor passed to the blend |
| Lock-on jitters / wrong axis | Wrong `ELockType`; tune `lockStrength` |
| Lock-on camera whip / motion sickness | Lower `lockStrength`, or set `MaxLockRotationRate` (off by default) to ~240 deg/s. For a fully custom feel, override the `Lock Camera On Position` event in a camera manager Blueprint. |
| Lock-on feels sluggish / lags behind the target | `MaxLockRotationRate` is capping the re-center speed — raise it or set it back to 0 (uncapped). |
| Camera never widens on far targets | Framing is off: set `TargetingType` to `EFrameBothActors`, or call `Start Framing Actors`. Check `MaxArmLength` is actually longer than the camera's own arm length |
| Framing lets the target slide off screen | The framing strength is too low to recover the angle in time, or `ScreenFraction` is so wide that the recovery only starts at the very edge |
| Framing keeps turning the camera on its own | `ScreenFraction` is too tight, so the pair is out almost always; raise it, and lower the strength to make the recovery gentler |
| Framing never moves the player off center | `MaxLateralOffset` is 0, or so small that the shift is invisible at the current arm length |
| Lock and framing seem to fight | They cannot: the manager keeps them exclusive. Something is re-calling the other one every frame instead |
| Camera jumps in whenever a character crosses in front of it | Something still blocks the spring arm's probe channel (`ECC_Camera`): a non-ACF pawn, an attached prop, or `bIgnoreCameraCollisions` turned off on `AACFCharacter` |
| Camera pumps in and out while circling a target | Lower `ArmInterpSpeed` in the manager's framing settings |
| Camera stays wide after the fight | Something started the framing without stopping it; `Stop Framing Actors` restores the arm length and the sideways shift (the targeting component does it on unlock) |
| Sequence actors not animating | `FCCMSequenceBinding.TagName` doesn't match the sequence's binding tracks |
| Two sequences fight | Only one gameplay sequence plays at once — starting a new one stops the previous; call `StopSequence` first if needed |
| `GetLocalCinematicCameraManager` returns null | Local PlayerController's camera manager isn't a `ACCMPlayerCameraManager` |
