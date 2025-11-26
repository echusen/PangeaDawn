# Base Upgrade System Integration

This guide shows how to integrate the Base Upgrade System with your game's other systems.

## Basic Integration

### Setting Up a Village

#### 1. Create Village Definition Data Asset

Create `DA_MainVillage` with your upgrade progression (see [Data Assets](base-upgrade-data-assets.md)).

#### 2. Place Village Actor in Level

```cpp
// In level or via Blueprint
AVillageBase* MainVillage = World->SpawnActor<AVillageBase>(VillageBaseClass);
MainVillage->UpgradeSystem->VillageDefinition = DA_MainVillage;
```

Or in Editor:
1. Drag `AVillageBase` into level
2. Set `Upgrade System` → `Village Definition` to your data asset

#### 3. Create Facility Groups

For each unlockable facility:
```cpp
AFacilityGroup* Blacksmith = World->SpawnActor<AFacilityGroup>();
Blacksmith->FacilityTag = FGameplayTag::RequestGameplayTag("Village.Facility.Blacksmith");
Blacksmith->AttachToActor(MainVillage, FAttachmentTransformRules::KeepWorldTransform);
```

Or in Editor:
1. Place `FacilityGroup` actors as children of your village
2. Set their `FacilityTag` property
3. Add `FacilitySlotComponent` for each actor to spawn

---

## Progression Integration

### Connecting to XP/Level System

When player gains a village level:

```cpp
void AMyPlayerController::OnVillageXPThresholdReached(int32 NewLevel)
{
    // Find player's village
    AVillageBase* PlayerVillage = GetPlayerVillage();
    if (!PlayerVillage)
        return;

    // Notify upgrade system
    APawn* PlayerPawn = GetPawn();
    PlayerVillage->UpgradeSystem->OnLevelIncreased(NewLevel, PlayerPawn);
    
    // Show notification
    ShowNotification(FText::Format(
        INVTEXT("Village upgraded to level {0}!"),
        FText::AsNumber(NewLevel)
    ));
}
```

### Alternative: Manual Upgrade Flow

If players manually trigger upgrades:

```cpp
bool AVillageBase::UpgradeBase(APawn* InstigatorPawn) const
{
    if (!UpgradeSystem)
        return false;

    // Check if can upgrade
    if (!UpgradeSystem->CanUpgradeToNextLevel(InstigatorPawn))
    {
        // Show why they can't
        return false;
    }

    // Consume resources (implement your logic here)
    ConsumeUpgradeResources(InstigatorPawn);

    // Upgrade
    int32 NewLevel = UpgradeSystem->CurrentLevel + 1;
    UpgradeSystem->OnLevelIncreased(NewLevel, InstigatorPawn);

    return true;
}
```

---

## UI Integration

### Upgrade Menu Widget

Create a widget to show upgrade information:

```cpp
UCLASS()
class UVillageUpgradeMenuWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintReadWrite, meta=(BindWidget))
    UTextBlock* CurrentLevelText;

    UPROPERTY(BlueprintReadWrite, meta=(BindWidget))
    UTextBlock* NextLevelText;

    UPROPERTY(BlueprintReadWrite, meta=(BindWidget))
    UButton* UpgradeButton;

    UPROPERTY(BlueprintReadWrite, meta=(BindWidget))
    UScrollBox* RequirementsList;

    UPROPERTY(BlueprintReadWrite, meta=(BindWidget))
    UScrollBox* RewardsList;

    UPROPERTY()
    UUpgradeSystemComponent* UpgradeSystem;

    UPROPERTY()
    APawn* PlayerPawn;

    virtual void NativeConstruct() override;
    void RefreshUpgradeInfo();

private:
    UFUNCTION()
    void OnUpgradeButtonClicked();
};
```

#### Implementation

```cpp
void UVillageUpgradeMenuWidget::RefreshUpgradeInfo()
{
    if (!UpgradeSystem || !PlayerPawn)
        return;

    // Display current level
    CurrentLevelText->SetText(FText::Format(
        INVTEXT("Level {0}"),
        FText::AsNumber(UpgradeSystem->CurrentLevel)
    ));

    // Get next level info
    FUpgradeLevelDefinition NextLevel;
    if (!UpgradeSystem->GetNextLevelDefinition(NextLevel))
    {
        // Max level reached
        NextLevelText->SetText(INVTEXT("Max Level"));
        UpgradeButton->SetIsEnabled(false);
        return;
    }

    NextLevelText->SetText(FText::Format(
        INVTEXT("Next: Level {0}"),
        FText::AsNumber(NextLevel.Level)
    ));

    // Show unmet requirements
    RequirementsList->ClearChildren();
    TArray<UUpgradeRequirement*> UnmetReqs;
    UpgradeSystem->GetUnmetRequirementsForNextLevel(PlayerPawn, UnmetReqs);

    for (UUpgradeRequirement* Req : UnmetReqs)
    {
        URequirementEntryWidget* Entry = CreateWidget<URequirementEntryWidget>(this, RequirementWidgetClass);
        Entry->SetRequirement(Req);
        Entry->SetIsMet(false);
        RequirementsList->AddChild(Entry);
    }

    // Show what you'll unlock
    RewardsList->ClearChildren();
    TArray<FGameplayTag> UnlockedFacilities;
    UpgradeSystem->GetFacilitiesUnlockedAtLevel(NextLevel.Level, UnlockedFacilities);

    for (const FGameplayTag& FacilityTag : UnlockedFacilities)
    {
        UFacilityUnlockEntryWidget* Entry = CreateWidget<UFacilityUnlockEntryWidget>(this, FacilityWidgetClass);
        Entry->SetFacility(FacilityTag);
        RewardsList->AddChild(Entry);
    }

    // Enable/disable upgrade button
    bool bCanUpgrade = UpgradeSystem->CanUpgradeToNextLevel(PlayerPawn);
    UpgradeButton->SetIsEnabled(bCanUpgrade);
}

void UVillageUpgradeMenuWidget::OnUpgradeButtonClicked()
{
    AVillageBase* Village = Cast<AVillageBase>(UpgradeSystem->GetOwner());
    if (Village)
    {
        Village->UpgradeBase(PlayerPawn);
        RefreshUpgradeInfo();
    }
}
```

### Requirement Display Widget

```cpp
UCLASS()
class URequirementEntryWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintReadWrite, meta=(BindWidget))
    UTextBlock* DescriptionText;

    UPROPERTY(BlueprintReadWrite, meta=(BindWidget))
    UImage* StatusIcon;

    void SetRequirement(UUpgradeRequirement* Requirement)
    {
        if (!Requirement)
            return;

        DescriptionText->SetText(Requirement->GetRequirementDescription());
    }

    void SetIsMet(bool bMet)
    {
        // Green checkmark if met, red X if not
        StatusIcon->SetBrushFromTexture(bMet ? CheckmarkTexture : CrossTexture);
    }
};
```

---

## Save System Integration

### Saving Village State

```cpp
USTRUCT()
struct FVillageSaveData
{
    GENERATED_BODY()

    UPROPERTY()
    int32 CurrentLevel = 0;

    UPROPERTY()
    FGameplayTagContainer CompletedMilestones;

    UPROPERTY()
    TArray<FGameplayTag> UnlockedFacilities;
};

void AMyGameMode::SaveVillageState(AVillageBase* Village)
{
    if (!Village)
        return;

    FVillageSaveData SaveData;
    SaveData.CurrentLevel = Village->UpgradeSystem->CurrentLevel;
    SaveData.CompletedMilestones = Village->UpgradeSystem->CompletedMilestones;

    // Save which facilities are unlocked
    for (const FFacilityEntry& Entry : Village->FacilityManager->GetAllFacilities())
    {
        if (Entry.bUnlocked)
        {
            SaveData.UnlockedFacilities.Add(Entry.FacilityTag);
        }
    }

    // Write to save game
    MySaveGame->VillageData = SaveData;
}
```

### Loading Village State

```cpp
void AMyGameMode::LoadVillageState(AVillageBase* Village)
{
    if (!Village)
        return;

    FVillageSaveData SaveData = MySaveGame->VillageData;

    // Restore level
    Village->UpgradeSystem->CurrentLevel = SaveData.CurrentLevel;
    Village->UpgradeSystem->CompletedMilestones = SaveData.CompletedMilestones;

    // Restore facilities
    for (const FGameplayTag& FacilityTag : SaveData.UnlockedFacilities)
    {
        Village->FacilityManager->EnableFacility(FacilityTag);
    }
}
```

### Using SaveGame Tags

The `UUpgradeSystemComponent` properties are marked with `SaveGame`:

```cpp
UPROPERTY(SaveGame)
int32 CurrentLevel;

UPROPERTY(SaveGame)
FGameplayTagContainer CompletedMilestones;
```

If using a save system that respects these tags (like ALS LoadAndSave), they'll be automatically saved.

---

## Inventory System Integration

### Custom Item Requirement

```cpp
bool UReq_HasItems::IsRequirementMet_Implementation(UObject* ContextObject) const
{
    APawn* Pawn = Cast<APawn>(ContextObject);
    if (!Pawn)
        return false;

    // Get ACF inventory component
    UACFInventoryComponent* Inventory = Pawn->FindComponentByClass<UACFInventoryComponent>();
    if (!Inventory)
        return false;

    // Check each required item
    for (const FRequiredACFItem& Item : RequiredItems)
    {
        int32 ActualCount = Inventory->GetTotalCountOfItemsByClass(Item.ItemClass);
        if (ActualCount < Item.Count)
        {
            return false;
        }
    }

    return true;
}
```

### Custom Grant Inventory Action

```cpp
void UUA_GrantInventory::Execute_Implementation(UObject* ContextObject)
{
    APawn* Pawn = Cast<APawn>(ContextObject);
    if (!Pawn)
        return;

    UACFInventoryComponent* Inventory = Pawn->FindComponentByClass<UACFInventoryComponent>();
    if (!Inventory)
        return;

    for (const FBaseItem& Item : ItemsToGrant)
    {
        Inventory->AddItemToInventory(Item, false); // false = don't auto-equip
    }
}
```

---

## Quest System Integration

### Quest Requirement

```cpp
UCLASS()
class UReq_QuestCompleted : public UUpgradeRequirement
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, Category="Requirement")
    FGameplayTag QuestTag;

    virtual bool IsRequirementMet_Implementation(UObject* ContextObject) const override
    {
        APawn* Pawn = Cast<APawn>(ContextObject);
        if (!Pawn)
            return false;

        APlayerController* PC = Cast<APlayerController>(Pawn->GetController());
        if (!PC)
            return false;

        // Get your quest manager
        UMyQuestManager* QuestManager = PC->FindComponentByClass<UMyQuestManager>();
        if (!QuestManager)
            return false;

        return QuestManager->IsQuestCompleted(QuestTag);
    }

    virtual FText GetRequirementDescription_Implementation() const override
    {
        return FText::Format(
            INVTEXT("Complete Quest: {0}"),
            FText::FromString(QuestTag.ToString())
        );
    }
};
```

---

## Event Broadcasting

### Listening to Upgrade Events

```cpp
void AMyGameMode::BeginPlay()
{
    Super::BeginPlay();

    // Find village
    AVillageBase* Village = FindVillageInWorld();
    if (Village && Village->UpgradeSystem)
    {
        // Bind to level changes
        Village->UpgradeSystem->OnLevelChanged.AddDynamic(this, &AMyGameMode::HandleVillageLevelChanged);
    }
}

void AMyGameMode::HandleVillageLevelChanged(int32 NewLevel)
{
    // Play fanfare
    PlayLevelUpEffect();

    // Notify players
    BroadcastNotification(FText::Format(
        INVTEXT("Village has reached level {0}!"),
        FText::AsNumber(NewLevel)
    ));

    // Achievement check
    CheckVillageLevelAchievements(NewLevel);
}
```

### Custom Broadcast Action

```cpp
UCLASS()
class UUA_BroadcastEvent : public UUpgradeAction
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, Category="Action")
    FGameplayTag EventTag;

    virtual void Execute_Implementation(UObject* ContextObject) override
    {
        AVillageBase* Village = Cast<AVillageBase>(ContextObject);
        if (!Village)
            return;

        UGameInstance* GI = Village->GetWorld()->GetGameInstance();
        if (UMyGameInstance* GameInstance = Cast<UMyGameInstance>(GI))
        {
            GameInstance->OnGameplayEvent.Broadcast(EventTag);
        }
    }
};
```

---

## Multiplayer Considerations

### Replication Setup

```cpp
// In AVillageBase
void AVillageBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    // Replicate upgrade system state
    DOREPLIFETIME(AVillageBase, UpgradeSystem);
}

// In UUpgradeSystemComponent
void UUpgradeSystemComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(UUpgradeSystemComponent, CurrentLevel);
    DOREPLIFETIME(UUpgradeSystemComponent, CompletedMilestones);
}
```

### Server-Authoritative Upgrades

```cpp
void AVillageBase::RequestUpgrade(APawn* InstigatorPawn)
{
    // Client requests upgrade
    if (!HasAuthority())
    {
        ServerRequestUpgrade(InstigatorPawn);
        return;
    }

    // Server processes
    UpgradeBase(InstigatorPawn);
}

void AVillageBase::ServerRequestUpgrade_Implementation(APawn* InstigatorPawn)
{
    UpgradeBase(InstigatorPawn);
}

bool AVillageBase::ServerRequestUpgrade_Validate(APawn* InstigatorPawn)
{
    // Prevent cheating
    return UpgradeSystem && UpgradeSystem->CanUpgradeToNextLevel(InstigatorPawn);
}
```

---

## Debugging Tools

### Console Commands

```cpp
// In your cheat manager or game mode
UFUNCTION(Exec, Category="Debug|Village")
void DebugSetVillageLevel(int32 Level)
{
    AVillageBase* Village = FindVillageInWorld();
    if (Village && Village->UpgradeSystem)
    {
        Village->UpgradeSystem->OnLevelIncreased(Level, GetPlayerPawn());
        UE_LOG(LogTemp, Log, TEXT("Village level set to %d"), Level);
    }
}

UFUNCTION(Exec, Category="Debug|Village")
void DebugUnlockAllFacilities()
{
    AVillageBase* Village = FindVillageInWorld();
    if (Village && Village->FacilityManager)
    {
        for (const FFacilityEntry& Entry : Village->FacilityManager->GetAllFacilities())
        {
            Village->FacilityManager->EnableFacility(Entry.FacilityTag);
        }
        UE_LOG(LogTemp, Log, TEXT("All facilities unlocked"));
    }
}

UFUNCTION(Exec, Category="Debug|Village")
void DebugPrintUpgradeInfo()
{
    AVillageBase* Village = FindVillageInWorld();
    if (!Village || !Village->UpgradeSystem)
        return;

    UE_LOG(LogTemp, Log, TEXT("Current Level: %d"), Village->UpgradeSystem->CurrentLevel);
    UE_LOG(LogTemp, Log, TEXT("Can Upgrade: %s"), 
        Village->UpgradeSystem->CanUpgradeToNextLevel(GetPlayerPawn()) ? TEXT("Yes") : TEXT("No"));

    TArray<UUpgradeRequirement*> UnmetReqs;
    Village->UpgradeSystem->GetUnmetRequirementsForNextLevel(GetPlayerPawn(), UnmetReqs);
    UE_LOG(LogTemp, Log, TEXT("Unmet Requirements: %d"), UnmetReqs.Num());
    
    for (UUpgradeRequirement* Req : UnmetReqs)
    {
        UE_LOG(LogTemp, Log, TEXT("  - %s"), *Req->GetRequirementDescription().ToString());
    }
}
```

---

## Best Practices

### Context Object Usage

The `ContextObject` parameter is flexible:
- For player-specific checks: Pass player pawn/controller
- For village-wide checks: Pass village actor itself
- Design requirements/actions to handle both cases

### Error Handling

Always validate:
```cpp
if (!Village || !Village->UpgradeSystem)
{
    UE_LOG(LogTemp, Error, TEXT("Invalid village setup"));
    return;
}
```

### Performance

- Cache component references
- Don't check requirements every frame
- Use events/delegates instead of polling

### Designer Workflow

- Provide clear documentation on requirement/action types
- Create example data assets
- Add helpful tooltips to properties
- Test with designers present

---

## See Also

- [Components Reference](base-upgrade-components.md) - Component API
- [Data Assets](base-upgrade-data-assets.md) - Configuration guide
- [Requirements & Actions](base-upgrade-requirements-actions.md) - Extension guide

