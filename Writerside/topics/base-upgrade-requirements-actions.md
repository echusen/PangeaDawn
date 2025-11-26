# Requirements and Actions

This page documents the built-in requirement and action types, and how to create custom ones.

## Built-In Requirements

### UUpgradeRequirement (Base Class)

All requirements inherit from this abstract base class.

```cpp
UCLASS(Abstract, Blueprintable, EditInlineNew, DefaultToInstanced)
class UUpgradeRequirement : public UObject
{
    UFUNCTION(BlueprintNativeEvent)
    bool IsRequirementMet(UObject* ContextObject) const;
    
    UFUNCTION(BlueprintNativeEvent)
    FText GetRequirementDescription() const;
    
    UFUNCTION(BlueprintCallable)
    virtual FText GetFailureMessage() const;
};
```

### Req_HasItems

Checks if the player has specific items in their inventory.

**Properties**:
```cpp
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Requirement")
TArray<FRequiredACFItem> RequiredItems;

// Where FRequiredACFItem is:
USTRUCT(BlueprintType)
struct FRequiredACFItem
{
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TSubclassOf<UACFItem> ItemClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 Count = 1;
};
```

**Usage Example**:
Add entries to the `RequiredItems` array:
- Entry 0: ItemClass = Wood, Count = 100
- Entry 1: ItemClass = Stone, Count = 50
- Entry 2: ItemClass = Iron, Count = 25

**Implementation Notes**:
- Integrates with ACF inventory system
- Checks player pawn's inventory component
- Returns false if player doesn't have all required items

### Req_QuestCompleted

Checks if a specific quest has been completed.

**Properties**:
```cpp
UPROPERTY(EditAnywhere)
FGameplayTag QuestTag;
```

**Usage Example**:
- Quest Tag: `Quest.Main.DiscoverBlacksmith`

**Implementation Notes**:
- Requires integration with your quest system
- Override in Blueprint or C++ to check your quest manager

### Req_MilestoneCompleted

Checks if another milestone has been completed.

**Properties**:
```cpp
UPROPERTY(EditAnywhere)
FGameplayTag MilestoneTag;
```

**Usage Example**:
- Milestone Tag: `Village.Milestone.L1.BuildBlacksmith`

**Implementation Notes**:
- Queries the `UUpgradeSystemComponent` directly
- Useful for creating milestone dependencies

### Req_FacilityUnlocked

Checks if a specific facility is currently unlocked.

**Properties**:
```cpp
UPROPERTY(EditAnywhere)
FGameplayTag FacilityTag;
```

**Usage Example**:
- Facility Tag: `Village.Facility.Blacksmith`

**Implementation Notes**:
- Queries `UFacilityManagerComponent`
- Allows creating facility dependency chains

---

## Built-In Actions

### UUpgradeAction (Base Class)

All actions inherit from this abstract base class.

```cpp
UCLASS(Abstract, Blueprintable, EditInlineNew, DefaultToInstanced)
class UUpgradeAction : public UObject
{
    UFUNCTION(BlueprintNativeEvent)
    void Execute(UObject* ContextObject);
};
```

### UA_EnableFacility

Enables a facility, making it visible and functional.

**Properties**:
```cpp
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Action")
FGameplayTag FacilityTag;
```

**Usage Example**:
- Facility Tag: `Village.Facility.Blacksmith`

**Implementation**:
```cpp
void UA_EnableFacility::Execute_Implementation(UObject* ContextObject)
{
    AVillageBase* Village = Cast<AVillageBase>(ContextObject);
    if (Village && Village->FacilityManager)
    {
        Village->FacilityManager->EnableFacility(FacilityTag);
    }
}
```

### UA_GrantInventory

Grants items to the player's inventory.

**Properties**:
```cpp
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Action")
TArray<FBaseItem> ItemsToGrant;

// Where FBaseItem is from ACF:
USTRUCT(BlueprintType)
struct FBaseItem
{
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    TSubclassOf<UACFItem> ItemClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    int32 Count = 1;
};
```

**Usage Example**:
Add entries to the `ItemsToGrant` array:
- Entry 0: ItemClass = Wood, Count = 100
- Entry 1: ItemClass = Gold, Count = 50

**Implementation Notes**:
- Integrates with ACF inventory system
- Adds items to the player's inventory component

---

## Creating Custom Requirements

### C++ Implementation

#### 1. Create Header File

```cpp
// Req_PlayerLevel.h
#pragma once

#include "CoreMinimal.h"
#include "Objects/UpgradeRequirement.h"
#include "Req_PlayerLevel.generated.h"

UCLASS()
class PANGEABASEUPGRADESYSTEM_API UReq_PlayerLevel : public UUpgradeRequirement
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Requirement")
    int32 MinimumPlayerLevel = 1;

    virtual bool IsRequirementMet_Implementation(UObject* ContextObject) const override;
    virtual FText GetRequirementDescription_Implementation() const override;
};
```

#### 2. Implement Logic

```cpp
// Req_PlayerLevel.cpp
#include "Req_PlayerLevel.h"
#include "GameFramework/Character.h"
#include "YourPlayerCharacter.h"

bool UReq_PlayerLevel::IsRequirementMet_Implementation(UObject* ContextObject) const
{
    AYourPlayerCharacter* Player = Cast<AYourPlayerCharacter>(ContextObject);
    if (!Player)
    {
        return false;
    }

    return Player->GetPlayerLevel() >= MinimumPlayerLevel;
}

FText UReq_PlayerLevel::GetRequirementDescription_Implementation() const
{
    return FText::Format(
        NSLOCTEXT("Upgrade", "PlayerLevelReq", "Player Level {0}"),
        FText::AsNumber(MinimumPlayerLevel)
    );
}
```

### Blueprint Implementation

#### 1. Create Blueprint Class

1. Right-click in Content Browser → Blueprint Class
2. Choose `UpgradeRequirement` as parent
3. Name it (e.g., `BP_Req_PlayerLevel`)

#### 2. Add Variables

- Add variable: `MinimumPlayerLevel` (Integer)
- Make it Instance Editable and Expose on Spawn

#### 3. Override IsRequirementMet

```
Event IsRequirementMet
├─ Cast to PlayerCharacter (ContextObject)
├─ Get Player Level
├─ Integer >= MinimumPlayerLevel
└─ Return Boolean
```

#### 4. Override GetRequirementDescription

```
Event GetRequirementDescription
├─ Format Text: "Player Level {0}"
│  └─ Param 0: MinimumPlayerLevel
└─ Return Text
```

---

## Creating Custom Actions

### C++ Implementation

#### 1. Create Header File

```cpp
// UA_SpawnActor.h
#pragma once

#include "CoreMinimal.h"
#include "Objects/UpgradeAction.h"
#include "UA_SpawnActor.generated.h"

UCLASS()
class PANGEABASEUPGRADESYSTEM_API UUA_SpawnActor : public UUpgradeAction
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Action")
    TSubclassOf<AActor> ActorClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Action")
    FVector SpawnLocation;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Action")
    FRotator SpawnRotation;

    virtual void Execute_Implementation(UObject* ContextObject) override;
};
```

#### 2. Implement Logic

```cpp
// UA_SpawnActor.cpp
#include "UA_SpawnActor.h"
#include "Actors/VillageBase.h"

void UUA_SpawnActor::Execute_Implementation(UObject* ContextObject)
{
    AVillageBase* Village = Cast<AVillageBase>(ContextObject);
    if (!Village || !ActorClass)
    {
        return;
    }

    UWorld* World = Village->GetWorld();
    if (!World)
    {
        return;
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = Village;
    
    FVector WorldLocation = Village->GetActorLocation() + SpawnLocation;
    
    AActor* SpawnedActor = World->SpawnActor<AActor>(
        ActorClass,
        WorldLocation,
        SpawnRotation,
        SpawnParams
    );

    if (SpawnedActor)
    {
        UE_LOG(LogTemp, Log, TEXT("Spawned actor: %s"), *SpawnedActor->GetName());
    }
}
```

### Blueprint Implementation

#### 1. Create Blueprint Class

1. Right-click in Content Browser → Blueprint Class
2. Choose `UpgradeAction` as parent
3. Name it (e.g., `BP_UA_SpawnActor`)

#### 2. Add Variables

- `ActorClass` (Actor Class Reference)
- `SpawnLocation` (Vector)
- `SpawnRotation` (Rotator)

#### 3. Override Execute

```
Event Execute
├─ Cast to VillageBase (ContextObject)
├─ Get Actor Location
├─ Add Vector (Location + SpawnLocation)
├─ Spawn Actor from Class
│  ├─ Class: ActorClass
│  ├─ Location: Calculated Location
│  ├─ Rotation: SpawnRotation
│  └─ Owner: VillageBase
└─ Print String (Success message)
```

---

## Advanced Patterns

### Composite Requirements

Create a requirement that checks multiple conditions:

```cpp
UCLASS()
class UReq_Composite : public UUpgradeRequirement
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, Instanced, Category="Requirement")
    TArray<UUpgradeRequirement*> SubRequirements;

    UPROPERTY(EditAnywhere, Category="Requirement")
    bool bRequireAll = true; // true = AND, false = OR

    virtual bool IsRequirementMet_Implementation(UObject* ContextObject) const override
    {
        if (SubRequirements.Num() == 0)
            return true;

        if (bRequireAll)
        {
            // AND logic
            for (UUpgradeRequirement* Req : SubRequirements)
            {
                if (!Req->IsRequirementMet(ContextObject))
                    return false;
            }
            return true;
        }
        else
        {
            // OR logic
            for (UUpgradeRequirement* Req : SubRequirements)
            {
                if (Req->IsRequirementMet(ContextObject))
                    return true;
            }
            return false;
        }
    }
};
```

### Async Actions

Actions that take time (e.g., playing an animation):

```cpp
UCLASS()
class UUA_PlayAnimation : public UUpgradeAction
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, Category="Action")
    UAnimMontage* MontageToPlay;

    virtual void Execute_Implementation(UObject* ContextObject) override
    {
        AVillageBase* Village = Cast<AVillageBase>(ContextObject);
        if (!Village || !MontageToPlay)
            return;

        // Find skeletal mesh component
        USkeletalMeshComponent* Mesh = Village->FindComponentByClass<USkeletalMeshComponent>();
        if (Mesh && Mesh->GetAnimInstance())
        {
            Mesh->GetAnimInstance()->Montage_Play(MontageToPlay);
        }
    }
};
```

### Event Broadcasting Actions

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
        // Broadcast to game-wide event system
        UGameInstance* GI = ContextObject->GetWorld()->GetGameInstance();
        if (UYourGameInstance* GameInstance = Cast<UYourGameInstance>(GI))
        {
            GameInstance->BroadcastGameplayEvent(EventTag);
        }
    }
};
```

---

## Testing Custom Types

### In Editor

1. Create a test `VillageDefinitionData`
2. Add your custom requirement/action to a milestone
3. Configure properties
4. Place `AVillageBase` in test map
5. Use console commands or debug functions to trigger level increase
6. Verify behavior

### Console Commands

Add debug console commands:

```cpp
// In your game mode or cheat manager
UFUNCTION(Exec)
void DebugTriggerVillageLevel(int32 Level)
{
    AVillageBase* Village = FindVillageInWorld();
    if (Village && Village->UpgradeSystem)
    {
        Village->UpgradeSystem->OnLevelIncreased(Level, GetPlayerPawn());
    }
}

UFUNCTION(Exec)
void DebugCheckRequirements()
{
    AVillageBase* Village = FindVillageInWorld();
    if (Village && Village->UpgradeSystem)
    {
        bool bCanUpgrade = Village->UpgradeSystem->CanUpgradeToNextLevel(GetPlayerPawn());
        UE_LOG(LogTemp, Log, TEXT("Can Upgrade: %s"), bCanUpgrade ? TEXT("Yes") : TEXT("No"));
    }
}
```

---

## Best Practices

### Requirements

- **Keep them simple**: Each requirement should check one thing
- **Provide good descriptions**: Players should understand what's needed
- **Handle edge cases**: What if ContextObject is null?
- **Cache expensive checks**: Don't recalculate every frame
- **Use const correctness**: Requirements shouldn't modify state

### Actions

- **Make them atomic**: Each action should do one thing
- **Handle failures gracefully**: Don't crash if something is missing
- **Log important events**: Help debugging
- **Consider undo**: Some actions might need reversal
- **Test in isolation**: Each action should work independently

### Performance

- **Avoid heavy operations in IsRequirementMet**: It may be called frequently
- **Cache data when possible**: Store references during BeginPlay
- **Use early exits**: Return as soon as you know the answer
- **Profile in shipping builds**: EditInlineNew objects can have overhead

---

## See Also

- [Components Reference](base-upgrade-components.md) - How requirements/actions are used
- [Data Assets](base-upgrade-data-assets.md) - Configuring requirements/actions
- [Integration Guide](base-upgrade-integration.md) - Connecting to game systems

