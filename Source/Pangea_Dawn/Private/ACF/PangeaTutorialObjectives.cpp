#include "ACF/PangeaTutorialObjectives.h"

#include "AbilitySystemGlobals.h"
#include "Components/ACFAbilitySystemComponent.h"
#include "Components/ACFCharacterMovementComponent.h"
#include "Components/ACFEquipmentComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

// ---------------------------------------------------------------------------
// UPangeaTutorialObjectiveBase
// ---------------------------------------------------------------------------

APawn* UPangeaTutorialObjectiveBase::GetTutorialPawn() const
{
    const APlayerController* PC = GetLocalPlayerController();
    return PC ? PC->GetPawn() : nullptr;
}

void UPangeaTutorialObjectiveBase::OnObjectiveStarted_Implementation()
{
    Super::OnObjectiveStarted_Implementation();

    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    bTutorialActive = true;
    LastPollTime = World->GetTimeSeconds();
    BeginWatching();
    World->GetTimerManager().SetTimer(PollTimer, this, &UPangeaTutorialObjectiveBase::HandlePoll, PollInterval, true);
}

void UPangeaTutorialObjectiveBase::OnObjectiveCompleted_Implementation()
{
    StopWatching();
    Super::OnObjectiveCompleted_Implementation();
}

void UPangeaTutorialObjectiveBase::OnObjectiveInterrupted_Implementation()
{
    StopWatching();
    Super::OnObjectiveInterrupted_Implementation();
}

void UPangeaTutorialObjectiveBase::HandlePoll()
{
    UWorld* World = GetWorld();
    if (!bTutorialActive || !World)
    {
        StopWatching();
        return;
    }

    const double Now = World->GetTimeSeconds();
    const float DeltaSeconds = static_cast<float>(Now - LastPollTime);
    LastPollTime = Now;
    PollTutorial(DeltaSeconds);
}

void UPangeaTutorialObjectiveBase::StopWatching()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(PollTimer);
    }
    if (bTutorialActive)
    {
        bTutorialActive = false;
        EndWatching();
    }
}

bool UPangeaTutorialObjectiveBase::IsPawnSprinting() const
{
    const APawn* Pawn = GetTutorialPawn();
    const UACFCharacterMovementComponent* Movement = Pawn ? Pawn->FindComponentByClass<UACFCharacterMovementComponent>() : nullptr;
    return Movement && Movement->IsSprinting();
}

bool UPangeaTutorialObjectiveBase::IsPawnCrouched() const
{
    const ACharacter* Character = Cast<ACharacter>(GetTutorialPawn());
    return Character && Character->bIsCrouched;
}

void UPangeaTutorialObjectiveBase::CompleteOnce()
{
    if (!bTutorialActive)
    {
        return;
    }
    // Stop polling first so a late poll cannot complete twice.
    StopWatching();
    CompleteObjective();
}

// ---------------------------------------------------------------------------
// UPangeaDistanceTravelledObjective
// ---------------------------------------------------------------------------

void UPangeaDistanceTravelledObjective::BeginWatching()
{
    DistanceTravelledMeters = 0.0f;
    bHasLastLocation = false;
}

void UPangeaDistanceTravelledObjective::PollTutorial(float DeltaSeconds)
{
    const APawn* Pawn = GetTutorialPawn();
    if (!Pawn)
    {
        bHasLastLocation = false;
        return;
    }

    const FVector Location = Pawn->GetActorLocation();
    if (!bHasLastLocation)
    {
        LastLocation = Location;
        bHasLastLocation = true;
        return;
    }

    const float StepMeters = static_cast<float>(FVector::Dist2D(Location, LastLocation)) / 100.0f;
    LastLocation = Location;

    bool bStateOk = true;
    switch (RequiredState)
    {
    case EPangeaTutorialMoveState::Sprinting:
        bStateOk = IsPawnSprinting();
        break;
    case EPangeaTutorialMoveState::Crouched:
        bStateOk = IsPawnCrouched();
        break;
    default:
        break;
    }

    // Ignore teleports and respawns: no single poll should move more than 20m.
    if (bStateOk && StepMeters < 20.0f)
    {
        DistanceTravelledMeters += StepMeters;
    }

    if (DistanceTravelledMeters >= RequiredDistanceMeters)
    {
        CompleteOnce();
    }
}

// ---------------------------------------------------------------------------
// UPangeaActionPerformedObjective
// ---------------------------------------------------------------------------

void UPangeaActionPerformedObjective::BeginWatching()
{
    LastSprintTime = -1.0;
    TryBindAbilitySystem();
}

void UPangeaActionPerformedObjective::EndWatching()
{
    if (UACFAbilitySystemComponent* ASC = BoundAbilitySystem.Get())
    {
        ASC->OnAbilityStartedEvent.RemoveDynamic(this, &UPangeaActionPerformedObjective::HandleActionStarted);
    }
    BoundAbilitySystem.Reset();
}

void UPangeaActionPerformedObjective::PollTutorial(float DeltaSeconds)
{
    // Rebind if the pawn changed (respawn, possession swap).
    TryBindAbilitySystem();

    if (bRequireSprinting && IsPawnSprinting())
    {
        if (const UWorld* World = GetWorld())
        {
            LastSprintTime = World->GetTimeSeconds();
        }
    }
}

void UPangeaActionPerformedObjective::TryBindAbilitySystem()
{
    UACFAbilitySystemComponent* ASC = Cast<UACFAbilitySystemComponent>(
        UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetTutorialPawn()));
    if (ASC == BoundAbilitySystem.Get())
    {
        return;
    }

    EndWatching();
    if (ASC)
    {
        ASC->OnAbilityStartedEvent.AddUniqueDynamic(this, &UPangeaActionPerformedObjective::HandleActionStarted);
        BoundAbilitySystem = ASC;
    }
}

void UPangeaActionPerformedObjective::HandleActionStarted(FGameplayTag ActionTag)
{
    if (!bTutorialActive || !ActionTag.MatchesAny(ActionTags))
    {
        return;
    }

    if (bRequireSprinting)
    {
        const UWorld* World = GetWorld();
        const bool bSprintingNow = IsPawnSprinting();
        const bool bSprintedRecently = World && LastSprintTime >= 0.0
            && (World->GetTimeSeconds() - LastSprintTime) <= SprintGraceSeconds;
        if (!bSprintingNow && !bSprintedRecently)
        {
            return;
        }
    }

    // Each match is one repetition; the quest manager completes the node at Repetitions.
    CompleteObjective();
}

// ---------------------------------------------------------------------------
// UPangeaWeaponWieldedObjective
// ---------------------------------------------------------------------------

void UPangeaWeaponWieldedObjective::PollTutorial(float DeltaSeconds)
{
    const APawn* Pawn = GetTutorialPawn();
    const UACFEquipmentComponent* Equipment = Pawn ? Pawn->FindComponentByClass<UACFEquipmentComponent>() : nullptr;
    if (Equipment && Equipment->GetCurrentMainWeapon())
    {
        CompleteOnce();
    }
}
