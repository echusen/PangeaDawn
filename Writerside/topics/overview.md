# Pangea Dawn Systems Documentation

Welcome to the comprehensive documentation for Pangea Dawn's core gameplay systems. This documentation covers the architecture, implementation details, and integration guides for the major systems powering the game's dinosaur management mechanics.

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

