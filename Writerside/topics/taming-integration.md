# Taming System Integration

Quick integration guide for the Taming System.

## Basic Setup

### 1. Add Component to Dinosaurs

```cpp
UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
TObjectPtr<UPangeaTamingComponent> TamingComponent;

APDDinosaurBase::APDDinosaurBase()
{
    TamingComponent = CreateDefaultSubobject<UPangeaTamingComponent>(TEXT("Taming"));
}
```

### 2. Create Species Config

Create `DA_RaptorTameConfig` with:
- Requirements (items, stats)
- Behavior settings
- Role configurations
- AI controller classes

### 3. Assign and Initialize

```cpp
void APDDinosaurBase::BeginPlay()
{
    Super::BeginPlay();

    if (TamingComponent)
    {
        TamingComponent->TameSpeciesConfig = RaptorTameConfig;
        TamingComponent->InitializeWild(); // or InitializeHostile()
    }
}
```

## Player Interaction

### Taming Flow

```cpp
// Player approaches wild dinosaur and presses interact
void APlayerCharacter::OnInteractPressed()
{
    AActor* LookedAt = GetLookedAtActor();
    APDDinosaurBase* Dinosaur = Cast<APDDinosaurBase>(LookedAt);
    if (!Dinosaur || !Dinosaur->TamingComponent)
        return;

    // Check state
    if (Dinosaur->TamingComponent->GetTameState() != ETameState::Wild)
    {
        ShowMessage("Cannot tame this creature right now");
        return;
    }

    // Start taming
    Dinosaur->TamingComponent->StartTameAttempt(this);
}
```

### Minigame Widget Example

```cpp
UCLASS()
class UTamingMinigameWidget : public UUserWidget, public ITamingWidget
{
    GENERATED_BODY()

public:
    UPROPERTY()
    UPangeaTamingComponent* TamingComponent;

    float Duration;
    float TimeRemaining;
    int32 SuccessfulInputs;
    int32 RequiredInputs;

    void Initialize(UPangeaTamingComponent* InComponent, float InDuration)
    {
        TamingComponent = InComponent;
        Duration = InDuration;
        TimeRemaining = Duration;
        RequiredInputs = FMath::RandRange(5, 10);
        SuccessfulInputs = 0;

        GetWorld()->GetTimerManager().SetTimer(
            TickTimerHandle,
            this,
            &UTamingMinigameWidget::Tick,
            0.1f,
            true
        );
    }

    void Tick()
    {
        TimeRemaining -= 0.1f;

        if (TimeRemaining <= 0.0f)
        {
            // Time's up - check success
            bool bSuccess = SuccessfulInputs >= RequiredInputs;
            CompleteTaming(bSuccess);
        }

        UpdateProgressBar();
    }

    virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override
    {
        // Simple example: press space at right time
        if (InKeyEvent.GetKey() == EKeys::SpaceBar)
        {
            if (IsInSuccessWindow())
            {
                SuccessfulInputs++;
                PlaySuccessAnimation();
            }
            else
            {
                PlayFailureAnimation();
            }
        }

        return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
    }

    void CompleteTaming(bool bSuccess)
    {
        GetWorld()->GetTimerManager().ClearTimer(TickTimerHandle);

        // Let player choose role if successful
        if (bSuccess)
        {
            ShowRoleSelectionUI();
        }
        else
        {
            TamingComponent->OnTameResolved(false, ETamedRole::None);
            RemoveFromParent();
        }
    }

    void OnRoleSelected(ETamedRole Role)
    {
        TamingComponent->OnTameResolved(true, Role);
        RemoveFromParent();
    }
};
```

## Event Handling

```cpp
void APlayerController::OnDinosaurTamed(APDDinosaurBase* Dinosaur)
{
    if (!Dinosaur || !Dinosaur->TamingComponent)
        return;

    // Bind to state changes
    Dinosaur->TamingComponent->OnTameStateChanged.AddDynamic(
        this,
        &APlayerController::HandleTameStateChanged
    );

    // Bind to role changes
    Dinosaur->TamingComponent->OnTameRoleSelected.AddDynamic(
        this,
        &APlayerController::HandleRoleSelected
    );
}

void APlayerController::HandleTameStateChanged(ETameState NewState)
{
    switch (NewState)
    {
        case ETameState::Tamed:
            ShowAchievementNotification("First Tame!");
            IncrementTameCount();
            break;

        case ETameState::Hostile:
            ShowWarning("Dinosaur became hostile!");
            break;
    }
}

void APlayerController::HandleRoleSelected(ETamedRole Role)
{
    FString RoleStr = Role == ETamedRole::Mount ? "Mount" : "Companion";
    ShowNotification(FText::Format(INVTEXT("Role: {0}"), FText::FromString(RoleStr)));
}
```

## Save System

```cpp
// Properties auto-saved with SaveGame tag
UPROPERTY(SaveGame)
ETameState TamedState;

UPROPERTY(SaveGame)
ETamedRole TamedRole;

// On load, restore state
void APDDinosaurBase::OnLoaded_Implementation()
{
    Super::OnLoaded_Implementation();

    if (TamingComponent)
    {
        TamingComponent->HandleLoadedActor();
    }
}
```

## UI Examples

### Creature Status Widget

```cpp
void UCreatureStatusWidget::UpdateTamingInfo(UPangeaTamingComponent* TamingComp)
{
    if (!TamingComp)
        return;

    // State
    FText StateText;
    FLinearColor StateColor;

    switch (TamingComp->GetTameState())
    {
        case ETameState::Wild:
            StateText = INVTEXT("Wild");
            StateColor = FLinearColor::Yellow;
            break;
        case ETameState::Hostile:
            StateText = INVTEXT("Hostile");
            StateColor = FLinearColor::Red;
            break;
        case ETameState::Tamed:
            StateText = INVTEXT("Tamed");
            StateColor = FLinearColor::Green;
            break;
    }

    TameStateText->SetText(StateText);
    TameStateText->SetColorAndOpacity(StateColor);

    // Role (if tamed)
    if (TamingComp->GetTameState() == ETameState::Tamed)
    {
        FText RoleText = TamingComp->GetTamedRole() == ETamedRole::Mount ?
            INVTEXT("Mount") : INVTEXT("Companion");
        TameRoleText->SetText(RoleText);
        TameRoleText->SetVisibility(ESlateVisibility::Visible);
    }
    else
    {
        TameRoleText->SetVisibility(ESlateVisibility::Collapsed);
    }
}
```

## Console Commands

```cpp
UFUNCTION(Exec)
void DebugTameSelected(int32 RoleIndex)
{
    AActor* Selected = GetSelectedActor();
    APDDinosaurBase* Dinosaur = Cast<APDDinosaurBase>(Selected);
    if (!Dinosaur || !Dinosaur->TamingComponent)
        return;

    ETamedRole Role = RoleIndex == 0 ? ETamedRole::Mount : ETamedRole::Companion;
    Dinosaur->TamingComponent->OnTameResolved(true, Role);

    UE_LOG(LogTemp, Log, TEXT("Force tamed: %s as %s"),
        *Dinosaur->GetName(),
        *UEnum::GetValueAsString(Role)
    );
}

UFUNCTION(Exec)
void DebugMakeHostile()
{
    AActor* Selected = GetSelectedActor();
    APDDinosaurBase* Dinosaur = Cast<APDDinosaurBase>(Selected);
    if (Dinosaur && Dinosaur->TamingComponent)
    {
        Dinosaur->TamingComponent->InitializeHostile();
    }
}
```

## Best Practices

- Always validate tame state before initiating taming
- Provide clear UI feedback during minigame
- Handle failed tames gracefully (don't frustrate player)
- Make AI controller transitions smooth
- Test role switching thoroughly
- Balance stat requirements for progression
- Consider taming cooldowns to prevent spam

## See Also

- [Taming Component](taming-component.md)
- [Taming Config](taming-config.md)
- [States & Roles](taming-states-roles.md)

