# Pangea Dawn Systems Documentation

**Latest 2026-10-22:** rebuilt Korro transport prototype wired and accepted in the
separate SC_Demo area. Prototype tiers 4/5 use the Pangea Korro draft variant at
450 cm/s. After fixing navmesh selection, CDO spawn dimensions and late duty-speed
initialization, transport passed **12/12** and broader ACF regression **83/83**,
zero errors (warnings remain). Vendor files stay unchanged/read-only. The human
leads on foot; towing is kinematic, not seated driving or trailer physics.
See the [current mining checkpoint](acf-mining-routines.md) for remaining limits.

**2026-10-21:** rebuilt convoy classes verified and the Pangea ALS restore
ordering bug fixed. Broader ACF regression passed 80/80; after creating the separate
hybrid prototype kit, transport/kit checks passed 10/10. Four prototype assets are
saved under `/Game/Yarin`; the existing demo profile remains legacy and unchanged.
The cart is cosmetic, not a working dinosaur chariot, and copied travel speeds
are unchanged. See the latest [mining checkpoint](acf-mining-routines.md) for setup,
warnings and remaining acceptance limits. Older checkpoints below are historical.

**2026-10-21 mining checkpoint:** Pangea-owned walking blendspace/moveset/AnimBP
copies are saved and pass the focused 2/2 animation checks (95 warnings).
Day-end unattended worker losses, persistent vacancies and paid replacement
source are prepared but **require a closed-Editor normal rebuild and acceptance**.
Replacement prices remain unconfigured. See the latest section of
[Mining ACF routines](acf-mining-routines.md); later counts below are historical.

Welcome to the comprehensive documentation for Pangea Dawn's core gameplay systems. This documentation covers the architecture, implementation details, and integration guides for the major systems powering the game's dinosaur management mechanics.

For the source-verified UE 5.8.3 / ACF 4.5 inventory, current tests and prioritized
upgrade work, see [ACF modernization: Phase 0 baseline](acf-modernization-baseline.md).
The legacy reference pages predate the current fragment and Game Feature setup;
use the baseline and current source when they disagree.

[B03: breeding persistence and replication](acf-breeding-persistence.md) uses the
selected pause-offline policy and passed the 25-test native Editor suite after rebuild.

[B04: base-upgrade safety](acf-base-upgrade-safety.md) is rebuilt and verified in
the 31-test native Editor suite, retaining separate explicit costs.

[B05: mining timing](acf-mining-timing.md) is rebuilt and verified with pause-offline
accounting: 11/11 focused mining tests and the full 40/40 native Editor suite pass
after the serialization Live Coding correction. Release gates remain separate.

[B06: mining public APIs](acf-mining-public-apis.md) is rebuilt and verified:
16/16 focused mining tests and the full 45/45 native Editor suite pass after
test-fixture corrections. Real navigation, packaging and multiplayer gates remain.

[Mining ACF routine bridge](acf-mining-routines.md) is rebuilt and wired through
Pangea-owned actions, setup/preview components, controller copies and skill UI.
All vendor source, campfire-task and remaining sample edits are restored to stock;
the plugin trees and FullSample submodule are Git-clean. **8,136 authored vendor
files are filesystem read-only**, checked by `Scripts/Protect-PangeaVendor.ps1`.
Pangea asset checksums were unchanged by rollback. New ACF-derived work belongs
in Pangea-named copies/children, never originals (root `AGENTS.md`).
The latest Pangea-only reliability slice passed **63/63 Pangea.ACF tests twice
consecutively** (99.64 s / 102.53 s; 1,219 warnings each; 30 mining cases).
It fixes cross-site cleanup deleting nearby foreign actors and cancels latent
animation initialization when retiring freshly spawned NPCs. New real ALS
in-memory active-duty round trips verify non-empty inventory, repeated restore,
old-job cleanup, role counts and resumed work. The animation fixture protects
its observer before warmup so player death cannot open a pausing menu mid-test;
NPC movement/station assertions remain intact. A Pangea-owned mining routine
controller copy is selected, with all other presentation settings unchanged.
The earlier 60/61 timeout and 61/61 reports are historical, not current acceptance.
Function-only changes are Live Coding verified; include them in the next normal
C++ build before relying on a restarted Editor. Slow-looking walking animation
tuning is explicitly deferred until last at the user's request.
Corrected economy, authored preview settings and pause-offline policy remain.
Historical vendor-patched 60/60 reports are superseded. Dedicated server, late
join/relevance, latency/loss, disk/new-session active-duty save/load, persistent
workforce/death/recruitment, physical cargo/vehicle/escort, further path/combat,
simultaneous multi-site navigation and packaging remain gates. The worker-loss
sampling period and recruitment pricing have not been silently chosen.

## System Overview

Pangea Dawn features four interconnected systems that work together to create a rich dinosaur taming and base-building experience:

### 1. Base Upgrade System
The **PangeaBaseUpgradeSystem** manages player base progression, facility unlocking, and upgrade requirements. It provides a data-driven approach to managing village/base levels, milestones, and the unlock progression of various facilities.

**Key Features:**
- Data-driven upgrade definitions via `UVillageDefinitionData`
- Flexible requirement and action system
- Facility management with enable/disable functionality
- Milestone tracking with gameplay tag integration
- UI helper functions for displaying upgrade information

[Learn more about the Base Upgrade System →](base-upgrade-system.md)

### 2. Breeding System
The **PangeaBreedingSystem** handles dinosaur breeding, genetics, and egg incubation. It implements a comprehensive genetic inheritance system with visual trait mixing and stat inheritance.

**Key Features:**
- Genetic trait inheritance system
- Material parameter blending for visual inheritance
- Farm-based breeding mechanics with breeding zones
- Egg incubation with progress tracking
- Fertility cooldown management
- Flexible genetic strategy system

[Learn more about the Breeding System →](breeding-system.md)

### 3. Taming System
The **PangeaTamingSystem** implements the mechanics for taming wild dinosaurs, managing tame states, and assigning roles to tamed creatures.

**Key Features:**
- State-based taming (Wild, Hostile, Tamed)
- Role assignment (Mount, Companion)
- Stat and item requirements for taming
- Minigame integration support
- Team switching on tame
- GAS (Gameplay Ability System) integration
- AI controller switching based on tame state and role

[Learn more about the Taming System →](taming-system.md)

### 4. Dinosaur AI
The **PangeaDinosaurAI** system provides the base character class for all dinosaurs in the game, integrating taming, breeding, and mounting systems.

**Key Features**:
- Integration point for all dinosaur-related systems
- Mount component support
- Vaulting component for obstacle traversal
- Save/load functionality
- Interaction interface implementation

[Learn more about the Dinosaur AI →](dinosaur-ai.md)

## Architecture Philosophy

These systems are designed with the following principles:

### Data-Driven Design
Most gameplay configuration is done through Data Assets, minimizing the need for C++ changes when balancing or adding new content.

### Component-Based Architecture
Systems are implemented as Actor Components that can be added to any actor, promoting reusability and flexibility.

### Blueprint-Friendly
All systems expose Blueprint events, functions, and properties, allowing designers to customize behavior without C++ knowledge.

### GAS Integration
Where applicable, systems integrate with Unreal's Gameplay Ability System for consistent gameplay mechanics.

### Modular Design
Each system is a separate Unreal module, allowing for clear dependencies and potential reuse in other projects.

## Getting Started

For programmers and technical designers working with these systems, we recommend:

1. Start with the [Base Upgrade System](base-upgrade-system.md) to understand the progression framework
2. Explore the [Taming System](taming-system.md) to understand creature state management
3. Review the [Breeding System](breeding-system.md) for genetics and reproduction mechanics
4. Finally, examine the [Dinosaur AI](dinosaur-ai.md) to see how everything comes together

## Dependencies

These systems depend on several third-party plugins:
- **ACF (Ascent Combat Framework)**: Provides character base classes, inventory, mounting, and interaction systems
- **ALS (Advanced Locomotion System)**: Character movement and animation
- **GAS (Gameplay Ability System)**: Ability and attribute management

## Module Structure

Each module follows the standard Unreal Engine plugin/module structure:

```
ModuleName/
├── Public/
│   ├── Components/       # Actor components
│   ├── Actors/          # Actor classes
│   ├── DataAssets/      # Configuration data assets
│   ├── Objects/         # UObject-based classes
│   ├── UI/              # Widget classes
│   └── ModuleName.h     # Module header
├── Private/
│   └── *.cpp            # Implementation files
└── ModuleName.Build.cs  # Build configuration
```

## Support and Contributing

This documentation is maintained alongside the source code. If you find inaccuracies or need clarification on any system, please refer to the source code or consult with the development team.

---

**Last Updated:** November 2025  
**Engine Version:** Unreal Engine 5.x

