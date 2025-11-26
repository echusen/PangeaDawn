# Base Upgrade Data Assets

This page details how to configure the data assets used by the Base Upgrade System.

## UVillageDefinitionData

The master data asset that defines everything about a village's upgrade progression.

### Creating a Village Definition

1. In Content Browser, right-click → Miscellaneous → Data Asset
2. Choose `VillageDefinitionData`
3. Name it (e.g., `DA_MainVillage_Upgrades`)

### Properties

#### VillageTag
```cpp
UPROPERTY(EditAnywhere, Category="Village")
FGameplayTag VillageTag;
```

**Purpose**: Unique identifier for this village (e.g., `Village.Main`, `Village.Outpost.North`).

**Usage**: Can be used to save/load village progress, or if your game has multiple villages.

#### FacilityGroups
```cpp
UPROPERTY(EditAnywhere, Category="Village")
TArray<FFacilityGroupReference> FacilityGroups;
```

**Purpose**: Declares all facilities that can be unlocked in this village.

**Structure**:
```cpp
struct FFacilityGroupReference
{
    FText FacilityGroupName;     // Display name (e.g., "Blacksmith")
    FGameplayTag FacilityTag;    // Unique tag (e.g., Village.Facility.Blacksmith)
};
```

**Example Configuration**:
| Facility Name | Facility Tag |
|--------------|--------------|
| Blacksmith | Village.Facility.Blacksmith |
| Alchemist | Village.Facility.Alchemist |
| Stable | Village.Facility.Stable |
| Training Grounds | Village.Facility.TrainingGrounds |

#### Levels
```cpp
UPROPERTY(EditAnywhere, Category="Upgrade System")
TArray<FUpgradeLevelDefinition> Levels;
```

**Purpose**: The core upgrade progression - defines what happens at each level.

---

## FUpgradeLevelDefinition

Defines a single level's milestones.

### Structure

```cpp
USTRUCT(BlueprintType)
struct FUpgradeLevelDefinition
{
    UPROPERTY(EditAnywhere)
    int32 Level;

    UPROPERTY(EditAnywhere)
    TArray<FUpgradeMilestoneDefinition> Milestones;
};
```

### Configuration

Each level entry should have:
- **Level**: Numeric level (1, 2, 3, etc.)
- **Milestones**: Array of milestone definitions

**Example**:
```
Level 1:
  - Milestone: "Basic Facilities"
  - Milestone: "Unlock Taming"
  
Level 2:
  - Milestone: "Advanced Facilities"
  - Milestone: "Unlock Breeding"
```

---

## FUpgradeMilestoneDefinition

A milestone is a set of requirements and actions that occur together.

### Structure

```cpp
USTRUCT(BlueprintType)
struct FUpgradeMilestoneDefinition
{
    UPROPERTY(EditAnywhere)
    FGameplayTag MilestoneTag;

    UPROPERTY(EditAnywhere)
    TArray<TObjectPtr<UUpgradeRequirement>> Requirements;

    UPROPERTY(EditAnywhere)
    TArray<TObjectPtr<UUpgradeAction>> Actions;
};
```

### Properties

#### MilestoneTag
**Purpose**: Unique identifier for this milestone.

**Naming Convention**: `Village.Milestone.{Description}`

**Examples**:
- `Village.Milestone.UnlockedBlacksmith`
- `Village.Milestone.UnlockedTaming`
- `Village.Milestone.GrantStarterGear`

#### Requirements
**Purpose**: Conditions that must be satisfied before actions execute.

**Common Patterns**:
- Empty requirements = always executes when level is reached
- Multiple requirements = ALL must be satisfied (AND logic)
- Mix requirement types for complex conditions

#### Actions
**Purpose**: What happens when requirements are met.

**Execution**: All actions execute in array order.

---

## Example Configuration

### Complete Village Definition

Here's a full example of how to configure a village with 3 levels:

#### Level 1: Basic Village

**Milestone 1: "Initial Setup"**
- **Tag**: `Village.Milestone.L1.InitialSetup`
- **Requirements**: None (auto-grants)
- **Actions**:
  - Enable Facility: `Village.Facility.Storage`
  - Grant Inventory: 100x Wood, 50x Stone

**Milestone 2: "Build Blacksmith"**
- **Tag**: `Village.Milestone.L1.BuildBlacksmith`
- **Requirements**:
  - Has Items: 200x Wood, 100x Stone, 50x Iron
- **Actions**:
  - Enable Facility: `Village.Facility.Blacksmith`

#### Level 2: Established Village

**Milestone 1: "Unlock Taming"**
- **Tag**: `Village.Milestone.L2.UnlockTaming`
- **Requirements**:
  - Facility Unlocked: `Village.Facility.Blacksmith`
  - Has Items: 500x Wood, 300x Stone
- **Actions**:
  - Enable Facility: `Village.Facility.Stable`
  - Enable Facility: `Village.Facility.TamingPen`

**Milestone 2: "Advanced Crafting"**
- **Tag**: `Village.Milestone.L2.AdvancedCrafting`
- **Requirements**:
  - Quest Completed: `Quest.Main.DiscoverForge`
- **Actions**:
  - Enable Facility: `Village.Facility.AdvancedForge`

#### Level 3: Thriving Village

**Milestone 1: "Breeding Program"**
- **Tag**: `Village.Milestone.L3.BreedingProgram`
- **Requirements**:
  - Facility Unlocked: `Village.Facility.Stable`
  - Has Items: 1000x Wood, 500x Stone
- **Actions**:
  - Enable Facility: `Village.Facility.BreedingFarm`
  - Enable Facility: `Village.Facility.Incubator`

---

## Configuration Workflow

### Step-by-Step Setup

#### 1. Plan Your Progression

Create a spreadsheet or document outlining:
- How many levels you want
- What unlocks at each level
- What players need to progress

#### 2. Create Gameplay Tags

In `Project Settings` → `GameplayTags`, add:
- Village tags (e.g., `Village.Main`)
- Milestone tags (e.g., `Village.Milestone.*`)
- Facility tags (e.g., `Village.Facility.*`)

#### 3. Create the Data Asset

- Create your `VillageDefinitionData` asset
- Set the `VillageTag`

#### 4. Add Facility References

In the `FacilityGroups` array, add an entry for each facility with:
- Display name for UI
- Gameplay tag for code reference

#### 5. Configure Each Level

For each level:
1. Add entry to `Levels` array
2. Set `Level` number
3. Add milestones to `Milestones` array

#### 6. Configure Each Milestone

For each milestone:
1. Set unique `MilestoneTag`
2. Add `Requirements` objects (see [Requirements & Actions](base-upgrade-requirements-actions.md))
3. Add `Actions` objects
4. Configure each requirement/action's properties

#### 7. Link to Village Actor

- Place or find your `AVillageBase` actor
- Set `VillageDefinition` reference to your data asset

#### 8. Test

- Play in editor
- Use console or debug functions to trigger level increases
- Verify facilities unlock correctly
- Check requirement logic

---

## Tips and Best Practices

### Milestone Design

**Keep Milestones Focused**
- Each milestone should have a clear purpose
- Don't mix unrelated unlocks in one milestone

**Use Descriptive Tags**
- Include level number in tag: `Village.Milestone.L2.UnlockTaming`
- Use consistent naming conventions

**Consider Dependencies**
- If Milestone B requires Milestone A to be completed, use `Req_MilestoneCompleted`

### Level Progression

**Balance Pacing**
- Don't front-load too much at level 1
- Spread interesting unlocks across levels
- Consider "reward" milestones with no requirements

**Clear Requirements**
- Make resource requirements visible to players
- Consider adding quest markers for milestone requirements

### Facility Organization

**Group Related Facilities**
- Put all taming-related facilities together in tag hierarchy
- Makes it easier to enable groups of related features

**Plan for Expansion**
- Leave room in your level structure for future content
- Use placeholder milestones if needed

---

## Common Patterns

### Auto-Grant Milestone
```
Requirements: (empty)
Actions: Grant items, unlock facility
```
Executes immediately when level is reached.

### Resource Gate
```
Requirements: Req_HasItems (Wood x500, Stone x200)
Actions: UA_EnableFacility
```
Player must gather resources to unlock.

### Quest Gate
```
Requirements: Req_QuestCompleted
Actions: UA_EnableFacility, UA_GrantInventory
```
Player must complete story content to unlock.

### Facility Dependency
```
Requirements: Req_FacilityUnlocked (Blacksmith)
Actions: UA_EnableFacility (AdvancedForge)
```
One facility requires another to be unlocked first.

### Milestone Chain
```
Requirements: Req_MilestoneCompleted
Actions: UA_EnableFacility
```
Create sequential unlocks within the same level.

---

## See Also

- [Components Reference](base-upgrade-components.md) - How the system executes these definitions
- [Requirements & Actions](base-upgrade-requirements-actions.md) - Available requirement and action types
- [Integration Guide](base-upgrade-integration.md) - Connecting to your game

