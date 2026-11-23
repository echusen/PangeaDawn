// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "ACFTopDownControllerComponent.h"
#include "ATSBaseTargetComponent.h"
#include "ATSTargetingComponent.h"
#include "Blueprint/AIBlueprintHelperLibrary.h"
#include "Interfaces/ACFInteractableInterface.h"
#include "NiagaraFunctionLibrary.h"
#include <Engine/World.h>
#include <GameFramework/Pawn.h>
#include <GameFramework/PlayerController.h>

UACFTopDownControllerComponent::UACFTopDownControllerComponent()
{
    // The tick is only needed while aiming at the cursor or while walking towards a clicked
    // interactable, so it is turned on on demand.
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;

    bEnabled = true;
    ShortPressThreshold = 0.3f;
    ClickMaxSteerFrames = 1;
    bHandleMouseCursor = true;
    CursorTraceChannel = ECollisionChannel::ECC_Visibility;

    bAimAtCursor = true;
    AimInterpSpeed = 0.f;

    bClickToTarget = true;
    bClearTargetOnEmptyClick = true;
    bDisableTargetingCameraLock = true;

    bClickToInteract = true;
    InteractAcceptanceRadius = 150.f;
    InteractMoveTimeout = 10.f;
    InteractionType = FString();
    // Resolved by path so this module does not depend on AscentCombatFramework: that module already
    // depends on this one, so a direct dependency would be circular.
    InteractionComponentClass = TSoftClassPtr<UActorComponent>(FSoftObjectPath(TEXT("/Script/AscentCombatFramework.ACFInteractionComponent")));

    FollowTime = 0.f;
    SteerFrames = 0;
    CachedDestination = FVector::ZeroVector;
    bHasCachedDestination = false;
    bHasPendingInteraction = false;
    PendingInteractGoal = FVector::ZeroVector;
    PendingInteractTime = 0.f;
}

// Called when the game starts
void UACFTopDownControllerComponent::BeginPlay()
{
    Super::BeginPlay();

    APlayerController* Controller = GetOwnerController();
    if (!Controller) {
        UE_LOG(LogTemp, Error, TEXT("TopDown Component needs to be added to a Player Controller! - UACFTopDownControllerComponent"));
        return;
    }

    if (!Controller->IsLocalController()) {
        return;
    }

    ApplyMouseCursor();
    ApplyTargetingCameraLock();
    RefreshTickState();
}

void UACFTopDownControllerComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    UpdateAimAtCursor(DeltaTime);
    UpdatePendingInteraction(DeltaTime);
}

void UACFTopDownControllerComponent::SetTopDownEnabled(bool bInEnabled)
{
    if (bEnabled == bInEnabled) {
        return;
    }

    bEnabled = bInEnabled;
    FollowTime = 0.f;
    SteerFrames = 0;
    bHasCachedDestination = false;

    ApplyMouseCursor();

    if (bEnabled) {
        ApplyTargetingCameraLock();
        RefreshTickState();
        return;
    }

    // Clearing the pending interaction refreshes the tick state on its way out.
    CancelPendingInteraction();
}

void UACFTopDownControllerComponent::SetClickToInteractEnabled(bool bInEnabled)
{
    if (bClickToInteract == bInEnabled) {
        return;
    }

    bClickToInteract = bInEnabled;

    if (bClickToInteract) {
        return;
    }

    CancelPendingInteraction();
}

void UACFTopDownControllerComponent::SetAimAtCursorEnabled(bool bInEnabled)
{
    if (bAimAtCursor == bInEnabled) {
        return;
    }

    bAimAtCursor = bInEnabled;

    RefreshTickState();
}

// Called by the Player Controller Blueprint from the Started event of the click Input Action
void UACFTopDownControllerComponent::SetDestinationStarted()
{
    if (!bEnabled) {
        return;
    }

    APlayerController* Controller = GetOwnerController();
    if (!Controller) {
        return;
    }

    // A new click cancels the navigation request of the previous one, otherwise the pawn would keep
    // walking to the old destination while this press is being evaluated. The pending interaction
    // goes with it: a new click always replaces the previous target.
    FollowTime = 0.f;
    SteerFrames = 0;
    bHasCachedDestination = false;
    ClearPendingInteraction();
    Controller->StopMovement();
}

// Called by the Player Controller Blueprint from the Triggered event of the click Input Action,
// which fires every frame while the input is held down
void UACFTopDownControllerComponent::SetDestinationTriggered()
{
    if (!bEnabled) {
        return;
    }

    APlayerController* Controller = GetOwnerController();
    if (!Controller) {
        return;
    }

    // We flag that the input is being pressed
    FollowTime += GetWorld()->GetDeltaSeconds();

    // We look for the location in the world where the player has pressed the input
    const bool bHasDestination = UpdateDestinationUnderCursor(Controller);
    if (!bHasDestination && !bHasCachedDestination) {
        return;
    }

    // Move towards mouse pointer or touch
    APawn* ControlledPawn = Controller->GetPawn();
    if (ControlledPawn != nullptr) {
        FVector WorldDirection = (CachedDestination - ControlledPawn->GetActorLocation()).GetSafeNormal();
        ControlledPawn->AddMovementInput(WorldDirection, 1.0, false);

        // Only a frame that really steered the pawn counts as holding: a release that arrives before
        // any steering happened is a click, however long that single frame lasted.
        SteerFrames++;
    }
}

// Called by the Player Controller Blueprint from both the Completed and the Canceled events of the
// click Input Action
void UACFTopDownControllerComponent::SetDestinationReleased()
{
    if (!bEnabled) {
        return;
    }

    APlayerController* Controller = GetOwnerController();
    if (!Controller) {
        return;
    }

    // A press short enough to never produce a Triggered event has no point of its own yet
    if (!bHasCachedDestination) {
        UpdateDestinationUnderCursor(Controller);
    }

    // If it was a short press
    if (WasShortPress() && bHasCachedDestination) {
        // A click on an interactable walks to the actor instead of the point under the cursor, so
        // the interaction can be fired on arrival.
        AActor* Interactable = nullptr;
        if (bClickToInteract) {
            Interactable = ResolveInteractable(HoveredActor.Get(), Controller->GetPawn());
        }

        if (Interactable) {
            StartPendingInteraction(Controller, Interactable);
        } else {
            // A click on an enemy picks it as the current target. It does not replace the movement:
            // the pawn still walks to the clicked point, exactly as it did before.
            if (bClickToTarget) {
                UpdateClickTarget();
            }

            // We move there and spawn some particles
            UAIBlueprintHelperLibrary::SimpleMoveToLocation(Controller, CachedDestination);
        }

        if (FXCursor) {
            UNiagaraFunctionLibrary::SpawnSystemAtLocation(Controller, FXCursor, CachedDestination, FRotator::ZeroRotator, FVector(1.f, 1.f, 1.f), true, true, ENCPoolMethod::None, true);
        }
    }

    FollowTime = 0.f;
    SteerFrames = 0;
}

bool UACFTopDownControllerComponent::WasShortPress() const
{
    // A press that never steered the pawn for more than ClickMaxSteerFrames frames is a click even
    // when the elapsed time went past the threshold: one single frame can be longer than the threshold
    // on its own, and in that case the time alone would read a one frame click as a hold and swallow
    // the movement order entirely.
    if (SteerFrames <= ClickMaxSteerFrames) {
        return true;
    }

    return FollowTime <= ShortPressThreshold;
}

bool UACFTopDownControllerComponent::MoveToInteractable(AActor* Interactable)
{
    if (!bEnabled) {
        return false;
    }

    APlayerController* Controller = GetOwnerController();
    if (!Controller) {
        return false;
    }

    AActor* Target = ResolveInteractable(Interactable, Controller->GetPawn());
    if (!Target) {
        return false;
    }

    StartPendingInteraction(Controller, Target);
    return true;
}

bool UACFTopDownControllerComponent::TargetActor(AActor* Target)
{
    UATSBaseTargetComponent* TargetComponent = FindTargetComponent();
    if (!TargetComponent) {
        return false;
    }

    AActor* Targetable = ResolveTargetable(Target, TargetComponent);
    if (!Targetable) {
        return false;
    }

    TargetComponent->SetCurrentTarget(Targetable);
    return true;
}

AActor* UACFTopDownControllerComponent::GetTargetableUnderCursor() const
{
    APlayerController* Controller = GetOwnerController();
    if (!Controller) {
        return nullptr;
    }

    FHitResult Hit;
    if (!TraceUnderCursor(Controller, Hit)) {
        return nullptr;
    }

    return ResolveTargetable(Hit.GetActor(), FindTargetComponent());
}

void UACFTopDownControllerComponent::UpdateClickTarget()
{
    UATSBaseTargetComponent* TargetComponent = FindTargetComponent();
    if (!TargetComponent) {
        return;
    }

    AActor* Targetable = ResolveTargetable(HoveredActor.Get(), TargetComponent);
    if (Targetable) {
        TargetComponent->SetCurrentTarget(Targetable);
        return;
    }

    if (bClearTargetOnEmptyClick && TargetComponent->GetCurrentTarget()) {
        TargetComponent->SetCurrentTarget(nullptr);
    }
}

void UACFTopDownControllerComponent::ApplyTargetingCameraLock()
{
    if (!bDisableTargetingCameraLock) {
        return;
    }

    UATSTargetingComponent* LockComponent = Cast<UATSTargetingComponent>(FindTargetComponent());
    if (!LockComponent) {
        return;
    }

    if (LockComponent->GetTargetingType() == ETargetingType::EDontLock) {
        return;
    }

    LockComponent->SetTargetingType(ETargetingType::EDontLock);
}

AActor* UACFTopDownControllerComponent::ResolveTargetable(AActor* HitActor, const UATSBaseTargetComponent* TargetComponent) const
{
    if (!IsValid(HitActor) || !TargetComponent) {
        return nullptr;
    }

    // Clicking something attached to an enemy (its weapon, its shield) has to target the enemy
    // itself, the same way an interactable is resolved.
    AActor* Candidate = HitActor;
    int32 Depth = 0;
    while (Candidate && Depth < 4) {
        if (TargetComponent->CanTargetActor(Candidate)) {
            return Candidate;
        }

        Candidate = Candidate->GetAttachParentActor();
        Depth++;
    }

    return nullptr;
}

UATSBaseTargetComponent* UACFTopDownControllerComponent::FindTargetComponent() const
{
    APlayerController* Controller = GetOwnerController();
    if (!Controller) {
        return nullptr;
    }

    // The targeting component usually lives on the controller, next to this one, but a pawn driven
    // setup is just as legitimate, so both are looked up before giving up.
    if (UATSBaseTargetComponent* ControllerComponent = Controller->FindComponentByClass<UATSBaseTargetComponent>()) {
        return ControllerComponent;
    }

    const APawn* ControlledPawn = Controller->GetPawn();
    if (!ControlledPawn) {
        return nullptr;
    }

    return ControlledPawn->FindComponentByClass<UATSBaseTargetComponent>();
}

AActor* UACFTopDownControllerComponent::GetPendingInteractable() const
{
    return PendingInteractable.Get();
}

void UACFTopDownControllerComponent::CancelPendingInteraction()
{
    ClearPendingInteraction();

    APlayerController* Controller = GetOwnerController();
    if (!Controller) {
        return;
    }

    Controller->StopMovement();
}

void UACFTopDownControllerComponent::StartPendingInteraction(APlayerController* Controller, AActor* Interactable)
{
    if (!Controller || !Interactable) {
        return;
    }

    PendingInteractable = Interactable;
    bHasPendingInteraction = true;
    PendingInteractGoal = Interactable->GetActorLocation();
    PendingInteractTime = 0.f;
    RefreshTickState();

    UAIBlueprintHelperLibrary::SimpleMoveToLocation(Controller, PendingInteractGoal);
}

void UACFTopDownControllerComponent::UpdatePendingInteraction(float DeltaTime)
{
    if (!bHasPendingInteraction) {
        return;
    }

    AActor* Interactable = PendingInteractable.Get();
    if (!Interactable) {
        ClearPendingInteraction();
        return;
    }

    if (!bEnabled || !bClickToInteract) {
        CancelPendingInteraction();
        return;
    }

    APlayerController* Controller = GetOwnerController();
    if (!Controller) {
        ClearPendingInteraction();
        return;
    }

    APawn* ControlledPawn = Controller->GetPawn();
    if (!ControlledPawn) {
        ClearPendingInteraction();
        return;
    }

    // The target may die, be looted or be disabled while the pawn is still walking: in that case the
    // pending interaction is dropped and the pawn simply stops where it is.
    if (!CanInteractWith(Interactable, ControlledPawn)) {
        CancelPendingInteraction();
        return;
    }

    if (InteractMoveTimeout > 0.f) {
        PendingInteractTime += DeltaTime;
        if (PendingInteractTime > InteractMoveTimeout) {
            CancelPendingInteraction();
            return;
        }
    }

    const FVector TargetLocation = Interactable->GetActorLocation();

    // Top down pawns walk on the ground, so the height difference with the target must not count
    // towards the acceptance radius.
    if (FVector::Dist2D(ControlledPawn->GetActorLocation(), TargetLocation) <= InteractAcceptanceRadius) {
        Controller->StopMovement();
        ClearPendingInteraction();

        TriggerInteraction(ControlledPawn, Interactable);
        OnInteractableReached.Broadcast(Interactable);
        return;
    }

    // A moving target (a wandering NPC) is followed by repathing, instead of walking to the spot
    // where it was when it got clicked.
    if (FVector::Dist2D(TargetLocation, PendingInteractGoal) > 50.f) {
        PendingInteractGoal = TargetLocation;
        UAIBlueprintHelperLibrary::SimpleMoveToLocation(Controller, PendingInteractGoal);
    }
}

void UACFTopDownControllerComponent::ClearPendingInteraction()
{
    PendingInteractable = nullptr;
    bHasPendingInteraction = false;
    PendingInteractGoal = FVector::ZeroVector;
    PendingInteractTime = 0.f;

    // The tick may still be needed by the cursor aiming, so it is not blindly turned off here.
    RefreshTickState();
}

void UACFTopDownControllerComponent::RefreshTickState()
{
    if (bHasPendingInteraction) {
        SetComponentTickEnabled(true);
        return;
    }

    if (!bEnabled || !bAimAtCursor) {
        SetComponentTickEnabled(false);
        return;
    }

    // Only the machine that owns the cursor aims with it: on the server and on the simulated proxies
    // there is no cursor to trace under.
    const APlayerController* Controller = GetOwnerController();
    if (!Controller) {
        SetComponentTickEnabled(false);
        return;
    }

    SetComponentTickEnabled(Controller->IsLocalController());
}

void UACFTopDownControllerComponent::UpdateAimAtCursor(float DeltaTime)
{
    if (!bEnabled || !bAimAtCursor) {
        return;
    }

    APlayerController* Controller = GetOwnerController();
    if (!Controller) {
        return;
    }

    if (!Controller->IsLocalController()) {
        return;
    }

    const APawn* ControlledPawn = Controller->GetPawn();
    if (!ControlledPawn) {
        return;
    }

    FVector AimPoint;
    if (!GetAimPointUnderCursor(AimPoint)) {
        return;
    }

    FVector EyeLocation;
    FRotator EyeRotation;
    ControlledPawn->GetActorEyesViewPoint(EyeLocation, EyeRotation);

    const FVector AimDirection = AimPoint - EyeLocation;
    if (AimDirection.IsNearlyZero()) {
        return;
    }

    const FRotator TargetRotation = AimDirection.Rotation();

    // The control rotation drives the aim, not the camera: in top down the camera has its own
    // absolute rotation and does not use the pawn control rotation.
    if (AimInterpSpeed <= 0.f) {
        Controller->SetControlRotation(TargetRotation);
        return;
    }

    const FRotator SmoothedRotation = FMath::RInterpTo(Controller->GetControlRotation(), TargetRotation, DeltaTime, AimInterpSpeed);
    Controller->SetControlRotation(SmoothedRotation);
}

AActor* UACFTopDownControllerComponent::ResolveInteractable(AActor* HitActor, APawn* InteractingPawn) const
{
    if (!IsValid(HitActor) || !InteractingPawn) {
        return nullptr;
    }

    // Clicking a mesh attached to an interactable (the wares on a vendor stand, a weapon on an NPC)
    // has to target the interactable actor itself.
    AActor* Candidate = HitActor;
    int32 Depth = 0;
    while (Candidate && Depth < 4) {
        if (Candidate == InteractingPawn) {
            return nullptr;
        }

        if (CanInteractWith(Candidate, InteractingPawn)) {
            return Candidate;
        }

        Candidate = Candidate->GetAttachParentActor();
        Depth++;
    }

    return nullptr;
}

bool UACFTopDownControllerComponent::CanInteractWith(AActor* Interactable, APawn* InteractingPawn) const
{
    if (!IsValid(Interactable) || !InteractingPawn) {
        return false;
    }

    if (!Interactable->GetClass()->ImplementsInterface(UACFInteractableInterface::StaticClass())) {
        return false;
    }

    return IACFInteractableInterface::Execute_CanBeInteracted(Interactable, InteractingPawn);
}

bool UACFTopDownControllerComponent::TriggerInteraction(APawn* InteractingPawn, AActor* Interactable)
{
    UActorComponent* InteractionComponent = FindInteractionComponent(InteractingPawn);
    if (!InteractionComponent) {
        return false;
    }

    // The interaction component picks its best interactable from its own detection sphere, which
    // knows nothing about the cursor, so the clicked actor has to be passed in explicitly. That is
    // what the interaction on behalf of a pawn is for: pushing the clicked actor in as the current
    // best interactable instead would fire the register and unregister events that drive the
    // interaction prompt, and would leave the component out of sync with its own sphere, which only
    // re-evaluates on overlap.
    // This mirrors the parameters of UACFInteractionComponent::ServerInteractOnBehalf, validated
    // against the reflection data below so a signature change fails loudly instead of corrupting the
    // stack.
    struct FInteractOnBehalfParams {
        FString InteractionType;
        AActor* BestInteractable = nullptr;
        APawn* InteractingPawn = nullptr;
    };

    UFunction* InteractFunction = InteractionComponent->FindFunction(FName(TEXT("ServerInteractOnBehalf")));
    if (!InteractFunction) {
        UE_LOG(LogTemp, Warning, TEXT("Interaction component without ServerInteractOnBehalf: click to interact does nothing - UACFTopDownControllerComponent"));
        return false;
    }

    if (InteractFunction->ParmsSize != sizeof(FInteractOnBehalfParams)) {
        UE_LOG(LogTemp, Warning, TEXT("Unexpected ServerInteractOnBehalf signature: click to interact does nothing - UACFTopDownControllerComponent"));
        return false;
    }

    FInteractOnBehalfParams InteractParams;
    InteractParams.InteractionType = InteractionType;
    InteractParams.BestInteractable = Interactable;
    InteractParams.InteractingPawn = InteractingPawn;

    InteractionComponent->ProcessEvent(InteractFunction, &InteractParams);

    return true;
}

UActorComponent* UACFTopDownControllerComponent::FindInteractionComponent(APawn* InteractingPawn) const
{
    if (!InteractingPawn) {
        return nullptr;
    }

    UClass* ComponentClass = InteractionComponentClass.LoadSynchronous();
    if (!ComponentClass) {
        return nullptr;
    }

    return InteractingPawn->FindComponentByClass(ComponentClass);
}

bool UACFTopDownControllerComponent::UpdateDestinationUnderCursor(APlayerController* Controller)
{
    FHitResult Hit;
    if (!TraceUnderCursor(Controller, Hit)) {
        HoveredActor = nullptr;
        return false;
    }

    CachedDestination = Hit.Location;
    bHasCachedDestination = true;
    HoveredActor = Hit.GetActor();
    return true;
}

bool UACFTopDownControllerComponent::TraceUnderCursor(APlayerController* Controller, FHitResult& OutHit) const
{
    if (!Controller) {
        return false;
    }

    return Controller->GetHitResultUnderCursor(CursorTraceChannel, true, OutHit);
}

bool UACFTopDownControllerComponent::GetAimPointUnderCursor(FVector& OutPoint) const
{
    OutPoint = FVector::ZeroVector;

    APlayerController* Controller = GetOwnerController();
    if (!Controller) {
        return false;
    }

    FHitResult Hit;
    if (!TraceUnderCursor(Controller, Hit)) {
        return false;
    }

    OutPoint = Hit.Location;
    return true;
}

void UACFTopDownControllerComponent::ApplyMouseCursor()
{
    if (!bHandleMouseCursor) {
        return;
    }

    APlayerController* Controller = GetOwnerController();
    if (!Controller) {
        return;
    }

    Controller->bShowMouseCursor = bEnabled;
}

APlayerController* UACFTopDownControllerComponent::GetOwnerController() const
{
    return Cast<APlayerController>(GetOwner());
}
