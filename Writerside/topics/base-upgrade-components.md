# Base Upgrade Components

This page documents the core components used in the Base Upgrade System.

## UUpgradeSystemComponent

The heart of the upgrade system, managing level progression and milestone execution.

### Properties

#### Configuration

```cpp
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade")
UVillageDefinitionData* VillageDefinition;
```
**Master data asset** that defines all levels, milestones, and facilities for this village.

#### Runtime State

```cpp
UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Upgrade", SaveGame)
int32 CurrentLevel;
```
Current village/base level (0 = uninitialized).

```cpp
UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Upgrade", SaveGame)
FGameplayTagContainer CompletedMilestones;
```
Set of milestone tags that have been successfully executed.

### Core Functions

#### OnLevelIncreased
```cpp
UFUNCTION(BlueprintCallable, Category="Upgrade")
void OnLevelIncreased(int32 NewLevel, UObject* PlayerContext);
```

**Purpose**: Notifies the system that the village level has increased.

**Parameters**:
- `NewLevel`: The new level value
- `PlayerContext`: Typically the player pawn or controller, used by requirements to check inventory, quests, etc.

**Behavior**:
1. Updates `CurrentLevel` to `NewLevel`
2. Finds all milestones for that level
3. For each milestone, checks if requirements are met
4. If met, executes all actions and marks milestone as completed

**Usage Example**:
```cpp
// In your leveling system
void AMyGameMode::GrantBaseLevel(int32 NewLevel, APlayerController* Player)
{
    AVillageBase* Village = GetPlayerVillage(Player);
    if (Village && Village->UpgradeSystem)
    {
        Village->UpgradeSystem->OnLevelIncreased(NewLevel, Player->GetPawn());
    }
}
```

#### CanUpgradeToNextLevel
```cpp
UFUNCTION(BlueprintCallable, Category="Upgrade")
bool CanUpgradeToNextLevel(UObject* PlayerContext) const;
```

**Purpose**: Checks if all requirements for the next level are satisfied.

**Returns**: `true` if player can upgrade to `CurrentLevel + 1`.

**Usage Example**:
```cpp
// In UI widget
void UUpgradeMenuWidget::UpdateUpgradeButton()
{
    bool bCanUpgrade = UpgradeSystem->CanUpgradeToNextLevel(OwningPlayer);
    UpgradeButton->SetIsEnabled(bCanUpgrade);
}
```

#### IsMilestoneCompleted
```cpp
UFUNCTION(BlueprintCallable, Category="Upgrade")
bool IsMilestoneCompleted(FGameplayTag MilestoneTag) const;
```

**Purpose**: Check if a specific milestone has been completed.

**Usage Example**:
```cpp
if (UpgradeSystem->IsMilestoneCompleted(FGameplayTag::RequestGameplayTag("Village.Milestone.UnlockedBlacksmith")))
{
    // Blacksmith is available
}
```

### UI Helper Functions

#### GetNextLevelDefinition
```cpp
UFUNCTION(BlueprintCallable, Category="Upgrade|UI")
bool GetNextLevelDefinition(FUpgradeLevelDefinition& OutLevel) const;
```

Returns the definition for `CurrentLevel + 1`, useful for displaying "Next Level" info in UI.

#### GetMilestonesForLevel
```cpp
UFUNCTION(BlueprintCallable, Category="Upgrade|UI")
void GetMilestonesForLevel(int32 Level, TArray<FUpgradeMilestoneDefinition>& OutMilestones) const;
```

Retrieves all milestones for a specific level.

#### GetUnmetRequirementsForNextLevel
```cpp
UFUNCTION(BlueprintCallable, Category="Upgrade|UI")
void GetUnmetRequirementsForNextLevel(UObject* PlayerContext, TArray<UUpgradeRequirement*>& OutRequirements) const;
```

Returns a list of requirements that are **not yet met** for the next level, useful for displaying what the player still needs.

#### GetFacilitiesUnlockedAtLevel
```cpp
UFUNCTION(BlueprintCallable, Category="Upgrade|UI")
void GetFacilitiesUnlockedAtLevel(int32 Level, TArray<FGameplayTag>& OutFacilities) const;
```

Returns all facility tags that are unlocked at a specific level.

---

## UFacilityManagerComponent

Manages the enabled/disabled state of all facility groups in the base.

### Properties

```cpp
UPROPERTY()
TArray<FFacilityEntry> Facilities;
```

Internal storage tracking all discovered facilities and their state.

### Core Functions

#### EnableFacility / DisableFacility
```cpp
void EnableFacility(const FGameplayTag& FacilityTag);
void DisableFacility(const FGameplayTag& FacilityTag);
```

**Purpose**: Show or hide a facility by tag.

**Behavior**:
- Finds the `AFacilityGroup` with matching tag
- Calls `SetFacilityEnabled(bool)` on it
- Updates internal tracking

**Usage Example**:
```cpp
// Called by UA_EnableFacility action
void UA_EnableFacility::Execute_Implementation(UObject* ContextObject)
{
    AVillageBase* Village = Cast<AVillageBase>(ContextObject);
    if (Village && Village->FacilityManager)
    {
        Village->FacilityManager->EnableFacility(FacilityTag);
    }
}
```

#### IsFacilityEnabled
```cpp
bool IsFacilityEnabled(const FGameplayTag& FacilityTag) const;
```

**Purpose**: Check if a facility is currently enabled.

#### GetAllFacilities
```cpp
const TArray<FFacilityEntry>& GetAllFacilities() const;
```

**Purpose**: Retrieve all facilities for UI display or querying.

### Initialization

The component automatically discovers all `AFacilityGroup` actors under the owning actor during `BeginPlay()`:

```cpp
void UFacilityManagerComponent::BeginPlay()
{
    Super::BeginPlay();
    DiscoverFacilityGroups();
}
```

---

## UFacilitySlotComponent

A component attached to `AFacilityGroup` that defines what actor to spawn and where.

### Properties

```cpp
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Facility")
TSubclassOf<AActor> ActorClass;
```
The actor class to spawn (NPC, decoration, etc.).

```cpp
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Facility")
FTransform SpawnTransform;
```
Relative transform for the spawned actor.

```cpp
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Facility")
EFacilitySlotType SlotType;
```
Type of slot: NPC, Decoration, Interactable, or Custom.

### Usage

Facility slots are added to `AFacilityGroup` actors in the level editor:

1. Select your `FacilityGroup` actor
2. Add Component → `FacilitySlotComponent`
3. Configure `ActorClass` and `SpawnTransform`
4. The actor will be spawned when the facility group is enabled

---

## Integration Example

### Complete Setup Flow

```cpp
// 1. Create a village base in your level
AVillageBase* Village = World->SpawnActor<AVillageBase>();

// 2. Assign village definition
Village->UpgradeSystem->VillageDefinition = MyVillageData;

// 3. Create facility groups as children
AFacilityGroup* Blacksmith = World->SpawnActor<AFacilityGroup>();
Blacksmith->FacilityTag = FGameplayTag::RequestGameplayTag("Village.Facility.Blacksmith");
Blacksmith->AttachToActor(Village, FAttachmentTransformRules::KeepWorldTransform);

// 4. Add slots to facility group
UFacilitySlotComponent* SmithNPC = NewObject<UFacilitySlotComponent>(Blacksmith);
SmithNPC->ActorClass = ABlacksmithNPC::StaticClass();
SmithNPC->RegisterComponent();

// 5. On level up
Village->UpgradeSystem->OnLevelIncreased(2, PlayerPawn);
// This will check milestones for level 2 and unlock facilities
```

### Querying from UI

```cpp
// In your upgrade menu widget
void UVillageUpgradeMenuWidget::RefreshUpgradeInfo()
{
    // Check if can upgrade
    bool bCanUpgrade = UpgradeSystem->CanUpgradeToNextLevel(PlayerPawn);
    
    // Get next level info
    FUpgradeLevelDefinition NextLevel;
    if (UpgradeSystem->GetNextLevelDefinition(NextLevel))
    {
        // Display level number
        LevelText->SetText(FText::AsNumber(NextLevel.Level));
        
        // Get what's needed
        TArray<UUpgradeRequirement*> UnmetReqs;
        UpgradeSystem->GetUnmetRequirementsForNextLevel(PlayerPawn, UnmetReqs);
        
        // Display requirements
        for (UUpgradeRequirement* Req : UnmetReqs)
        {
            AddRequirementWidget(Req->GetRequirementDescription());
        }
    }
}
```

## See Also

- [Data Assets](base-upgrade-data-assets.md) - Configure upgrade definitions
- [Requirements & Actions](base-upgrade-requirements-actions.md) - Create custom logic
- [Integration Guide](base-upgrade-integration.md) - Connect to game systems

