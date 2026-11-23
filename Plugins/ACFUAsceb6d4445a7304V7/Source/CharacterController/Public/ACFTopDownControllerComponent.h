// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "Engine/HitResult.h"
#include "UObject/SoftObjectPtr.h"

#include "ACFTopDownControllerComponent.generated.h"

class AActor;
class APawn;
class APlayerController;
class UActorComponent;
class UATSBaseTargetComponent;
class UNiagaraSystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTopDownInteractableReached, AActor*, InteractableActor);

/**
 * Click to move controller for top down games. Must be added to a Player Controller.
 * A short press moves the pawn to the point under the cursor through the navmesh, while holding
 * the input steers the pawn towards the cursor directly.
 * Clicking an actor that implements IACFInteractableInterface walks the pawn to that actor instead
 * and fires the interaction on arrival, so dialogues, vendors and loot are reachable with the mouse
 * alone.
 * While enabled it also keeps the control rotation pointing at the cursor, so ranged attacks shoot
 * at the mouse instead of at the fixed camera direction.
 * Clicking an actor that can be targeted makes it the current target of the targeting component,
 * without ever touching the camera.
 * Input is not handled here: the Player Controller Blueprint owns the Input Action and calls
 * SetDestinationStarted / Triggered / Released from its Enhanced Input events.
 */
UCLASS(ClassGroup = (ACF), meta = (BlueprintSpawnableComponent))
class CHARACTERCONTROLLER_API UACFTopDownControllerComponent : public UActorComponent {
    GENERATED_BODY()

public:
    // Sets default values for this component's properties
    UACFTopDownControllerComponent();

    /** Call this from the Started event of the click Input Action in the Player Controller
     * Blueprint. */
    UFUNCTION(BlueprintCallable, Category = ACF)
    void SetDestinationStarted();

    /** Call this from the Triggered event of the click Input Action in the Player Controller
     * Blueprint: it fires every frame while the input is held and steers the pawn. */
    UFUNCTION(BlueprintCallable, Category = ACF)
    void SetDestinationTriggered();

    /** Call this from both the Completed and the Canceled events of the click Input Action in the
     * Player Controller Blueprint. */
    UFUNCTION(BlueprintCallable, Category = ACF)
    void SetDestinationReleased();

    /** Turns top down control on or off at runtime: handles mouse cursor and stops any movement in
     * progress. */
    UFUNCTION(BlueprintCallable, Category = ACF)
    void SetTopDownEnabled(bool bInEnabled);

    UFUNCTION(BlueprintPure, Category = ACF)
    bool IsTopDownEnabled() const
    {
        return bEnabled;
    }

    /** Last point under the cursor the pawn has been sent to. */
    UFUNCTION(BlueprintPure, Category = ACF)
    FVector GetCachedDestination() const
    {
        return CachedDestination;
    }

    /** Turns cursor aiming on or off at runtime. */
    UFUNCTION(BlueprintCallable, Category = ACF)
    void SetAimAtCursorEnabled(bool bInEnabled);

    UFUNCTION(BlueprintPure, Category = ACF)
    bool IsAimingAtCursor() const
    {
        return bAimAtCursor;
    }

    /** Point under the cursor right now, traced on CursorTraceChannel. Returns false when the cursor
     * is not over the world, leaving the output at zero. */
    UFUNCTION(BlueprintPure, Category = ACF)
    bool GetAimPointUnderCursor(FVector& OutPoint) const;

    /** Turns click to interact on or off at runtime. Turning it off drops any pending interaction,
     * leaving plain click to move behaviour. */
    UFUNCTION(BlueprintCallable, Category = ACF)
    void SetClickToInteractEnabled(bool bInEnabled);

    UFUNCTION(BlueprintPure, Category = ACF)
    bool IsClickToInteractEnabled() const
    {
        return bClickToInteract;
    }

    /** Walks the pawn to the given interactable and interacts with it on arrival. Returns false when
     * the actor is not a valid interactable for the possessed pawn. Cancels any previous pending
     * interaction. */
    UFUNCTION(BlueprintCallable, Category = ACF)
    bool MoveToInteractable(AActor* Interactable);

    /** Drops the pending interaction, if any, and stops the pawn. */
    UFUNCTION(BlueprintCallable, Category = ACF)
    void CancelPendingInteraction();

    /** Interactable the pawn is currently walking to, null when there is none. */
    UFUNCTION(BlueprintPure, Category = ACF)
    AActor* GetPendingInteractable() const;

    /** Makes the given actor the current target of the targeting component, when it can be targeted.
     * Returns false when there is no targeting component or the actor is not a valid target. It never
     * touches the camera: whether the lock frames the target is decided by the targeting type of the
     * targeting component. */
    UFUNCTION(BlueprintCallable, Category = ACF)
    bool TargetActor(AActor* Target);

    /** Actor under the cursor right now that could be targeted, null when there is none. Useful to
     * change the cursor or highlight an enemy on hover. */
    UFUNCTION(BlueprintPure, Category = ACF)
    AActor* GetTargetableUnderCursor() const;

    /** Fired when the pawn reaches the clicked interactable, after the interaction has been
     * triggered. Use it to react to the arrival or to interact yourself on pawns that have no ACF
     * interaction component. */
    UPROPERTY(BlueprintAssignable, Category = ACF)
    FOnTopDownInteractableReached OnInteractableReached;

protected:
    // Called when the game starts
    virtual void BeginPlay() override;

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    /** While false every call coming from the Blueprint is ignored and the cursor is not shown. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = ACF)
    bool bEnabled;

    /** Time Threshold to know if it was a short press */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = ACF)
    float ShortPressThreshold;

    /** Frames of actual steering a press is still treated as a single click at, whatever the elapsed
     * time says. A single frame can be longer than the threshold on its own (a hitch, a low framerate,
     * the editor ticking in background), and a press that never steered the pawn for longer than this
     * has to give its movement order instead of being swallowed as a hold. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = ACF, meta = (ClampMin = "0"))
    int32 ClickMaxSteerFrames;

    /** Shows the mouse cursor while top down control is enabled. Turn it off to handle the cursor
     * yourself. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = ACF)
    bool bHandleMouseCursor;

    /** Collision channel traced to find the destination under the cursor. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = ACF)
    TEnumAsByte<ECollisionChannel> CursorTraceChannel;

    /** FX Class that we will spawn when clicking */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = ACF)
    TObjectPtr<UNiagaraSystem> FXCursor;

    /** When true the control rotation of the owning controller is kept pointing at the point under
     * the cursor, so a ranged shot goes where the mouse is instead of where the pawn faces. It does
     * not move the camera: in top down the camera has its own absolute rotation. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = ACF)
    bool bAimAtCursor;

    /** Speed the control rotation is interpolated to the cursor with. Zero snaps it in a single
     * frame, which is what a mouse driven top down game usually wants. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = ACF, meta = (ClampMin = "0.0"))
    float AimInterpSpeed;

    /** When true, clicking an actor that implements the ACF interactable interface walks the pawn to
     * it and interacts on arrival instead of just moving to the clicked point. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = ACF)
    bool bClickToInteract;

    /** Distance from the interactable at which the pawn is considered arrived and the interaction is
     * fired. Keep it lower than the interaction area of the pawn's interaction component. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = ACF, meta = (ClampMin = "0.0"))
    float InteractAcceptanceRadius;

    /** Seconds after which a pending interaction is dropped when the pawn has not reached the
     * target, so a blocked path can never leave the pawn waiting forever. Zero disables it. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = ACF, meta = (ClampMin = "0.0"))
    float InteractMoveTimeout;

    /** When true, clicking an actor that can be targeted makes it the current target of the targeting
     * component, so attacks and aim assist follow the mouse. The camera is left alone: the lock
     * behaviour comes from the targeting type of the targeting component. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = ACF)
    bool bClickToTarget;

    /** When true, clicking the ground or anything that cannot be targeted drops the current target. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = ACF)
    bool bClearTargetOnEmptyClick;

    /** When true, the targeting component of the owner is asked for a targeting type that leaves the
     * camera alone as soon as top down control starts. A lock on would fight the fixed top down
     * camera, and it would also take the rotation of the pawn away from the click to move. Turn it
     * off to keep the targeting type the component was configured with. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = ACF)
    bool bDisableTargetingCameraLock;

    /** Interaction type string forwarded to the interaction, left empty for the default one. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = ACF)
    FString InteractionType;

    /** Component class looked up on the possessed pawn to fire the interaction. Defaults to the ACF
     * interaction component: it is resolved by class so this component works on non ACF pawns too,
     * and simply falls back to plain movement when the pawn has no such component. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = ACF)
    TSoftClassPtr<UActorComponent> InteractionComponentClass;

private:
    float FollowTime; // For how long it has been pressed

    /** Frames the current press has actually steered the pawn for. The elapsed time alone cannot tell
     * a click from a hold when a single frame is longer than the threshold. */
    int32 SteerFrames;

    FVector CachedDestination;

    /** True once the current press has traced a point under the cursor, so a release always walks to
     * the point of this click and never to the one left behind by the previous one. */
    bool bHasCachedDestination;

    /** Actor under the cursor on the last trace, used to know if the click landed on an interactable. */
    TWeakObjectPtr<AActor> HoveredActor;

    /** Interactable the pawn is walking to, if any. */
    TWeakObjectPtr<AActor> PendingInteractable;

    /** True while the pawn is walking to an interactable, so a target destroyed mid walk is still
     * cleaned up even though the tick is shared with cursor aiming. */
    bool bHasPendingInteraction;

    /** Location the pawn has last been pathed to while chasing the pending interactable. */
    FVector PendingInteractGoal;

    /** How long the pawn has been walking towards the pending interactable. */
    float PendingInteractTime;

    /** Traces under the cursor and caches the point. Returns false when the cursor is not over the
     * world, leaving the previous destination untouched. */
    bool UpdateDestinationUnderCursor(APlayerController* Controller);

    /** True when the press being released has to be treated as a single click instead of a hold. */
    bool WasShortPress() const;

    /** Single cursor trace shared by the destination and the aiming, so there is one channel and one
     * query for both. */
    bool TraceUnderCursor(APlayerController* Controller, FHitResult& OutHit) const;

    /** Points the control rotation at the cursor. Local controllers only: the control rotation
     * reaches the server through the movement replication, so the shot stays authoritative. */
    void UpdateAimAtCursor(float DeltaTime);

    /** Turns the tick on while either cursor aiming or a pending interaction needs it, and off when
     * neither does. */
    void RefreshTickState();

    void ApplyMouseCursor();

    class APlayerController* GetOwnerController() const;

    /** Walks up the attachment chain of the hit actor looking for an actor the pawn can interact
     * with, so clicking a mesh attached to a vendor still targets the vendor. */
    AActor* ResolveInteractable(AActor* HitActor, APawn* InteractingPawn) const;

    bool CanInteractWith(AActor* Interactable, APawn* InteractingPawn) const;

    /** Starts walking to the interactable and turns the tick on. */
    void StartPendingInteraction(APlayerController* Controller, AActor* Interactable);

    /** Checks arrival, target validity and timeout of the pending interaction. */
    void UpdatePendingInteraction(float DeltaTime);

    /** Resets the pending interaction state and turns the tick off, without touching the movement. */
    void ClearPendingInteraction();

    /** Fires the interaction through the interaction component of the possessed pawn. Returns false
     * when the pawn has no usable interaction component. */
    bool TriggerInteraction(APawn* InteractingPawn, AActor* Interactable);

    UActorComponent* FindInteractionComponent(APawn* InteractingPawn) const;

    /** Targets the actor under the cursor on the last trace, or drops the current target when the
     * click landed on something that cannot be targeted. */
    void UpdateClickTarget();

    /** Takes the camera away from the lock on of the targeting component, when there is one and the
     * owner asked for it. */
    void ApplyTargetingCameraLock();

    /** Walks up the attachment chain of the hit actor looking for an actor the targeting component
     * accepts, so clicking a weapon held by an enemy still targets the enemy. */
    AActor* ResolveTargetable(AActor* HitActor, const UATSBaseTargetComponent* TargetComponent) const;

    /** Targeting component to drive, looked up on the owning Player Controller first and on the
     * possessed pawn next. Null is a legitimate answer: a pawn with no targeting simply never gets a
     * target out of a click. */
    UATSBaseTargetComponent* FindTargetComponent() const;
};
