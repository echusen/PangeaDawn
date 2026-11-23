---
name: targeting-system
description: Set up lock-on / soft targeting in ACF — configure the targeting component, target points, filters and camera magnetism with the Ascent Targeting System (ATS).
globs: []
alwaysApply: false
---

# Ascent Targeting System (ATS) — ACF Ultimate

The **AscentTargetingSystem** module gives the player (and AI) soft-lock / hard-lock targeting. The player uses `UATSTargetingComponent`, which scans for nearby actors within a distance/angle cone, picks the best candidate (nearest, or closest to forward), and optionally locks the camera onto it with configurable magnetism. Targetable actors expose `IATSTargetableInterface` and one or more `UATSTargetPointComponent`s (the precise points the camera/reticle snaps to, e.g. head/torso). Targeting can be narrowed with `UATSTargetingFilter` objects. AI uses the sibling `UATSAITargetComponent`. This skill covers configuring and wiring lock-on targeting.

---

## 1 — Understand the assets

| Asset | Class | Location (sample) | Purpose |
|---|---|---|---|
| Player pawn with targeting | `ACharacter` + `UATSTargetingComponent` | `/Game/FullSample/` player pawn | Drives target search, lock and camera control |
| Targetable enemy | `AACFCharacter` implementing `IATSTargetableInterface` | `/Game/FullSample/` NPCs | Can be locked; reacts on acquire/lose |
| Target points | `UATSTargetPointComponent` | added to the enemy mesh sockets | Snap points (head/torso/etc.) with optional camera event |
| Targeting filter | `UATSTargetingFilter` (BP subclass) | sample targeting config | Custom rule to include/exclude potential targets |

> **Never edit sample assets.** Duplicate the sample player pawn / filter Blueprints into your own folder (e.g. `Content/YourGame/Targeting/`) and configure **your copies**. Editing the originals = lost on update.

### Key classes

| Class | Role |
|---|---|
| `UATSTargetingComponent` | Player lock-on: search, switch, camera magnetism, line-of-sight checks |
| `UATSBaseTargetComponent` | Shared base for targeting components (current target storage) |
| `UATSAITargetComponent` | AI variant used by `AACFAIController` |
| `UATSTargetPointComponent` | A lockable point on a target (extends look-at point); exposes a camera event name |
| `IATSTargetableInterface` | Interface targets implement: `OnTargetAcquired`, `OnTargetLosed` |
| `UATSTargetingFilter` | `IsActorTargetable(owner, target)` rule to filter candidates |
| `UATSTargetingFunctionLibrary` | Blueprint helpers for targeting queries |

### Important enums

- `ETargetingType`: `EDontLock` (no camera change), `EMagneticLock` (smooth lock), `EMagneticLockYawOnly` (yaw-only lock).
- `ETargetSelectionType`: `ENearestTarget` (lowest distance), `EForwardTarget` (closest to forward vector).
- `ETargetingDirection`: `ELeft`, `ERight`, `EUp`, `EDown` (for switching targets via input).

---

## 2 — Setup / Configuration

### A. Add the targeting component to the player

1. Open your **duplicated** player pawn Blueprint (`Content/YourGame/Targeting/`).
2. Add a `UATSTargetingComponent`.
3. Configure detection:
   - `MaxTargetingDistance` (default 1500) — how far targets can be acquired.
   - `MaxAngularDistanceDegree` (default 60) — half-angle of the search cone in front of the player.
   - `TargetSelectionType` — `ENearestTarget` or `EForwardTarget`.
   - `ObjectsToQuery` — the object types (e.g. Pawn) the sweep checks.
4. Configure the camera behaviour:
   - `TargetingType` — the camera behaviour: `EDontLock`, `EMagneticLock`, `EMagneticLockYawOnly`,
     or `EFrameBothActors`, which replaces the lock with a composition: nothing is centered, the
     camera works to keep both inside a fraction of the screen, and either of them can be on either
     side.
   - `LockMagnetism` (default 15) — how hard the camera works: magnetism for the lock types, recovery
     strength for the framing type. Greyed out for `EDontLock`.
   - `ActorFramingSettings` — greyed out unless `EFrameBothActors`: `ScreenFraction` (how much of the
     screen the pair may use), `MaxArmLength`, `MaxLateralOffset` (how far the camera may slide
     sideways to give them a side of the frame each), `ArmInterpSpeed`, `bFramePitch`. Handed to the
     camera manager as a parameter when the framing starts.
   - `BottomPitchLimitDegree` / `UpperPitchLimitDegree` — pitch clamps to avoid gimbal lock.
   - `bStopTargetingIfOutOfSight` — drop the target if line of sight is blocked.
   - `InputTrasholdForSearch` (default 0.7) — stick deflection needed to switch targets via input.

### B. Make enemies targetable

1. On your enemy actor, **implement `IATSTargetableInterface`** (Blueprint: Class Settings → Implemented Interfaces).
2. Add one or more `UATSTargetPointComponent`s attached to mesh sockets (e.g. `spine_03` for torso, `head`). Each point can set a `CameraEvent` name to trigger a camera tweak when that point is locked.
3. Implement `OnTargetAcquired(point)` / `OnTargetLosed()` to show/hide a lock reticle or play feedback.

### C. (Optional) Create targeting filters

1. Create a Blueprint subclass of `UATSTargetingFilter`.
2. Override `IsActorTargetable(componentOwner, Target)` to return `false` for actors you never want to lock (dead, friendly team, stealthed, etc.).
3. Add it to the component's `TargetFilters` array (instanced), or add/remove at runtime (see below).

---

## 3 — Core Workflow / Runtime API

### Activating targeting (`UATSTargetingComponent`)

```
Targeting->TriggerTargeting(true);    // start lock-on
Targeting->TriggerTargeting(false);   // stop
Targeting->ToggleTargeting();         // flip current state
bool bOn = Targeting->IsTargetingEnabled();
```

### Switching targets with input

```
// Bind to right-stick axes; the component uses InputTrasholdForSearch as the deadzone:
Targeting->RightSearchTargetWithInput(AxisX);   // switch left/right
Targeting->UpSearchTargetWithInput(AxisY);      // switch up/down
```

### Reading / forcing the current target

```
AActor* current = Targeting->GetCurrentTarget();   // from UATSBaseTargetComponent
Targeting->SetCurrentTarget(SpecificEnemy);        // force a target
```

### Framing the player and the target together

```
Targeting->SetTargetingType(ETargetingType::EFrameBothActors); // applies immediately
bool bOn = Targeting->GetIsFramingBothActors();
Targeting->SetActorFramingSettings(Settings);  // recomposes the shot mid-fight
```

It starts and stops with the target and takes the place of the magnetic lock: nothing gets centered,
the camera only recovers the angle needed to keep both inside the screen and slides sideways to give
them a side of the frame each, from whichever side they happen to be. Body rotation and aim offset
still point at the target, so only the camera behaves differently.

Whatever the targeting type, this component makes exactly **one** call into the camera manager —
`Start Framing Actors` with its own `ActorFramingSettings` for the framing type, `Lock Camera On
Actor` / `On Component` for the lock types — and never touches the camera itself. The manager keeps the
two mutually exclusive, so they can never overwrite each other. Compose the shot your own way by
overriding `Frame Actor Pair` on the camera manager; see `skill://cinematic-camera`.

### Runtime tuning

```
Targeting->SetMaxTargetingDistance(2000.f);
Targeting->SetMaxAngularDistanceDegree(45.f);
Targeting->SetTargetingType(ETargetingType::EMagneticLockYawOnly);
Targeting->SetLockMagnetism(20.f);
Targeting->SetTargetSelectionType(ETargetSelectionType::EForwardTarget);
```

### Filters & object types at runtime

```
Targeting->AddFilter(UMyDeadEnemyFilter::StaticClass());
Targeting->RemoveFilter(UMyDeadEnemyFilter::StaticClass());
Targeting->AddObjectType(ObjectTypeQuery_Pawn);
Targeting->RemoveObjectType(ObjectTypeQuery_Pawn);
```

### Body & aim orientation while locked

While lock-on is engaged the character faces the **target**, not the camera. The camera magnetism alone
is not enough: it puts the target near the screen center, not exactly under it.

| Blueprint node / property | Where | What it does |
|---|---|---|
| `Is Lock On Active` | `UATSBaseTargetComponent` | True only while a lock-on mode is engaged. Having a target is not the same thing — AI combat targets and scripted `SetCurrentTarget` set a target with no lock-on |
| `Get Current Target Point Location` | `UATSBaseTargetComponent` | Live world position of the locked target point. Game Thread only — from an Animation Blueprint use the anim instance nodes below instead |
| `Get Look At Location` | `UATSBaseTargetComponent` | World location a look-at should point at, with optional eye-height correction (`bAimAtTargetEyeHeight`). Game Thread only. This is what the ACF Look At Target node reads |
| `Find Target Component For Actor` | `UATSBaseTargetComponent` | Finds the targeting component on the possessing controller, then on the actor itself |
| `bOrientToLockOnTarget` | `UACFCharacterMovementComponent` | **On by default.** Turns the body towards the locked target instead of the control rotation. Turn speed is the usual `RotationRate.Yaw` |
| `Is Orienting To Lock On Target` | `UACFCharacterMovementComponent` | True while that rotation is actually being applied — the node to check when the body is not turning |
| `bIsLockedOnTarget` | `UACFAnimInstance` | True while the aim offset is aimed at the target instead of the camera direction |
| `Get Aim Rotation` | `UACFAnimInstance` | **The node to build a Blueprint aim offset from.** World rotation with the lock-on already resolved: eyes-to-target while locked, control rotation (or replicated base aim rotation on proxies) otherwise. Thread safe |
| `Get Lock On Aim Rotation` | `UACFAnimInstance` | Only the lock-on half, for graphs that want their own camera fallback. Returns false when no lock-on is engaged |
| `Get Target Look At Location` | `UACFAnimInstance` | Thread-safe target position for aim / look-at logic, refreshed every frame on the Game Thread. Returns false with no target, so keep a fallback |

The **aim offset is the mechanism that points the character at the opponent**. While locked,
`AimOffset` is the delta between the eyes-to-target direction and the pose, so it also covers the yaw
the body has not caught up with yet.

AnimBPs that build their own aim offset in Blueprint (GASP-style, converting a rotation into X/Y
relative to the **root/mesh** transform rather than the actor) should feed that conversion with
**`Get Aim Rotation`** instead of `Is Locally Controlled ? Get Control Rotation : Get Base Aim
Rotation`. Same fallback when unlocked, target-based rotation when locked, and thread safe. Keep your
own conversion node: ACF's C++ `AimOffset` is relative to the actor rotation, which with orientation
warping is deliberately not the root rotation.

On top of that you can add the **ACF Look At Target** node to bend a single bone (head, neck, upper
spine) straight at the target. It does **not** require a `UACFAnimInstance`: `PreUpdate` reads
`Get Look At Location` from the targeting component on the owner's controller (or the owner itself),
so it works on any AnimBP, including linked moveset/overlay layers. Put it between `Local To Component`
and `Component To Local`, set **Bone To Modify** in the details panel — with no bone the node is
skipped — and leave the hidden `Owner` pin empty: it defaults to the actor the mesh belongs to.

The body rotation needs `RotationMode = EStrafing`; in forward-facing mode the character keeps orienting to
its movement direction and lock-on never touches it. Toggle the strafe mode on `OnTargetingStateChanged`.

### Delegates

| Delegate | When it fires |
|---|---|
| `OnTargetingStateChanged(bIsTargeting)` | Lock-on turned on/off — toggle UI/movement mode |
| `OnTargetChanged(targetActor)` | The locked target switched — move the reticle |

---

## 4 — Wire to Characters / Blueprints

1. **Input → component**: bind your lock button to `ToggleTargeting()` (or hold = `TriggerTargeting(true/false)`), and the right-stick / look axes to `RightSearchTargetWithInput` / `UpSearchTargetWithInput`.
2. **Camera control**: with `EMagneticLock` the component interpolates the camera toward the locked target point using `LockMagnetism`. Use `EMagneticLockYawOnly` for action games that keep player pitch control. The `UATSTargetPointComponent.CameraEvent` lets specific points drive a custom camera modifier.
3. **Reticle UI**: listen to `OnTargetChanged` to reposition a lock-on widget over `GetCurrentTarget()`, and to `OnTargetingStateChanged` to show/hide it.
4. **Movement mode**: many games switch the character to strafe/orient-to-target movement while locked — toggle that on `OnTargetingStateChanged`.
5. **AI**: AI targeting uses `UATSAITargetComponent`, created/owned by `AACFAIController`; the controller's `GetTarget()` / `SetTarget()` read and write through it. You normally don't configure the player `UATSTargetingComponent` on NPCs.
6. **Combat integration**: ACF actions/abilities can read the current target from the targeting component to orient attacks toward the locked enemy.

---

## 5 — Verify

**Checklist before testing:**

- [ ] Player pawn is a **duplicate** with a `UATSTargetingComponent` added.
- [ ] `ObjectsToQuery` includes the object type your enemies use (e.g. Pawn).
- [ ] Enemies **implement `IATSTargetableInterface`** and have at least one `UATSTargetPointComponent`.
- [ ] `MaxTargetingDistance` and `MaxAngularDistanceDegree` are large enough for your encounters.
- [ ] Input is bound to `ToggleTargeting`/`TriggerTargeting` and the search-by-direction functions.
- [ ] A lock reticle is wired to `OnTargetChanged` / `OnTargetingStateChanged`.
- [ ] Any custom `UATSTargetingFilter`s are added to `TargetFilters` (or at runtime).

**Common failures:**

| Symptom | Fix |
|---|---|
| Nothing gets targeted | Enemy missing `IATSTargetableInterface` or `UATSTargetPointComponent`, or `ObjectsToQuery` doesn't include its object type |
| Target found but camera doesn't lock | `TargetingType = EDontLock`, or `LockMagnetism = 0` |
| Camera snaps too hard / jitters | Lower `LockMagnetism`, or use `EMagneticLockYawOnly` |
| Can't switch targets with the stick | `InputTrasholdForSearch` too high, or input not bound to `RightSearchTargetWithInput`/`UpSearchTargetWithInput` |
| Locks onto dead/friendly actors | Add a `UATSTargetingFilter` that rejects them via `IsActorTargetable` |
| Target stays locked behind walls | Enable `bStopTargetingIfOutOfSight` |
| Out-of-range enemies get locked | Lower `MaxTargetingDistance` / `MaxAngularDistanceDegree` |
| Body doesn't turn to the target | `RotationMode` is not `EStrafing`, or `bOrientToLockOnTarget` is off on the movement component. Check `Is Orienting To Lock On Target` |
| Body turns to the target too slowly/fast | Tune `RotationRate.Yaw` on the character movement component |
| Character aims at the camera center instead of the enemy | `Is Lock On Active` is false — the targeting component has a target but no lock-on mode engaged |
| Character aims down at the enemy's feet while locked | `bAimAtTargetEyeHeight` is off on the targeting component (`UATSBaseTargetComponent`), so the aim ray runs from the eyes down to the target point (authored for the camera, usually the chest). The tilt gets steeper the closer the opponent is |
| ACF Look At Target node does nothing at all | `Bone To Modify` is empty, or the node sits outside a `Local To Component` → `Component To Local` pair. It no longer needs an ACF AnimInstance or the `Owner` pin wired |
| Head bends but not towards the enemy | The node follows the ATS target, not the lock: with no targeting component or no target it looks at the actor forward vector plus `NoTargetOffset`. Check `Get Look At Location` on the targeting component returns true |
